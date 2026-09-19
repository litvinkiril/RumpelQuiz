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

  // Serialize attempts for this account, including concurrent requests.
  const auto limits = transaction.Execute(
      "SELECT login_attempts >= 5 AND login_window_started_at > "
      "NOW() - INTERVAL '15 minutes' AS blocked "
      "FROM auth.users WHERE email = $1 FOR UPDATE", email);
  if (!limits.IsEmpty() && limits[0]["blocked"].As<bool>())
    return LoginError::kInvalidCredentials;

  const auto user = user_repository_.FindByEmail(transaction, email);

  if (!user.has_value() ||
      !password_hasher_.VerifyPassword(password, user->password_hash) ||
      !user->email_verified) {
    transaction.Execute(
        "UPDATE auth.users SET login_attempts = CASE WHEN "
        "login_window_started_at <= NOW() - INTERVAL '15 minutes' "
        "THEN 1 ELSE login_attempts + 1 END, "
        "login_window_started_at = CASE WHEN login_window_started_at <= "
        "NOW() - INTERVAL '15 minutes' THEN NOW() ELSE login_window_started_at END "
        "WHERE email = $1", email);
    transaction.Commit();
    return LoginError::kInvalidCredentials;
  }

  transaction.Execute("UPDATE auth.users SET login_attempts = 0 WHERE email = $1", email);
  transaction.Commit();
  return LoginSuccess{user->id};
}

}  // namespace RumpelQuiz
