#include "resend_code_service.hpp"

#include <chrono>
#include <string>

#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/storages/postgres/options.hpp>

namespace RumpelQuiz {
namespace {

constexpr auto kVerificationCodeLifetime = std::chrono::minutes{15};
constexpr auto kResendCooldown = std::chrono::minutes{1};

}  // namespace

ResendCodeService::ResendCodeService(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      pg_(context.FindComponent<userver::components::Postgres>("postgres-db-1")
              .GetCluster()),
      email_service_(context.FindComponent<EmailService>()) {}

ResendCodeResult ResendCodeService::Resend(
    const boost::uuids::uuid& verification_id) const {
  std::string email;
  std::string plain_code;
  boost::uuids::uuid new_verification_id;

  {
    auto transaction =
        pg_->Begin(userver::storages::postgres::ClusterHostType::kMaster,
                   userver::storages::postgres::TransactionOptions{});

    const auto verify_code =
        verification_repository_.FindCode(transaction, verification_id);
    if (!verify_code.has_value()) {
      return ResendCodeError::kVerificationNotFound;
    }

    const auto now = std::chrono::system_clock::now();
    if (now < verify_code->created_at.GetUnderlying() + kResendCooldown) {
      return ResendCodeError::kTooSoon;
    }

    const auto user =
        user_repository_.FindById(transaction, verify_code->user_id);
    if (!user.has_value() || user->email_verified) {
      return ResendCodeError::kVerificationNotFound;
    }

    const auto current_code = verification_code_service_.Generate();
    const userver::storages::postgres::TimePointTz expires_at{
        now + kVerificationCodeLifetime};

    new_verification_id = verification_repository_.UpsertCode(
        transaction, verify_code->user_id, current_code.hash, expires_at);
    email = user->email;
    plain_code = current_code.plain;

    transaction.Commit();
  }

  email_service_.SendVerificationCode(email, plain_code);

  return ResendCodeSuccess{new_verification_id};
}

}  // namespace RumpelQuiz
