#pragma once

#include <string>
#include <string_view>
#include <userver/server/handlers/http_handler_base.hpp>

namespace RumpelQuiz {
class FrontendHandler final
    : public userver::server::handlers::HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-frontend";
  using HttpHandlerBase::HttpHandlerBase;
  std::string HandleRequestThrow(
      const userver::server::http::HttpRequest&,
      userver::server::request::RequestContext&) const override;
};
}  // namespace RumpelQuiz
