#include "jwt_auth_checker.hpp"

#include <memory>
#include <string>
#include <string_view>

#include <userver/components/component_context.hpp>
#include <userver/http/common_headers.hpp>
#include <userver/server/handlers/auth/auth_checker_base.hpp>
#include <userver/server/handlers/exceptions.hpp>

#include "auth/middleware/jwt/auth_context.hpp"

namespace RumpelQuiz {
namespace {

class JwtAuthChecker final
    : public userver::server::handlers::auth::AuthCheckerBase {
 public:
  using AuthCheckResult = userver::server::handlers::auth::AuthCheckResult;

  explicit JwtAuthChecker(const JwtService& jwt_service)
      : jwt_service_(jwt_service) {}

  AuthCheckResult CheckAuth(
      const userver::server::http::HttpRequest& request,
      userver::server::request::RequestContext& context) const override {
    const auto& authorization =
        request.GetHeader(userver::http::headers::kAuthorization);
    constexpr std::string_view kBearerPrefix = "Bearer ";
    if (!std::string_view{authorization}.starts_with(kBearerPrefix) ||
        authorization.size() == kBearerPrefix.size()) {
      return {AuthCheckResult::Status::kTokenNotFound,
              {},
              "Authorization header must contain a Bearer token",
              userver::server::handlers::HandlerErrorCode::kUnauthorized};
    }

    try {
      const std::string_view token{authorization.data() + kBearerPrefix.size(),
                                   authorization.size() - kBearerPrefix.size()};
      context.SetData(std::string{kAuthenticatedUserId},
                      jwt_service_.VerifyAccessToken(token));
      return {};
    } catch (const JwtError&) {
      return {AuthCheckResult::Status::kTokenNotFound,
              {},
              "Invalid or expired access token",
              userver::server::handlers::HandlerErrorCode::kUnauthorized};
    }
  }

  bool SupportsUserAuth() const noexcept override { return true; }

 private:
  const JwtService& jwt_service_;
};

}  // namespace

JwtAuthCheckerFactory::JwtAuthCheckerFactory(
    const userver::components::ComponentContext& context)
    : jwt_service_(context.FindComponent<JwtServiceComponent>().GetService()) {}

userver::server::handlers::auth::AuthCheckerBasePtr
JwtAuthCheckerFactory::MakeAuthChecker(
    const userver::server::handlers::auth::HandlerAuthConfig&) const {
  return std::make_shared<JwtAuthChecker>(jwt_service_);
}

}  // namespace RumpelQuiz
