#include "password_reset_token_repository.hpp"

#include <string_view>
#include <userver/storages/postgres/io/uuid.hpp>

namespace RumpelQuiz {

namespace {

constexpr const char* kCreateToken = R"(
    INSERT INTO auth.password_reset_token (
        user_id,
        token_hash,
        expires_at
    )
    VALUES ($1, $2, $3)
    ON CONFLICT (user_id)
    DO UPDATE SET
        id = gen_random_uuid(), token_hash = EXCLUDED.token_hash,
        expires_at = EXCLUDED.expires_at,
        created_at = NOW()
    RETURNING id
)";

constexpr const char* kFindById = R"(
    SELECT
        id,
        user_id,
        token_hash,
        expires_at, created_at FROM auth.password_reset_token
    WHERE id = $1 FOR UPDATE
)";

constexpr const char* kDeleteToken = R"(
    DELETE FROM auth.password_reset_token
    WHERE id = $1
)";

}  // namespace

boost::uuids::uuid ResetTokenRepository::Create(
    userver::storages::postgres::Transaction& transaction,
    const boost::uuids::uuid& user_id, std::string_view token_hash,
    userver::storages::postgres::TimePointTz expires_at) const {
  const auto result =
      transaction.Execute(kCreateToken, user_id, token_hash, expires_at);

  return result.AsSingleRow<boost::uuids::uuid>();
}

std::optional<ResetTokenData> ResetTokenRepository::FindById(
    userver::storages::postgres::Transaction& transaction,
    const boost::uuids::uuid& token_id) const {
  const auto result = transaction.Execute(kFindById, token_id);

  if (result.IsEmpty()) {
    return std::nullopt;
  }

  return result.AsSingleRow<ResetTokenData>(
      userver::storages::postgres::kRowTag);
}

void ResetTokenRepository::Delete(
    userver::storages::postgres::Transaction& transaction,
    const boost::uuids::uuid& token_id) const {
  transaction.Execute(kDeleteToken, token_id);
}

}  // namespace RumpelQuiz
