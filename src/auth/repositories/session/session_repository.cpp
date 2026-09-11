#include "session_repository.hpp"
#include <userver/storages/postgres/io/uuid.hpp>

namespace RumpelQuiz {

boost::uuids::uuid SessionRepository::Create(
    userver::storages::postgres::Transaction& transaction,
    const boost::uuids::uuid& user_id, std::string_view refresh_token_hash,
    userver::storages::postgres::TimePointTz expires_at) const {
  const auto result = transaction.Execute(
      R"(
        INSERT INTO auth.sessions (
          user_id,
          refresh_token_hash,
          expires_at
        )
        VALUES ($1, $2, $3)
        RETURNING id
      )",
      user_id, refresh_token_hash, expires_at);

  return result.AsSingleRow<boost::uuids::uuid>();
}

std::optional<SessionData> SessionRepository::FindById(
    userver::storages::postgres::Transaction& transaction,
    const boost::uuids::uuid& session_id) const {
  const auto result = transaction.Execute(
      R"(
        SELECT
          id,
          user_id,
          refresh_token_hash,
          expires_at
        FROM auth.sessions
        WHERE id = $1
        FOR UPDATE
      )",
      session_id);

  if (result.IsEmpty()) {
    return std::nullopt;
  }

  return result.AsSingleRow<SessionData>(userver::storages::postgres::kRowTag);
}

void SessionRepository::UpdateRefreshToken(
    userver::storages::postgres::Transaction& transaction,
    const boost::uuids::uuid& session_id,
    std::string_view refresh_token_hash) const {
  transaction.Execute(
      R"(
        UPDATE auth.sessions
        SET
          refresh_token_hash = $2
        WHERE id = $1
      )",
      session_id, refresh_token_hash);
}

void SessionRepository::Delete(
    userver::storages::postgres::Transaction& transaction,
    const boost::uuids::uuid& session_id) const {
  transaction.Execute(
      R"(
        DELETE FROM auth.sessions
        WHERE id = $1
      )",
      session_id);
}

void SessionRepository::DeleteAllByUserId(
    userver::storages::postgres::Transaction& transaction,
    const boost::uuids::uuid& user_id) const {
  transaction.Execute(
      R"(
        DELETE FROM auth.sessions
        WHERE user_id = $1
      )",
      user_id);
}

}  // namespace RumpelQuiz