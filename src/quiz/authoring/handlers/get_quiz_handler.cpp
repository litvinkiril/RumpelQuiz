#include "get_quiz_handler.hpp"
#include <boost/uuid/string_generator.hpp>
#include <userver/components/component_context.hpp>
#include <userver/formats/json.hpp>
#include "auth/middleware/jwt/auth_context.hpp"
namespace RumpelQuiz {
GetQuizHandler::GetQuizHandler(const userver::components::ComponentConfig& c,
                               const userver::components::ComponentContext& x)
    : HttpHandlerJsonBase(c, x), service_(x.FindComponent<QuizService>()) {}
userver::formats::json::Value GetQuizHandler::HandleRequestJsonThrow(
    const userver::server::http::HttpRequest& req,
    const userver::formats::json::Value&,
    userver::server::request::RequestContext& ctx) const {
  req.GetHttpResponse().SetHeader(std::string_view{"Cache-Control"},
                                  "no-store");
  userver::formats::json::ValueBuilder out;
  const auto& user = GetAuthenticatedUserId(ctx);
  if (!req.HasPathArg("quiz_id")) {
    out["success"] = true;
    out["quizzes"] = service_.ListQuizzes(user);
    return out.ExtractValue();
  }
  boost::uuids::uuid id;
  try {
    id = boost::uuids::string_generator{}(req.GetPathArg("quiz_id"));
  } catch (const std::runtime_error&) {
    req.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest);
    out["success"] = false;
    out["error"] = "invalid_quiz_id";
    return out.ExtractValue();
  }
  auto quiz = service_.ReadQuiz(user, id);
  if (quiz.IsNull()) {
    req.SetResponseStatus(userver::server::http::HttpStatus::kNotFound);
    out["success"] = false;
    out["error"] = "quiz_not_found";
  } else {
    out["success"] = true;
    out["quiz"] = quiz;
  }
  return out.ExtractValue();
}
}  // namespace RumpelQuiz
