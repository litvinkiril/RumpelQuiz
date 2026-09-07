#include "verification_repository.hpp"

#include <string>
#include <string_view>
#include <userver/storages/postgres/io/uuid.hpp>

namespace RumpelQuiz {
boost::uuids::uuid VerificationRepository::UpsertCode(
    userver::storages::postgres::Transaction& transaction,
    const boost::uuids::uuid& user_id, std::string_view code_hash,
    userver::storages::postgres::TimePointTz expires_at) const {
  const auto result = transaction.Execute(
      R"(
            INSERT INTO auth.verification_codes (
                id,
                user_id,
                code_hash,
                expires_at
            )
            VALUES (
                gen_random_uuid(),
                $1,
                $2,
                $3
            )

            ON CONFLICT (user_id)
            DO UPDATE SET
                id = gen_random_uuid(),
                code_hash = EXCLUDED.code_hash,
                expires_at = EXCLUDED.expires_at,
                created_at = NOW()

            RETURNING id
        )",
      user_id, code_hash, expires_at);

  return result[0]["id"].As<boost::uuids::uuid>();
}

std::optional<CodeData> VerificationRepository::FindCode(
    userver::storages::postgres::Transaction& transaction,
    const boost::uuids::uuid& verification_id) const {
  const auto result = transaction.Execute(
      R"(
                SELECT id, user_id, code_hash, expires_at, created_at
                FROM auth.verification_codes
                WHERE id = $1
                FOR UPDATE
            )",
      verification_id);

  if (result.IsEmpty()) {
    return std::nullopt;
  }

  const auto row = result[0];
  return CodeData{
      row["id"].As<boost::uuids::uuid>(),
      row["user_id"].As<boost::uuids::uuid>(),
      row["code_hash"].As<std::string>(),
      row["expires_at"].As<userver::storages::postgres::TimePointTz>(),
      row["created_at"].As<userver::storages::postgres::TimePointTz>()};
}

void VerificationRepository::DeleteCode(
    userver::storages::postgres::Transaction& transaction,
    const boost::uuids::uuid& verification_id) const {
  transaction.Execute(
      R"(
            DELETE FROM auth.verification_codes WHERE id = $1
        )",
      verification_id);
}
}  // namespace RumpelQuiz
