#include "registration_service.hpp"

#include <chrono>
#include <cstdint>
#include <exception>
#include <stdexcept>
#include <string>
#include <string_view>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/logging/log.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/storages/postgres/options.hpp>
#include <userver/yaml_config/merge_schemas.hpp>
#include <userver/yaml_config/schema.hpp>

namespace RumpelQuiz {

RegistrationService::RegistrationService(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      pg_(context.FindComponent<userver::components::Postgres>("postgres-db-1")
              .GetCluster()),
      email_service_(context.FindComponent<EmailService>()),
      verification_code_lifetime_{std::chrono::seconds{
          config["verification-code-ttl-seconds"].As<std::int64_t>()}} {}

RegisterResult RegistrationService::Register(
    std::string_view email, std::string_view password,
    std::string_view password_confirmation) const {
  if (password != password_confirmation) {
    return RegisterError::kPasswordsDoNotMatch;
  }

  if (email.empty() || email.size() > 254 ||
      email.find('@') == std::string_view::npos ||
      email.find_first_of(" \t\r\n") != std::string_view::npos ||
      email.find('\0') != std::string_view::npos || password.empty() ||
      password.size() > 72 || password.find('\0') != std::string_view::npos) {
    return RegisterError::kInvalidRequest;
  }
  const std::string password_hash = password_hasher_.HashPassword(password);

  const VerificationCode verification_code =
      verification_code_service_.Generate();

  const userver::storages::postgres::TimePointTz expires_at{
      std::chrono::system_clock::now() + verification_code_lifetime_};

  boost::uuids::uuid user_id;
  boost::uuids::uuid verification_id;

  {
    auto transaction =
        pg_->Begin(userver::storages::postgres::ClusterHostType::kMaster,
                   userver::storages::postgres::TransactionOptions{});

    const auto existing_user = user_repository_.FindByEmail(transaction, email);

    if (!existing_user.has_value()) {
      user_id = user_repository_.CreateUser(transaction, email, password_hash);
    } else {
      if (existing_user->email_verified) {
        return RegisterError::kEmailAlreadyExists;
      }

      user_id = existing_user->id;

      user_repository_.UpdateUnverifiedUser(transaction, user_id,
                                            password_hash);
    }

    verification_id = verification_repository_.UpsertCode(
        transaction, user_id, verification_code.hash, expires_at);

    transaction.Commit();
  }

  try {
    email_service_.SendVerificationCode(email, verification_code.plain,
                                        verification_code_lifetime_);
  } catch (const std::exception&) {
    LOG_ERROR() << "Verification email delivery failed after "
                   "registration commit";
    throw std::runtime_error(
        "Verification email delivery failed after registration commit");
  }

  return RegistrationSuccess{verification_id};
}
}  // namespace RumpelQuiz

namespace RumpelQuiz {
userver::yaml_config::Schema RegistrationService::GetStaticConfigSchema() {
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
