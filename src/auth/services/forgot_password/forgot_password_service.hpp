#pragma once

#include <chrono>
#include <string_view>
#include <userver/components/component_base.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/yaml_config/schema.hpp>

#include "auth/models/forgot_password/forgot_password_results.hpp"
#include "auth/repositories/password_reset_codes/password_reset_repository.hpp"
#include "auth/repositories/password_reset_tokens/password_reset_token_repository.hpp"
#include "auth/repositories/user/user_repository.hpp"
#include "auth/services/password/password_hasher.hpp"
#include "auth/services/verification_code/verification_code_service.hpp"
#include "email/service/email_service.hpp"

namespace RumpelQuiz {
class ForgotPasswordService final : public userver::components::ComponentBase {
 public:
  static constexpr std::string_view kName = "forgot-password-service";
  ForgotPasswordService(const userver::components::ComponentConfig&,
                        const userver::components::ComponentContext&);
  static userver::yaml_config::Schema GetStaticConfigSchema();
  ForgotPasswordEmailResult EmailCheckSend(std::string_view email) const;
  ForgotPasswordVerifyResult PasswordResetCodeCheck(
      const boost::uuids::uuid& verification_id, std::string_view code) const;
  ForgotPasswordUpdateResult UpdatePassword(
      std::string_view reset_token, std::string_view password,
      std::string_view confirmation) const;

 private:
  userver::storages::postgres::ClusterPtr pg_;
  EmailService& email_service_;
  std::chrono::seconds code_lifetime_;
  std::chrono::seconds token_lifetime_;
  UserRepository user_repository_;
  PasswordResetCodeRepository code_repository_;
  ResetTokenRepository token_repository_;
  VerificationCodeService code_service_;
  PasswordHasher password_hasher_;
};
}  // namespace RumpelQuiz
