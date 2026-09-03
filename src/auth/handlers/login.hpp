#pragma once 

#include <userver/server/handlers/http_handler_json_base.hpp>

#include "auth/services/jwt_service.hpp"
#include "auth/services/login_service.hpp"

namespace RumpelQuiz {

class LoginHandler final
    : public userver::server::handlers::HttpHandlerJsonBase {
 public:
  static constexpr std::string_view kName = "handler-auth-login";

  LoginHandler(const userver::components::ComponentConfig& config,
                     const userver::components::ComponentContext& context);

  userver::formats::json::Value HandleRequestJsonThrow(
      const userver::server::http::HttpRequest& request,
      const userver::formats::json::Value& request_body,
      userver::server::request::RequestContext& context) const override;

 private:
  LoginService& login_service_;
  const JwtService& jwt_service_;
};

}