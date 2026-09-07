#include "resend_code_service.hpp"

#include <chrono>
#include <cstdint>
#include <exception>
#include <stdexcept>
#include <string>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/logging/log.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/storages/postgres/options.hpp>
#include <userver/yaml_config/merge_schemas.hpp>
#include <userver/yaml_config/schema.hpp>

namespace RumpelQuiz {
namespace {

constexpr auto kResendCooldown = std::chrono::minutes{1};

}  // namespace

ResendCodeService::ResendCodeService(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      pg_(context.FindComponent<userver::components::Postgres>("postgres-db-1")
              .GetCluster()),
      email_service_(context.FindComponent<EmailService>()),
      verification_code_lifetime_{std::chrono::seconds{
          config["verification-code-ttl-seconds"].As<std::int64_t>()}} {}

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

    const auto user =
        user_repository_.FindById(transaction, verify_code->user_id);
    if (!user.has_value() || user->email_verified) {
      return ResendCodeError::kVerificationNotFound;
    }

    const auto now = std::chrono::system_clock::now();
    if (now - kResendCooldown < verify_code->created_at) {
      return ResendCodeError::kTooSoon;
    }
    const auto current_code = verification_code_service_.Generate();
    const userver::storages::postgres::TimePointTz expires_at{
        now + verification_code_lifetime_};

    new_verification_id = verification_repository_.UpsertCode(
        transaction, verify_code->user_id, current_code.hash, expires_at);
    email = user->email;
    plain_code = current_code.plain;

    transaction.Commit();
  }

  try {
    email_service_.SendVerificationCode(email, plain_code,
                                        verification_code_lifetime_);
  } catch (const std::exception&) {
    LOG_ERROR() << "Verification email delivery failed after "
                   "resend commit";
    throw std::runtime_error(
        "Verification email delivery failed after resend commit");
  }

  return ResendCodeSuccess{new_verification_id};
}

}  // namespace RumpelQuiz

namespace RumpelQuiz {
userver::yaml_config::Schema ResendCodeService::GetStaticConfigSchema() {
  return userver::yaml_config::MergeSchemas<userver::components::ComponentBase>(
      R"(
type: object
description: Verification code settings.
additionalProperties: false
properties:
    verification-code-ttl-seconds:
        type: integer
        minimum: 1
        description: Code lifetime in seconds.
required:
  - verification-code-ttl-seconds
)");
}
}  // namespace RumpelQuiz
