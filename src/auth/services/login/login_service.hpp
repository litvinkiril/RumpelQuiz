#pragma once

#include <string_view>

#include <userver/components/component_base.hpp>
#include <userver/storages/postgres/cluster.hpp>

#include "auth/models/login/login_result.hpp"
#include "auth/repositories/user/user_repository.hpp"
#include "auth/services/password/password_hasher.hpp"

namespace RumpelQuiz {

class LoginService final
    : public userver::components::ComponentBase {
public:
    static constexpr std::string_view kName =
        "login-service";

    LoginService(
        const userver::components::ComponentConfig& config,
        const userver::components::ComponentContext& context
    );

    LoginResult LoginCheck(
        std::string_view email,
        std::string_view password
    ) const;

private:
    userver::storages::postgres::ClusterPtr pg_;

    PasswordHasher password_hasher_;
    UserRepository user_repository_;
};
}
