#pragma once

#include <string_view>

#include <userver/components/component_base.hpp>
#include <userver/storages/postgres/cluster.hpp>

#include "auth/models/registration/registration_result.hpp"
#include "auth/repositories/user/user_repository.hpp"
#include "auth/repositories/verification_code/verification_repository.hpp"
#include "auth/services/password/password_hasher.hpp"
#include "auth/services/verification_code/verification_code_service.hpp"
#include "email/service/email_service.hpp"

namespace RumpelQuiz {

class RegistrationService final
    : public userver::components::ComponentBase {
public:
    static constexpr std::string_view kName =
        "registration-service";

    RegistrationService(
        const userver::components::ComponentConfig& config,
        const userver::components::ComponentContext& context
    );

    RegisterResult Register(
        std::string_view email,
        std::string_view password,
        std::string_view password_confirmation
    ) const;

private:
    userver::storages::postgres::ClusterPtr pg_;
    EmailService& email_service_;

    UserRepository user_repository_;
    VerificationRepository verification_repository_;

    PasswordHasher password_hasher_;
    VerificationCodeService verification_code_service_;
};

}
