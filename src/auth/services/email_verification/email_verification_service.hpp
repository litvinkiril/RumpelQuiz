#pragma once

#include <string_view>
#include <userver/components/component_base.hpp>
#include <userver/storages/postgres/cluster.hpp>

#include "auth/models/email_verification/email_verification_result.hpp"
#include "auth/repositories/user/user_repository.hpp"
#include "auth/repositories/verification_code/verification_repository.hpp"
#include "auth/services/password/password_hasher.hpp"

namespace RumpelQuiz {
class EmailVerificationService final
    : public userver::components::ComponentBase {
 public:
  static constexpr std::string_view kName = "email-verify-service";

  EmailVerificationService(
      const userver::components::ComponentConfig& config,
      const userver::components::ComponentContext& context);

  EmailVerifyResult Verify(const boost::uuids::uuid& verification_id,
                           std::string_view code) const;

 private:
  userver::storages::postgres::ClusterPtr pg_;

  PasswordHasher password_hasher_;
  UserRepository user_repository_;
  VerificationRepository verification_repository_;
};
}  // namespace RumpelQuiz
