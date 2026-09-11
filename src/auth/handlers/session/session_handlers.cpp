#include "session_handlers.hpp"

#include <boost/uuid/string_generator.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <userver/components/component_context.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/server/http/http_status.hpp>
#include "auth/middleware/jwt/auth_context.hpp"

namespace RumpelQuiz {
namespace {
userver::formats::json::Value Error(
    const userver::server::http::HttpRequest& request,
    userver::server::http::HttpStatus status, std::string_view error) {
  request.SetResponseStatus(status);
  userver::formats::json::ValueBuilder response;
  response["success"] = false;
  response["error"] = error;
  return response.ExtractValue();
}
}  // namespace

RefreshHandler::RefreshHandler(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : HttpHandlerJsonBase(config, context),
      service_(context.FindComponent<AuthSessionService>()) {}

userver::formats::json::Value RefreshHandler::HandleRequestJsonThrow(
    const userver::server::http::HttpRequest& request,
    const userver::formats::json::Value& body,
    userver::server::request::RequestContext&) const {
  request.GetHttpResponse().SetHeader(std::string_view{"Cache-Control"},
                                      "no-store");
  boost::uuids::uuid session_id;
  std::string refresh_token;
  try {
    session_id =
        boost::uuids::string_generator{}(body["session_id"].As<std::string>());
    refresh_token = body["refresh_token"].As<std::string>();
  } catch (const std::exception&) {
    return Error(request, userver::server::http::HttpStatus::kBadRequest,
                 "invalid_request");
  }
  try {
    const auto tokens = service_.RefreshSession(session_id, refresh_token);
    userver::formats::json::ValueBuilder response;
    response["success"] = true;
    response["access_token"] = tokens.access_token;
    response["refresh_token"] = tokens.refresh_token;
    response["session_id"] = boost::uuids::to_string(tokens.session_id);
    response["token_type"] = "Bearer";
    return response.ExtractValue();
  } catch (const AuthSessionError&) {
    return Error(request, userver::server::http::HttpStatus::kUnauthorized,
                 "invalid_session");
  }
}

LogoutHandler::LogoutHandler(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : HttpHandlerJsonBase(config, context),
      service_(context.FindComponent<AuthSessionService>()) {}

userver::formats::json::Value LogoutHandler::HandleRequestJsonThrow(
    const userver::server::http::HttpRequest&,
    const userver::formats::json::Value&,
    userver::server::request::RequestContext& context) const {
  service_.RevokeSession(GetAuthenticatedSessionId(context));
  userver::formats::json::ValueBuilder response;
  response["success"] = true;
  return response.ExtractValue();
}
}  // namespace RumpelQuiz
