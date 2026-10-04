#include "list_quiz_sessions_handler.hpp"

#include <string_view>
#include <stdexcept>

#include <boost/uuid/string_generator.hpp>

#include <userver/components/component_context.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/server/http/http_status.hpp>

#include "auth/middleware/jwt/auth_context.hpp"

namespace RumpelQuiz {

ListQuizSessionsHandler::ListQuizSessionsHandler(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : HttpHandlerJsonBase(config, context),
      service_(context.FindComponent<QuizInfoService>()) {}

userver::formats::json::Value
ListQuizSessionsHandler::HandleRequestJsonThrow(
    const userver::server::http::HttpRequest& request,
    const userver::formats::json::Value&,
    userver::server::request::RequestContext& context) const {
  using userver::server::http::HttpStatus;

  request.GetHttpResponse().SetHeader(
      std::string_view{"Cache-Control"}, "no-store");

  userver::formats::json::ValueBuilder response;

  boost::uuids::uuid quiz_id;
  try {
    quiz_id = boost::uuids::string_generator{}(
        request.GetPathArg("quiz_id"));
    if (quiz_id.is_nil()) throw std::runtime_error("nil quiz id");
  } catch (const std::runtime_error&) {
    request.SetResponseStatus(HttpStatus::kBadRequest);
    response["success"] = false;
    response["error"] = "invalid_quiz_id";
    return response.ExtractValue();
  }

  const auto& user_id = GetAuthenticatedUserId(context);
  auto sessions = service_.ListQuizSessions(user_id, quiz_id);

  if (!sessions) {
    request.SetResponseStatus(HttpStatus::kNotFound);
    response["success"] = false;
    response["error"] = "quiz_not_found";
    return response.ExtractValue();
  }

  response["success"] = true;
  response["sessions"] = *sessions;
  return response.ExtractValue();
}

}  // namespace RumpelQuiz
