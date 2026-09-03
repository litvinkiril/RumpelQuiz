#include "verify_email.hpp"

#include <string>
#include <variant>

#include <boost/uuid/string_generator.hpp>

#include <userver/components/component_context.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/server/http/http_status.hpp>

namespace RumpelQuiz {

VerifyEmailHandler::VerifyEmailHandler(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : HttpHandlerJsonBase(config, context),
      email_verification_service_(
          context.FindComponent<EmailVerificationService>()),
      jwt_service_(context.FindComponent<JwtServiceComponent>().GetService()) {}

userver::formats::json::Value VerifyEmailHandler::HandleRequestJsonThrow(
    const userver::server::http::HttpRequest& request,
    const userver::formats::json::Value& request_body,
    userver::server::request::RequestContext&) const {
  const auto verification_id = boost::uuids::string_generator{}(
      request_body["verification_id"].As<std::string>());
  const auto code = request_body["code"].As<std::string>();

  const auto result = email_verification_service_.Verify(verification_id, code);

  userver::formats::json::ValueBuilder response;

  if (const auto* error = std::get_if<EmailVerifyError>(&result)) {
    request.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest);
    response["success"] = false;
    response["error"] = *error == EmailVerifyError::kCodeHasExpire
                            ? "code_expired"
                            : "codes_do_not_match";
    return response.ExtractValue();
  }

  const auto& success = std::get<EmailVerifySuccess>(result);
  response["success"] = true;
  response["access_token"] = jwt_service_.GenerateAccessToken(success.user_id);
  response["token_type"] = "Bearer";
  return response.ExtractValue();
}
}  // namespace RumpelQuiz
