#include "email_check.hpp"

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
ForgotPasswordEmailHandler::ForgotPasswordEmailHandler(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : HttpHandlerJsonBase(config, context),
      service_(context.FindComponent<ForgotPasswordService>()) {}
userver::formats::json::Value
ForgotPasswordEmailHandler::HandleRequestJsonThrow(
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
    const auto email = body["email"].As<std::string>();
    if (email.empty() || email.size() > 254 ||
        email.find('@') == std::string::npos)
      return fail("invalid_request");
    const auto result = service_.EmailCheckSend(email);
    if (const auto* success =
            std::get_if<ForgotPasswordEmailSuccess>(&result)) {
      response["success"] = true;
      response["verification_id"] =
          boost::uuids::to_string(success->verification_id);
    } else {
      switch (std::get<ForgotPasswordEmailError>(result)) {
        case ForgotPasswordEmailError::kEmailNotFound:
          return fail("email_not_found");
        case ForgotPasswordEmailError::kEmailNotVerified:
          return fail("email_not_verified");
        case ForgotPasswordEmailError::kTooSoon:
          return fail("resend_too_soon",
                      userver::server::http::HttpStatus::kTooManyRequests);
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
