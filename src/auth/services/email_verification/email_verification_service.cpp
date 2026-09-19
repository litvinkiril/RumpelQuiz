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

  const auto attempts = transaction.Execute(
      "SELECT attempts FROM auth.verification_codes WHERE id = $1",
      verification_id);
  if (attempts[0]["attempts"].As<int>() >= 5)
    return EmailVerifyError::kCodeDoesNotMatch;
  if (!password_hasher_.VerifyPassword(code, verify_code->code_hash)) {
    transaction.Execute(
        "UPDATE auth.verification_codes SET attempts = attempts + 1 WHERE id = $1",
        verification_id);
    transaction.Commit();
    return EmailVerifyError::kCodeDoesNotMatch;
  }

  user_repository_.VerifyUser(transaction, verify_code->user_id);

  verification_repository_.DeleteCode(transaction, verification_id);

  transaction.Commit();

  return EmailVerifySuccess{verify_code->user_id};
}

}  // namespace RumpelQuiz
