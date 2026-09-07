#include "email_verification_handler.hpp"

#include <boost/uuid/string_generator.hpp>
#include <exception>
#include <stdexcept>
#include <string>
#include <userver/components/component_context.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/server/http/http_status.hpp>
#include <variant>

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
  boost::uuids::uuid verification_id;
  std::string code;
  try {
    code = request_body["code"].As<std::string>();
    if (code.size() != 6 ||
        code.find_first_not_of("0123456789") != std::string::npos)
      throw std::invalid_argument("Invalid code");
    verification_id = boost::uuids::string_generator{}(
        request_body["verification_id"].As<std::string>());
  } catch (const std::exception&) {
    request.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest);
    userver::formats::json::ValueBuilder response;
    response["success"] = false;
    response["error"] = "invalid_or_expired_code";
    return response.ExtractValue();
  }

  const auto result = email_verification_service_.Verify(verification_id, code);

  userver::formats::json::ValueBuilder response;

  if (std::holds_alternative<EmailVerifyError>(result)) {
    request.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest);
    response["success"] = false;
    response["error"] = "invalid_or_expired_code";
    return response.ExtractValue();
  }

  const auto& success = std::get<EmailVerifySuccess>(result);
  response["success"] = true;
  response["access_token"] = jwt_service_.GenerateAccessToken(success.user_id);
  response["token_type"] = "Bearer";
  return response.ExtractValue();
}
}  // namespace RumpelQuiz
