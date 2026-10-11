#include "frontend_handler.hpp"

#include <string>
#include <string_view>
#include <userver/server/http/http_response.hpp>
#include <userver/server/http/http_status.hpp>

#include "frontend_assets.hpp"

namespace RumpelQuiz {
std::string FrontendHandler::HandleRequestThrow(
    const userver::server::http::HttpRequest& request,
    userver::server::request::RequestContext&) const {
  auto& response = request.GetHttpResponse();
  response.SetHeader(std::string_view{"Cache-Control"}, "no-store");
  response.SetHeader(std::string_view{"X-Content-Type-Options"}, "nosniff");
  response.SetHeader(std::string_view{"Referrer-Policy"}, "no-referrer");
  response.SetHeader(
      std::string_view{"Content-Security-Policy"},
      "default-src 'self'; script-src 'self'; style-src 'self'; "
      "img-src 'self' data: blob: https://storage.yandexcloud.net "
      "https://*.storage.yandexcloud.net; "
      "connect-src 'self'; frame-ancestors 'none'; base-uri "
      "'none'; form-action 'self'");
  const std::string_view request_path = request.GetRequestPath();
  const std::string_view path =
      request_path == "/" ? std::string_view{"/index.html"} : request_path;
  for (const auto& asset : FrontendAssets::kAssets) {
    if (asset.path == path) {
      response.SetContentType(std::string{asset.content_type});
      return std::string{asset.content};
    }
  }
  request.SetResponseStatus(userver::server::http::HttpStatus::kNotFound);
  return {};
}
}  // namespace RumpelQuiz
