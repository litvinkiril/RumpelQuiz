#include "game_session_service.hpp"
#include <boost/uuid/uuid_io.hpp>

#include <userver/formats/json.hpp>
#include <userver/formats/json/serialize.hpp>
#include <variant>
#include <unicode/uchar.h>
#include <unicode/utf8.h>
#include "s3client/s3client_component.hpp"

#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/storages/postgres/options.hpp>

namespace RumpelQuiz {
namespace pg = userver::storages::postgres;

GameSessionService::GameSessionService(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      pg_(context.FindComponent<userver::components::Postgres>("postgres-db-1")
              .GetCluster()),
      s3_(context.FindComponent<S3ClientComponent>().GetClient()),
      events_(context.FindComponent<GameEvents>()) {}

GameSessionResult GameSessionService::Create(
    const boost::uuids::uuid& user_id, const boost::uuids::uuid& quiz_id,
    const boost::uuids::uuid& session_id, const std::string& name) const {
  if (user_id.is_nil() || quiz_id.is_nil() || session_id.is_nil()) {
    return GameSessionFailure{GameSessionError::kInvalidRequest};
  }

  // Validate UTF-8 and trim Unicode whitespace before persisting the name.
  if (name.size() > 4096) return GameSessionFailure{GameSessionError::kInvalidRequest};
  int32_t offset = 0, first = -1, last = 0;
  int characters = 0, trimmed_characters = 0;
  while (offset < static_cast<int32_t>(name.size())) {
    const auto start = offset;
    UChar32 c;
    U8_NEXT(name.data(), offset, static_cast<int32_t>(name.size()), c);
    if (c < 0 || c == 0) return GameSessionFailure{GameSessionError::kInvalidRequest};
    if (!u_isUWhiteSpace(c) && c != 0xFEFF) {
      if (first < 0) first = start;
      last = offset;
      trimmed_characters = ++characters;
    } else if (first >= 0) {
      ++characters;
    }
  }
  if (first < 0 || trimmed_characters > 200)
    return GameSessionFailure{GameSessionError::kInvalidRequest};
  const auto normalized_name = name.substr(first, last - first);

  auto tx = pg_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});

  auto result = repository_.Create(tx, user_id, quiz_id, session_id, normalized_name);

  if (std::holds_alternative<GameSessionFailure>(result)) {
    // Транзакция откатится при уничтожении tx.
    return result;
  }

  tx.Commit();
  return result;
}

NextGameQuestionServiceResult GameSessionService::NextQuestion(
    const boost::uuids::uuid& user_id, const boost::uuids::uuid& session_id,
    const std::optional<boost::uuids::uuid>& expected_question_id) const {
  if (user_id.is_nil() || session_id.is_nil() ||
      (expected_question_id && expected_question_id->is_nil())) {
    return GameSessionFailure{GameSessionError::kInvalidRequest};
  }

  auto tx = pg_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});

  const auto result =
      repository_.NextQuestion(tx, user_id, session_id, expected_question_id);

  if (const auto* failure = std::get_if<GameSessionFailure>(&result)) {
    return *failure;
  }

  // Та же шкала времени БД, что и у opened_at/deadline_at.
  const auto server_time =
      tx.Execute("SELECT clock_timestamp()").AsSingleRow<pg::TimePointTz>();

  NextGameQuestionSuccess success{std::get<OpenedGameQuestion>(result),
                                  server_time};

  tx.Commit();
  try {
    const auto state = Read(user_id, session_id);
    userver::formats::json::ValueBuilder event;
    event["current_question"] = state["current_question"];
    event["server_time_ms"] = state["server_time_ms"];
    events_.Publish(session_id, GameEvents::Audience::kStudents, "question",
                    userver::formats::json::ToString(event.ExtractValue()));
  } catch (const std::exception&) {
    // The write succeeded; clients can recover from the authoritative state.
    events_.Publish(session_id, GameEvents::Audience::kAll, "resync", "{}");
  }
  return success;
}

GameSessionResult GameSessionService::Close(
    const boost::uuids::uuid& user_id,
    const boost::uuids::uuid& session_id) const {
  if (user_id.is_nil() || session_id.is_nil()) {
    return GameSessionFailure{GameSessionError::kInvalidRequest};
  }

  auto tx = pg_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});

  auto result = repository_.Close(tx, user_id, session_id);

  if (std::holds_alternative<GameSessionFailure>(result)) {
    return result;
  }

  tx.Commit();
  events_.Publish(session_id, GameEvents::Audience::kAll, "results", "{}");
  events_.presence.Clear(boost::uuids::to_string(session_id));
  return result;
}

userver::formats::json::Value GameSessionService::Read(
    const boost::uuids::uuid& user, const boost::uuids::uuid& session) const {
  auto tx = pg_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
  auto state = play_repository_.Read(tx, user, session);
  tx.Commit();
  userver::formats::json::ValueBuilder out(state);
  if (state["session"]["is_host"].As<bool>())
    out["presence"] = events_.ReadPresence(session);
  if (!state["current_question"].IsNull()) {
    const auto sign = [&](auto target, const auto& source) {
      auto key = source["image_key"].template As<std::optional<std::string>>();
      target.Remove("image_key");
      target["image_url"] = key ? s3_.DownloadUrl(*key) : std::string{};
    };
    sign(out["current_question"], state["current_question"]);
    for (std::size_t i = 0; i < state["current_question"]["answers"].GetSize();
         ++i)
      sign(out["current_question"]["answers"][i],
           state["current_question"]["answers"][i]);
  }
  return out.ExtractValue();
}
boost::uuids::uuid GameSessionService::Join(const boost::uuids::uuid& user,
                                            const std::string& code) const {
  auto tx = pg_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
  auto id = play_repository_.Join(tx, user, code);
  tx.Commit();
  events_.Publish(id, GameEvents::Audience::kHost, "resync", "{}");
  return id;
}
void GameSessionService::Submit(
    const boost::uuids::uuid& user, const boost::uuids::uuid& session,
    const boost::uuids::uuid& question,
    const std::vector<boost::uuids::uuid>& choices) const {
  auto tx = pg_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
  play_repository_.Submit(tx, user, session, question, choices);
  tx.Commit();
  events_.Publish(session, GameEvents::Audience::kHost, "resync", "{}");
}
void GameSessionService::UpdatePresence(
    const boost::uuids::uuid& user, const boost::uuids::uuid& session,
    const boost::uuids::uuid& client, std::int64_t sequence, bool online) const {
  auto tx = pg_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
  const auto allowed = tx.Execute(R"(
    SELECT s.status FROM game.sessions s
    JOIN game.participants p ON p.session_id=s.id AND p.user_id=$2
    WHERE s.id=$1 AND s.host_user_id<>$2 AND EXISTS(
      SELECT 1 FROM education.memberships m WHERE m.user_id=$2
      AND m.university_id=s.university_id AND m.role='student' AND m.status='active'
    ) FOR SHARE OF s
  )", session, user);
  if (allowed.IsEmpty()) throw GamePlayError("game_access_denied");
  const auto status = allowed.AsSingleRow<std::string>();
  if (status != "waiting" && status != "running") throw GamePlayError("session_closed");
  events_.presence.Update(boost::uuids::to_string(session), boost::uuids::to_string(user),
                          boost::uuids::to_string(client), sequence, online);
  tx.Commit();
}
}  // namespace RumpelQuiz
