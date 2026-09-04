#pragma once

#include <string_view>

#include <userver/components/component_base.hpp>
#include <userver/storages/postgres/cluster.hpp>

#include "auth/models/resend_code/resend_code_result.hpp"
#include "auth/repositories/user/user_repository.hpp"
#include "auth/repositories/verification_code/verification_repository.hpp"
#include "auth/services/verification_code/verification_code_service.hpp"
#include "email/service/email_service.hpp"

namespace RumpelQuiz {

class ResendCodeService final : public userver::components::ComponentBase {
 public:
  static constexpr std::string_view kName = "resend-code-service";

  ResendCodeService(const userver::components::ComponentConfig& config,
                    const userver::components::ComponentContext& context);

  ResendCodeResult Resend(const boost::uuids::uuid& verification_id) const;

 private:
  userver::storages::postgres::ClusterPtr pg_;
  EmailService& email_service_;
  UserRepository user_repository_;
  VerificationRepository verification_repository_;
  VerificationCodeService verification_code_service_;
};

}  // namespace RumpelQuiz
