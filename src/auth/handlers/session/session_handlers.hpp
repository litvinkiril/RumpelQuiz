#pragma once

#include <string_view>
#include <userver/server/handlers/http_handler_json_base.hpp>
#include "auth/services/auth_session/auth_session_service.hpp"

namespace RumpelQuiz {
class RefreshHandler final
    : public userver::server::handlers::HttpHandlerJsonBase {
 public:
  static constexpr std::string_view kName = "handler-auth-refresh";
  RefreshHandler(const userver::components::ComponentConfig& config,
                 const userver::components::ComponentContext& context);
  userver::formats::json::Value HandleRequestJsonThrow(
      const userver::server::http::HttpRequest& request,
      const userver::formats::json::Value& body,
      userver::server::request::RequestContext& context) const override;

 private:
  AuthSessionService& service_;
};

class LogoutHandler final
    : public userver::server::handlers::HttpHandlerJsonBase {
 public:
  static constexpr std::string_view kName = "handler-auth-logout";
  LogoutHandler(const userver::components::ComponentConfig& config,
                const userver::components::ComponentContext& context);
  userver::formats::json::Value HandleRequestJsonThrow(
      const userver::server::http::HttpRequest& request,
      const userver::formats::json::Value& body,
      userver::server::request::RequestContext& context) const override;

 private:
  AuthSessionService& service_;
};
}  // namespace RumpelQuiz
