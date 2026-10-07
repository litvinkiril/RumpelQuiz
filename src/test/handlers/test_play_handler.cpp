#include "test_play_handler.hpp"
#include <boost/uuid/string_generator.hpp>
#include <optional>
#include <userver/components/component_context.hpp>
#include <userver/formats/json.hpp>
#include "auth/middleware/jwt/auth_context.hpp"
namespace RumpelQuiz {
namespace {
boost::uuids::uuid Uuid(const std::string& text) {
  boost::uuids::uuid id;
  try {
    id = boost::uuids::string_generator{}(text);
  } catch (const std::runtime_error&) {
    throw TestPlayError("invalid_request");
  }
  if (id.is_nil()) throw TestPlayError("invalid_request");
  return id;
}
}  // namespace
TestPlayHandler::TestPlayHandler(const userver::components::ComponentConfig& c,
                                 const userver::components::ComponentContext& x)
    : HttpHandlerJsonBase(c, x), service_(x.FindComponent<TestService>()) {}
userver::formats::json::Value TestPlayHandler::HandleRequestJsonThrow(
    const userver::server::http::HttpRequest& req,
    const userver::formats::json::Value& body,
    userver::server::request::RequestContext& ctx) const {
  req.GetHttpResponse().SetHeader(std::string_view{"Cache-Control"},
                                  "no-store");
  try {
    std::optional<boost::uuids::uuid> test, question;
    std::vector<boost::uuids::uuid> choices;
    std::string operation = "available";
    if (req.HasPathArg("test_id")) {
      test = Uuid(req.GetPathArg("test_id"));
      const auto& path = req.GetRequestPath();
      operation = path.substr(path.find_last_of('/') + 1);
      if (operation == "answers") {
        if (!body.IsObject() || !body["question_id"].IsString() ||
            !body["answer_ids"].IsArray() || body["answer_ids"].GetSize() > 20)
          throw TestPlayError("invalid_request");
        question = Uuid(body["question_id"].As<std::string>());
        for (const auto& item : body["answer_ids"]) {
          if (!item.IsString()) throw TestPlayError("invalid_request");
          choices.push_back(Uuid(item.As<std::string>()));
        }
      }
    }
    return service_.Play(GetAuthenticatedUserId(ctx), test, operation, question,
                         choices);
  } catch (const TestPlayError& e) {
    const std::string code = e.what();
    using Status = userver::server::http::HttpStatus;
    auto status = Status::kConflict;
    if (code == "invalid_request") status = Status::kBadRequest;
    if (code == "test_not_found") status = Status::kNotFound;
    if (code == "test_access_denied") status = Status::kForbidden;
    req.SetResponseStatus(status);
    userver::formats::json::ValueBuilder out;
    out["success"] = false;
    out["error"] = code;
    return out.ExtractValue();
  }
}
}  // namespace RumpelQuiz
