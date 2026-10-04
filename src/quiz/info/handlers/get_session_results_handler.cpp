#include "get_session_results_handler.hpp"

#include <stdexcept>
#include <string_view>
#include <variant>

#include <boost/uuid/string_generator.hpp>

#include <userver/components/component_context.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/server/http/http_status.hpp>

#include "auth/middleware/jwt/auth_context.hpp"

namespace RumpelQuiz {

GetSessionResultsHandler::GetSessionResultsHandler(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : HttpHandlerJsonBase(config, context),
      service_(context.FindComponent<QuizInfoService>()) {}

userver::formats::json::Value
GetSessionResultsHandler::HandleRequestJsonThrow(
    const userver::server::http::HttpRequest& request,
    const userver::formats::json::Value&,
    userver::server::request::RequestContext& context) const {
  using userver::server::http::HttpStatus;

  request.GetHttpResponse().SetHeader(
      std::string_view{"Cache-Control"}, "no-store");

  userver::formats::json::ValueBuilder response;

  boost::uuids::uuid session_id;
  try {
    session_id = boost::uuids::string_generator{}(
        request.GetPathArg("session_id"));
    if (session_id.is_nil())
      throw std::runtime_error("nil session id");
  } catch (const std::runtime_error&) {
    request.SetResponseStatus(HttpStatus::kBadRequest);
    response["success"] = false;
    response["error"] = "invalid_session_id";
    return response.ExtractValue();
  }

  const auto& user_id = GetAuthenticatedUserId(context);
  auto result = service_.GetSessionResults(user_id, session_id);

  if (const auto* error =
          std::get_if<SessionResultsError>(&result)) {
    response["success"] = false;

    if (*error == SessionResultsError::kNotFound) {
      request.SetResponseStatus(HttpStatus::kNotFound);
      response["error"] = "session_not_found";
    } else {
      request.SetResponseStatus(HttpStatus::kConflict);
      response["error"] = "session_not_finished";
    }

    return response.ExtractValue();
  }

  response["success"] = true;
  response["session_id"] = request.GetPathArg("session_id");
  response["results"] =
      std::get<userver::formats::json::Value>(std::move(result));
  return response.ExtractValue();
}

}  // namespace RumpelQuiz