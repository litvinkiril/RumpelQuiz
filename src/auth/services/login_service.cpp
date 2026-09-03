#include "login_service.hpp"

#include <optional>

#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/storages/postgres/options.hpp>

namespace RumpelQuiz {
LoginService::LoginService(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      pg_(context.FindComponent<userver::components::Postgres>("postgres-db-1")
              .GetCluster()) {}

LoginResult LoginService::LoginCheck(
    std::string_view email, std::string_view password) const {
    std::optional<UserData> user;
    auto transaction =
        pg_->Begin(userver::storages::postgres::ClusterHostType::kMaster,
                    userver::storages::postgres::TransactionOptions{});

    user = user_repository_.FindByEmail(transaction, email);

    if (!user.has_value()) {
        return LoginError::kEmailDoesnotExist;
    }

    if (!password_hasher_.VerifyPassword(password, user->password_hash)) {
        return LoginError::kPasswordsDoNotMatch;
    }

    transaction.Commit();


    return LoginSuccess{user->id};
}

}  // namespace RumpelQuiz
