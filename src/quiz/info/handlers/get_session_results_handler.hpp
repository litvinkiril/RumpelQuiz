#pragma once

#include <string_view>

#include <userver/server/handlers/http_handler_json_base.hpp>

#include "quiz/info/service/quiz_info_service.hpp"

namespace RumpelQuiz {

class GetSessionResultsHandler final
    : public userver::server::handlers::HttpHandlerJsonBase {
 public:
  static constexpr std::string_view kName =
      "handler-get-session-results";

  GetSessionResultsHandler(
      const userver::components::ComponentConfig&,
      const userver::components::ComponentContext&);

 private:
  userver::formats::json::Value HandleRequestJsonThrow(
      const userver::server::http::HttpRequest&,
      const userver::formats::json::Value&,
      userver::server::request::RequestContext&) const override;

  QuizInfoService& service_;
};

}  // namespace RumpelQuiz