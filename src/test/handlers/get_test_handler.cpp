#include "get_test_handler.hpp"
#include <boost/uuid/string_generator.hpp>
#include <userver/components/component_context.hpp>
#include <userver/formats/json.hpp>
#include "auth/middleware/jwt/auth_context.hpp"
namespace RumpelQuiz {
GetTestHandler::GetTestHandler(const userver::components::ComponentConfig& c,
                               const userver::components::ComponentContext& x)
    : HttpHandlerJsonBase(c, x), service_(x.FindComponent<TestService>()) {}
userver::formats::json::Value GetTestHandler::HandleRequestJsonThrow(
    const userver::server::http::HttpRequest& req,
    const userver::formats::json::Value&,
    userver::server::request::RequestContext& ctx) const {
  req.GetHttpResponse().SetHeader(std::string_view{"Cache-Control"},
                                  "no-store");
  userver::formats::json::ValueBuilder out;
  const auto& user = GetAuthenticatedUserId(ctx);
  if (!req.HasPathArg("test_id")) {
    out["success"] = true;
    out["tests"] = service_.ListTests(user);
    return out.ExtractValue();
  }
  boost::uuids::uuid id;
  try {
    id = boost::uuids::string_generator{}(req.GetPathArg("test_id"));
    if (id.is_nil()) throw std::runtime_error("nil test id");
  } catch (const std::runtime_error&) {
    req.SetResponseStatus(userver::server::http::HttpStatus::kBadRequest);
    out["success"] = false;
    out["error"] = "invalid_test_id";
    return out.ExtractValue();
  }
  auto test = service_.ReadTest(user, id);
  if (test.IsNull()) {
    req.SetResponseStatus(userver::server::http::HttpStatus::kNotFound);
    out["success"] = false;
    out["error"] = "test_not_found";
  } else {
    out["success"] = true;
    out["test"] = test;
  }
  return out.ExtractValue();
}
}  // namespace RumpelQuiz
