#include "forgot_password_service.hpp"
#include "auth/repositories/session/session_repository.hpp"

#include <openssl/rand.h>
#include <array>
#include <boost/uuid/string_generator.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <chrono>
#include <cstdint>
#include <exception>
#include <stdexcept>
#include <string>
#include <string_view>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/storages/postgres/options.hpp>
#include <userver/yaml_config/merge_schemas.hpp>
#include <userver/yaml_config/schema.hpp>

namespace RumpelQuiz {
namespace {
std::string GenerateTokenSecret() {
  std::array<unsigned char, 32> bytes{};
  if (RAND_bytes(bytes.data(), bytes.size()) != 1)
    throw std::runtime_error("Failed to generate reset token");
  constexpr char hex[] = "0123456789abcdef";
  std::string result;
  for (const auto byte : bytes) {
    result += hex[byte >> 4];
    result += hex[byte & 15];
  }
  return result;
}
}  // namespace

ForgotPasswordService::ForgotPasswordService(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      pg_(context.FindComponent<userver::components::Postgres>("postgres-db-1")
              .GetCluster()),
      email_service_(context.FindComponent<EmailService>()),
      code_lifetime_(
          config["password-reset-code-ttl-seconds"].As<std::int64_t>()),
      token_lifetime_(
          config["password-reset-token-ttl-seconds"].As<std::int64_t>()) {}

ForgotPasswordEmailResult ForgotPasswordService::EmailCheckSend(
    std::string_view email) const {
  auto transaction =
      pg_->Begin(userver::storages::postgres::ClusterHostType::kMaster,
                 userver::storages::postgres::TransactionOptions{});
  transaction.Execute("SELECT id FROM auth.users WHERE email = $1 FOR UPDATE",
                      email);
  const auto user = user_repository_.FindByEmail(transaction, email);
  if (!user) return ForgotPasswordEmailError::kEmailNotFound;
  if (!user->email_verified) return ForgotPasswordEmailError::kEmailNotVerified;
  const auto recent = transaction.Execute(
      "SELECT id FROM auth.password_reset_codes WHERE user_id = $1 "
      "AND created_at > NOW() - INTERVAL '1 minute'",
      user->id);
  if (!recent.IsEmpty()) return ForgotPasswordEmailError::kTooSoon;
  const auto code = code_service_.Generate();
  const auto id = code_repository_.UpsertCode(
      transaction, user->id, code.hash,
      userver::storages::postgres::TimePointTz{
          std::chrono::system_clock::now() + code_lifetime_});
  transaction.Commit();
  email_service_.SendPasswordResetCode(email, code.plain, code_lifetime_);
  return ForgotPasswordEmailSuccess{id};
}

ForgotPasswordVerifyResult ForgotPasswordService::PasswordResetCodeCheck(
    const boost::uuids::uuid& verification_id,
    std::string_view plain_code) const {
  auto transaction =
      pg_->Begin(userver::storages::postgres::ClusterHostType::kMaster,
                 userver::storages::postgres::TransactionOptions{});
  const auto code = code_repository_.FindCode(transaction, verification_id);
  if (!code) return ForgotPasswordVerifyError::kVerificationNotFound;
  if (std::chrono::system_clock::now() >= code->expires_at)
    return ForgotPasswordVerifyError::kCodeExpired;
  const auto attempts = transaction.Execute(
      "SELECT attempts FROM auth.password_reset_codes WHERE id = $1",
      verification_id);
  if (attempts[0]["attempts"].As<int>() >= 5)
    return ForgotPasswordVerifyError::kInvalidCode;
  if (!password_hasher_.VerifyPassword(plain_code, code->code_hash)) {
    transaction.Execute(
        "UPDATE auth.password_reset_codes SET attempts = attempts + 1 WHERE id "
        "= $1",
        verification_id);
    transaction.Commit();
    return ForgotPasswordVerifyError::kInvalidCode;
  }
  const auto secret = GenerateTokenSecret();
  const auto id = token_repository_.Create(
      transaction, code->user_id, password_hasher_.HashPassword(secret),
      userver::storages::postgres::TimePointTz{
          std::chrono::system_clock::now() + token_lifetime_});
  code_repository_.DeleteCode(transaction, verification_id);
  transaction.Commit();
  return ForgotPasswordVerifySuccess{boost::uuids::to_string(id) + "." +
                                     secret};
}

ForgotPasswordUpdateResult ForgotPasswordService::UpdatePassword(
    std::string_view reset_token, std::string_view password,
    std::string_view confirmation) const {
  if (password != confirmation)
    return ForgotPasswordUpdateError::kPasswordsDoNotMatch;
  if (password.empty() || password.size() > 72 ||
      password.find('\0') != std::string_view::npos)
    return ForgotPasswordUpdateError::kInvalidPassword;
  boost::uuids::uuid id;
  if (reset_token.size() != 101 || reset_token[36] != '.')
    return ForgotPasswordUpdateError::kResetTokenNotFound;
  try {
    id = boost::uuids::string_generator{}(
        std::string{reset_token.substr(0, 36)});
  } catch (const std::exception&) {
    return ForgotPasswordUpdateError::kResetTokenNotFound;
  }
  auto transaction =
      pg_->Begin(userver::storages::postgres::ClusterHostType::kMaster,
                 userver::storages::postgres::TransactionOptions{});
  const auto token = token_repository_.FindById(transaction, id);
  if (!token) return ForgotPasswordUpdateError::kResetTokenNotFound;
  if (std::chrono::system_clock::now() >= token->expires_at)
    return ForgotPasswordUpdateError::kResetTokenExpired;
  if (!password_hasher_.VerifyPassword(reset_token.substr(37),
                                       token->token_hash))
    return ForgotPasswordUpdateError::kResetTokenNotFound;
  user_repository_.UpdatePassword(transaction, token->user_id,
                                  password_hasher_.HashPassword(password));
  SessionRepository{}.DeleteAllByUserId(transaction, token->user_id);
  transaction.Execute(
      "UPDATE auth.users SET login_attempts = 0 WHERE id = $1", token->user_id);
  token_repository_.Delete(transaction, id);
  transaction.Commit();
  return ForgotPasswordUpdateSuccess{token->user_id};
}

userver::yaml_config::Schema ForgotPasswordService::GetStaticConfigSchema() {
  return userver::yaml_config::MergeSchemas<userver::components::ComponentBase>(
      R"(
type: object
description: Password reset settings.
additionalProperties: false
properties:
    password-reset-code-ttl-seconds:
        type: integer
        minimum: 1
        description: Reset code lifetime in seconds.
    password-reset-token-ttl-seconds:
        type: integer
        minimum: 1
        description: Reset token lifetime in seconds.
required:
  - password-reset-code-ttl-seconds
  - password-reset-token-ttl-seconds
)");
}
}  // namespace RumpelQuiz
