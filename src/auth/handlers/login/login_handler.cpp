#include "login_handler.hpp"

#include <boost/uuid/uuid_io.hpp>
#include <string>
#include <userver/components/component_context.hpp>
#include <userver/formats/json/exception.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/server/http/http_status.hpp>
#include <variant>

namespace RumpelQuiz {

LoginHandler::LoginHandler(const userver::components::ComponentConfig& config,
                           const userver::components::ComponentContext& context)
    : HttpHandlerJsonBase(config, context),
      login_service_(context.FindComponent<LoginService>()),
      auth_session_service_(context.FindComponent<AuthSessionService>()) {}

userver::formats::json::Value LoginHandler::HandleRequestJsonThrow(
    const userver::server::http::HttpRequest& request,
    const userver::formats::json::Value& request_body,
    userver::server::request::RequestContext&) const {
  try {
    const auto email = request_body["email"].As<std::string>();
    const auto password = request_body["password"].As<std::string>();

    const auto result = login_service_.LoginCheck(email, password);

    userver::formats::json::ValueBuilder response;

    if (std::holds_alternative<LoginError>(result)) {
      request.SetResponseStatus(
          userver::server::http::HttpStatus::kUnauthorized);
      response["success"] = false;
      response["error"] = "invalid_credentials";
      return response.ExtractValue();
    }

    const auto& success = std::get<LoginSuccess>(result);
    response["success"] = true;
    const auto tokens = auth_session_service_.CreateSession(success.user_id);
    response["access_token"] = tokens.access_token;
    response["refresh_token"] = tokens.refresh_token;
    response["session_id"] = boost::uuids::to_string(tokens.session_id);
    request.GetHttpResponse().SetHeader(std::string_view{"Cache-Control"},
                                        "no-store");
    response["token_type"] = "Bearer";
    return response.ExtractValue();
  } catch (const userver::formats::json::Exception&) {
    request.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest);
    userver::formats::json::ValueBuilder response;
    response["success"] = false;
    response["error"] = "invalid_request";
    return response.ExtractValue();
  }
}
}  // namespace RumpelQuiz
