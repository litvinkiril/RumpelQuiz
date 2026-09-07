#include "verify_code.hpp"

#include <boost/uuid/string_generator.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <exception>
#include <stdexcept>
#include <string>
#include <string_view>
#include <userver/components/component_context.hpp>
#include <userver/formats/json/exception.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/server/http/http_status.hpp>
#include <variant>

namespace RumpelQuiz {
ForgotPasswordVerifyHandler::ForgotPasswordVerifyHandler(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : HttpHandlerJsonBase(config, context),
      service_(context.FindComponent<ForgotPasswordService>()) {}
userver::formats::json::Value
ForgotPasswordVerifyHandler::HandleRequestJsonThrow(
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
    boost::uuids::uuid id;
    try {
      id = boost::uuids::string_generator{}(
          body["verification_id"].As<std::string>());
    } catch (const std::exception&) {
      return fail("invalid_request");
    }
    const auto code = body["code"].As<std::string>();
    if (code.size() != 6 ||
        code.find_first_not_of("0123456789") != std::string::npos)
      return fail("invalid_or_expired_code");
    const auto result = service_.PasswordResetCodeCheck(id, code);
    if (const auto* success =
            std::get_if<ForgotPasswordVerifySuccess>(&result)) {
      response["success"] = true;
      response["reset_token"] = success->reset_token;
    } else {
      return fail("invalid_or_expired_code");
    }
  } catch (const userver::formats::json::Exception&) {
    return fail("invalid_request");
  } catch (const std::invalid_argument&) {
    return fail("invalid_request");
  }
  return response.ExtractValue();
}
}  // namespace RumpelQuiz
