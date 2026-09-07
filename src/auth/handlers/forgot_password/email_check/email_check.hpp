#pragma once

#include <string_view>
#include <userver/server/handlers/http_handler_json_base.hpp>

#include "auth/services/forgot_password/forgot_password_service.hpp"

namespace RumpelQuiz {
class ForgotPasswordEmailHandler final
    : public userver::server::handlers::HttpHandlerJsonBase {
 public:
  static constexpr std::string_view kName =
      "handler-forgot-password-email-check";
  ForgotPasswordEmailHandler(const userver::components::ComponentConfig&,
                             const userver::components::ComponentContext&);
  userver::formats::json::Value HandleRequestJsonThrow(
      const userver::server::http::HttpRequest&,
      const userver::formats::json::Value&,
      userver::server::request::RequestContext&) const override;

 private:
  ForgotPasswordService& service_;
};
}  // namespace RumpelQuiz
