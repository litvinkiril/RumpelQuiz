#pragma once

#include <string_view>

#include <userver/server/handlers/http_handler_json_base.hpp>

#include "auth/services/resend_code/resend_code_service.hpp"

namespace RumpelQuiz {

class ResendCodeHandler final
    : public userver::server::handlers::HttpHandlerJsonBase {
 public:
  static constexpr std::string_view kName = "handler-resend-code";

  ResendCodeHandler(const userver::components::ComponentConfig& config,
                    const userver::components::ComponentContext& context);

  userver::formats::json::Value HandleRequestJsonThrow(
      const userver::server::http::HttpRequest& request,
      const userver::formats::json::Value& request_body,
      userver::server::request::RequestContext& context) const override;

 private:
  ResendCodeService& resend_code_service_;
};

}  // namespace RumpelQuiz
