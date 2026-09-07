#include "email_verification_service.hpp"

#include <chrono>
#include <string_view>
#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/storages/postgres/options.hpp>

namespace RumpelQuiz {
EmailVerificationService::EmailVerificationService(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      pg_(context.FindComponent<userver::components::Postgres>("postgres-db-1")
              .GetCluster()) {}

EmailVerifyResult EmailVerificationService::Verify(
    const boost::uuids::uuid& verification_id, std::string_view code) const {
  auto transaction =
      pg_->Begin(userver::storages::postgres::ClusterHostType::kMaster,
                 userver::storages::postgres::TransactionOptions{});

  const auto verify_code =
      verification_repository_.FindCode(transaction, verification_id);
  if (!verify_code.has_value()) {
    return EmailVerifyError::kVerificationNotFound;
  }

  const auto now_time = std::chrono::system_clock::now();

  if (now_time >= verify_code->expires_at) {
    return EmailVerifyError::kCodeExpired;
  }

  if (!password_hasher_.VerifyPassword(code, verify_code->code_hash)) {
    return EmailVerifyError::kCodeDoesNotMatch;
  }

  user_repository_.VerifyUser(transaction, verify_code->user_id);

  verification_repository_.DeleteCode(transaction, verification_id);

  transaction.Commit();

  return EmailVerifySuccess{verify_code->user_id};
}

}  // namespace RumpelQuiz
