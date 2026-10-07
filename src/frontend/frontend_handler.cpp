#include "frontend_handler.hpp"

#include <string>
#include <string_view>
#include <userver/server/http/http_response.hpp>

#include "frontend_assets.hpp"

namespace RumpelQuiz {
std::string FrontendHandler::HandleRequestThrow(
    const userver::server::http::HttpRequest& request,
    userver::server::request::RequestContext&) const {
  auto& response = request.GetHttpResponse();
  response.SetHeader(std::string_view{"Cache-Control"}, "no-store");
  response.SetHeader(std::string_view{"X-Content-Type-Options"}, "nosniff");
  response.SetHeader(std::string_view{"Referrer-Policy"}, "no-referrer");
  response.SetHeader(std::string_view{"Content-Security-Policy"},
                     "default-src 'self'; script-src 'self'; style-src 'self'; "
                     "img-src 'self' data: blob: https://storage.yandexcloud.net https://*.storage.yandexcloud.net; "
                     "connect-src 'self'; frame-ancestors 'none'; base-uri "
                     "'none'; form-action 'self'");
  const auto& path = request.GetRequestPath();
  if (path == "/test.js") {
    response.SetContentType("text/javascript; charset=utf-8");
    return std::string{FrontendAssets::kTestJs};
  }
  if (path == "/game.js") {
    response.SetContentType("text/javascript; charset=utf-8");
    return std::string{FrontendAssets::kGameJs};
  }
  if (path == "/vendor/qrcode.js") {
    response.SetContentType("text/javascript; charset=utf-8");
    return std::string{FrontendAssets::kQrJs};
  }
  if (path == "/quiz.js") {
    response.SetContentType("text/javascript; charset=utf-8");
    return std::string{FrontendAssets::kQuizJs};
  }
  if (path == "/styles.css") {
    response.SetContentType("text/css; charset=utf-8");
    return std::string{FrontendAssets::kCss};
  }
  if (path == "/app.js") {
    response.SetContentType("text/javascript; charset=utf-8");
    return std::string{FrontendAssets::kJs};
  }
  response.SetContentType("text/html; charset=utf-8");
  return std::string{FrontendAssets::kHtml};
}
}  // namespace RumpelQuiz
