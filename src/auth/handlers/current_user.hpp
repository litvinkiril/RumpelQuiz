#pragma once

#include <userver/server/handlers/http_handler_json_base.hpp>

namespace RumpelQuiz {

class CurrentUserHandler final
    : public userver::server::handlers::HttpHandlerJsonBase {
 public:
  static constexpr std::string_view kName = "handler-auth-current-user";

  using HttpHandlerJsonBase::HttpHandlerJsonBase;

  userver::formats::json::Value HandleRequestJsonThrow(
      const userver::server::http::HttpRequest& request,
      const userver::formats::json::Value& request_body,
      userver::server::request::RequestContext& context) const override;
};

}  // namespace RumpelQuiz
