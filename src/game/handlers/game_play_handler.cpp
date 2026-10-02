#include "game_play_handler.hpp"
#include <algorithm>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include "auth/middleware/jwt/auth_context.hpp"
#include "game_handler_utils.hpp"
namespace RumpelQuiz {
GamePlayHandler::GamePlayHandler(const userver::components::ComponentConfig& c,
                                 const userver::components::ComponentContext& x)
    : HttpHandlerJsonBase(c, x),
      service_(x.FindComponent<GameSessionService>()),
      method_(c["method"].As<std::string>()) {}
userver::formats::json::Value GamePlayHandler::HandleRequestJsonThrow(
    const userver::server::http::HttpRequest& req,
    const userver::formats::json::Value& body,
    userver::server::request::RequestContext& ctx) const {
  GameHttp::NoStore(req);
  const auto& user = GetAuthenticatedUserId(ctx);
  try {
    if (!req.HasPathArg("session_id")) {
      if (!body.IsObject() || !body.HasMember("code") ||
          !body["code"].IsString())
        throw GamePlayError("invalid_request");
      const auto code = body["code"].As<std::string>();
      if (code.size() != 6 ||
          !std::all_of(code.begin(), code.end(),
                       [](char c) { return c >= '0' && c <= '9'; }))
        throw GamePlayError("invalid_request");
      GameHttp::Builder out;
      out["success"] = true;
      out["session_id"] = boost::uuids::to_string(service_.Join(user, code));
      return out.ExtractValue();
    }
    auto session = GameHttp::ParseUuid(req.GetPathArg("session_id"));
    if (!session) throw GamePlayError("invalid_request");
    if (method_ == "GET") return service_.Read(user, *session);
    auto question = GameHttp::ReadUuid(body, "question_id");
    if (!question || !body.HasMember("answer_ids") ||
        !body["answer_ids"].IsArray() || body["answer_ids"].GetSize() > 20)
      throw GamePlayError("invalid_request");
    std::vector<boost::uuids::uuid> choices;
    for (const auto& item : body["answer_ids"]) {
      if (!item.IsString()) throw GamePlayError("invalid_request");
      auto id = GameHttp::ParseUuid(item.As<std::string>());
      if (!id) throw GamePlayError("invalid_request");
      choices.push_back(*id);
    }
    service_.Submit(user, *session, *question, choices);
    GameHttp::Builder out;
    out["success"] = true;
    return out.ExtractValue();
  } catch (const GamePlayError& e) {
    const std::string code = e.what();
    auto status = GameHttp::Status::kConflict;
    if (code == "invalid_request")
      status = GameHttp::Status::kBadRequest;
    else if (code == "game_not_found")
      status = GameHttp::Status::kNotFound;
    else if (code == "game_access_denied" || code == "game_university_mismatch")
      status = GameHttp::Status::kForbidden;
    return GameHttp::Error(req, status, code);
  }
}
}  // namespace RumpelQuiz
