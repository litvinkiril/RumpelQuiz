#include "update_password.hpp"

#include <boost/uuid/string_generator.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <stdexcept>
#include <string>
#include <string_view>
#include <userver/components/component_context.hpp>
#include <userver/formats/json/exception.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/server/http/http_status.hpp>
#include <variant>

namespace RumpelQuiz {
ForgotPasswordUpdateHandler::ForgotPasswordUpdateHandler(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : HttpHandlerJsonBase(config, context),
      service_(context.FindComponent<ForgotPasswordService>()) {}
userver::formats::json::Value
ForgotPasswordUpdateHandler::HandleRequestJsonThrow(
    const userver::server::http::HttpRequest& request,
    const userver::formats::json::Value& body,
    userver::server::request::RequestContext&) const {
  userver::formats::json::ValueBuilder response;
  const auto fail = [&](std::string_view error,
                        userver::server::http::HttpStatus status =
                            userver::server::http::HttpStatus::kBadRequest) {
    request.SetResponseStatus(status);
    response["success"] = false;
    response["error"] = std::string{error};
    return response.ExtractValue();
  };
  try {
    const auto token = body["reset_token"].As<std::string>();
    const auto password = body["password"].As<std::string>();
    const auto confirmation = body["password_confirmation"].As<std::string>();
    const auto result = service_.UpdatePassword(token, password, confirmation);
    if (std::holds_alternative<ForgotPasswordUpdateSuccess>(result)) {
      response["success"] = true;
    } else {
      switch (std::get<ForgotPasswordUpdateError>(result)) {
        case ForgotPasswordUpdateError::kPasswordsDoNotMatch:
          return fail("passwords_do_not_match");
        case ForgotPasswordUpdateError::kInvalidPassword:
          return fail("invalid_password");
        default:
          return fail("invalid_or_expired_token");
      }
    }
  } catch (const userver::formats::json::Exception&) {
    return fail("invalid_request");
  } catch (const std::invalid_argument&) {
    return fail("invalid_request");
  }
  return response.ExtractValue();
}
}  // namespace RumpelQuiz
