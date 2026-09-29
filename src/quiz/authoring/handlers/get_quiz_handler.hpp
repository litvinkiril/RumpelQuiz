#pragma once
#include <userver/server/handlers/http_handler_json_base.hpp>
#include "quiz/authoring/service/quiz_service.hpp"
namespace RumpelQuiz {
class GetQuizHandler final
    : public userver::server::handlers::HttpHandlerJsonBase {
 public:
  static constexpr std::string_view kName = "handler-get-quiz";
  GetQuizHandler(const userver::components::ComponentConfig&,
                 const userver::components::ComponentContext&);

 private:
  userver::formats::json::Value HandleRequestJsonThrow(
      const userver::server::http::HttpRequest&,
      const userver::formats::json::Value&,
      userver::server::request::RequestContext&) const override;
  QuizService& service_;
};
}  // namespace RumpelQuiz
