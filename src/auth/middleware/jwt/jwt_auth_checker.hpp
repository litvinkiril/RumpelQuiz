#pragma once

#include <string_view>
#include <userver/server/handlers/auth/auth_checker_factory.hpp>

#include "auth/services/jwt/jwt_service.hpp"

namespace RumpelQuiz {

class JwtAuthCheckerFactory final
    : public userver::server::handlers::auth::AuthCheckerFactoryBase {
 public:
  static constexpr std::string_view kAuthType = "jwt";

  explicit JwtAuthCheckerFactory(
      const userver::components::ComponentContext& context);

  userver::server::handlers::auth::AuthCheckerBasePtr MakeAuthChecker(
      const userver::server::handlers::auth::HandlerAuthConfig& config)
      const override;

 private:
  const JwtService& jwt_service_;
};

}  // namespace RumpelQuiz
