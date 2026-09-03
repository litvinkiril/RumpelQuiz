#include "login.hpp"

#include <string>
#include <variant>

#include <userver/components/component_context.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/server/http/http_status.hpp>

namespace RumpelQuiz {

LoginHandler::LoginHandler(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : HttpHandlerJsonBase(config, context),
      login_service_(context.FindComponent<LoginService>()),
      jwt_service_(context.FindComponent<JwtServiceComponent>().GetService()) {}


userver::formats::json::Value LoginHandler::HandleRequestJsonThrow(
    const userver::server::http::HttpRequest& request,
    const userver::formats::json::Value& request_body,
    userver::server::request::RequestContext&) const {

    const auto email = request_body["email"].As<std::string>();
    const auto password = request_body["password"].As<std::string>();

    const auto result = login_service_.LoginCheck(email, password);

    userver::formats::json::ValueBuilder response;

    if (const auto* error = std::get_if<LoginError>(&result)) {
        request.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest);
        response["success"] = false;
        response["error"] = *error == LoginError::kEmailDoesnotExist
                                ? "email_does_not_exist"
                                : "passwords_do_not_match";
        return response.ExtractValue();
    }

    const auto& success = std::get<LoginSuccess>(result);
    response["success"] = true;
    response["access_token"] = jwt_service_.GenerateAccessToken(success.user_id);
    response["token_type"] = "Bearer";
    return response.ExtractValue();
}
}
