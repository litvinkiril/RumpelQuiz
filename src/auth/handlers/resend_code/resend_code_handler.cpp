#include "resend_code_handler.hpp"

#include <exception>
#include <string>
#include <variant>

#include <boost/uuid/string_generator.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <userver/components/component_context.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/server/http/http_status.hpp>

namespace RumpelQuiz {

ResendCodeHandler::ResendCodeHandler(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : HttpHandlerJsonBase(config, context),
      resend_code_service_(context.FindComponent<ResendCodeService>()) {}

userver::formats::json::Value ResendCodeHandler::HandleRequestJsonThrow(
    const userver::server::http::HttpRequest& request,
    const userver::formats::json::Value& request_body,
    userver::server::request::RequestContext&) const {
  boost::uuids::uuid verification_id;
  try {
    verification_id = boost::uuids::string_generator{}(
        request_body["verification_id"].As<std::string>());
  } catch (const std::exception&) {
    request.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest);
    userver::formats::json::ValueBuilder response;
    response["success"] = false;
    response["error"] = "invalid_verification_id";
    return response.ExtractValue();
  }

  const auto result = resend_code_service_.Resend(verification_id);

  userver::formats::json::ValueBuilder response;
  if (const auto* error = std::get_if<ResendCodeError>(&result)) {
    const bool too_soon = *error == ResendCodeError::kTooSoon;
    request.SetResponseStatus(
        too_soon ? userver::server::http::HttpStatus::kTooManyRequests
                 : userver::server::http::HttpStatus::kNotFound);
    response["success"] = false;
    response["error"] = too_soon ? "resend_too_soon" : "verification_not_found";
    return response.ExtractValue();
  }

  const auto& success = std::get<ResendCodeSuccess>(result);
  response["success"] = true;
  response["verification_id"] =
      boost::uuids::to_string(success.verification_id);
  return response.ExtractValue();
}

}  // namespace RumpelQuiz
