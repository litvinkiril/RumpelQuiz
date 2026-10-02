#pragma once
#include <string>
#include <userver/server/handlers/http_handler_json_base.hpp>
#include "game/service/game_session_service.hpp"
namespace RumpelQuiz {
class GamePlayHandler final
    : public userver::server::handlers::HttpHandlerJsonBase {
 public:
  static constexpr std::string_view kName = "handler-game-state";
  GamePlayHandler(const userver::components::ComponentConfig&,
                  const userver::components::ComponentContext&);

 private:
  userver::formats::json::Value HandleRequestJsonThrow(
      const userver::server::http::HttpRequest&,
      const userver::formats::json::Value&,
      userver::server::request::RequestContext&) const override;
  GameSessionService& service_;
  std::string method_;
};
}  // namespace RumpelQuiz
