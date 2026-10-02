#include "game_session_service.hpp"

#include <userver/formats/json.hpp>
#include <variant>
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
      s3_(context.FindComponent<S3ClientComponent>().GetClient()) {}

GameSessionResult GameSessionService::Create(
    const boost::uuids::uuid& user_id, const boost::uuids::uuid& quiz_id,
    const boost::uuids::uuid& session_id) const {
  if (user_id.is_nil() || quiz_id.is_nil() || session_id.is_nil()) {
    return GameSessionFailure{GameSessionError::kInvalidRequest};
  }

  auto tx = pg_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});

  auto result = repository_.Create(tx, user_id, quiz_id, session_id);

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
  return result;
}

userver::formats::json::Value GameSessionService::Read(
    const boost::uuids::uuid& user, const boost::uuids::uuid& session) const {
  auto tx = pg_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
  auto state = play_repository_.Read(tx, user, session);
  tx.Commit();
  userver::formats::json::ValueBuilder out(state);
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
  return id;
}
void GameSessionService::Submit(
    const boost::uuids::uuid& user, const boost::uuids::uuid& session,
    const boost::uuids::uuid& question,
    const std::vector<boost::uuids::uuid>& choices) const {
  auto tx = pg_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
  play_repository_.Submit(tx, user, session, question, choices);
  tx.Commit();
}
}  // namespace RumpelQuiz
