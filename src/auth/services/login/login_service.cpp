#include "login_service.hpp"

#include <string_view>
#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/storages/postgres/options.hpp>

namespace RumpelQuiz {
LoginService::LoginService(const userver::components::ComponentConfig& config,
                           const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      pg_(context.FindComponent<userver::components::Postgres>("postgres-db-1")
              .GetCluster()) {}

LoginResult LoginService::LoginCheck(std::string_view email,
                                     std::string_view password) const {
  if (password.empty() || password.size() > 72 ||
      password.find('\0') != std::string_view::npos) {
    return LoginError::kInvalidCredentials;
  }
  auto transaction =
      pg_->Begin(userver::storages::postgres::ClusterHostType::kMaster,
                 userver::storages::postgres::TransactionOptions{});

  const auto user = user_repository_.FindByEmail(transaction, email);

  if (!user.has_value() ||
      !password_hasher_.VerifyPassword(password, user->password_hash) ||
      !user->email_verified) {
    return LoginError::kInvalidCredentials;
  }

  transaction.Commit();
  return LoginSuccess{user->id};
}

}  // namespace RumpelQuiz
