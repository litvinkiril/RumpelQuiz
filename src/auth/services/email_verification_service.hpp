#pragma once

#include <string_view>

#include <userver/components/component_base.hpp>
#include <userver/storages/postgres/cluster.hpp>

#include "auth/models/email_verify_result.hpp"
#include "auth/services/password_hasher.hpp"
#include "auth/repositories/user_repository.hpp"
#include "auth/repositories/verification_repository.hpp"
#include "auth/services/verification_code_service.hpp"
#include "email/email_service.hpp"

namespace RumpelQuiz {
    class EmailVerificationService final
    : public userver::components::ComponentBase {
public:
    static constexpr std::string_view kName =
        "email-verify-service";

    EmailVerificationService(
        const userver::components::ComponentConfig& config,
        const userver::components::ComponentContext& context
    );

    EmailVerifyResult Verify(
        const boost::uuids::uuid& verification_id,
        std::string_view code
    ) const;

private:
    userver::storages::postgres::ClusterPtr pg_;

    PasswordHasher password_hasher_;
    UserRepository user_repository_;
    VerificationRepository verification_repository_;
};
}
