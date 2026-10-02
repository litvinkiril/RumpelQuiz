#pragma once

#include <string_view>

#include <userver/server/handlers/http_handler_json_base.hpp>

#include "game/service/game_session_service.hpp"

namespace RumpelQuiz {

class CreateGameSessionHandler final
    : public userver::server::handlers::HttpHandlerJsonBase {
 public:
    static constexpr std::string_view kName =
        "handler-create-game-session";

    CreateGameSessionHandler(
        const userver::components::ComponentConfig& config,
        const userver::components::ComponentContext& context
    );

 private:
    userver::formats::json::Value HandleRequestJsonThrow(
        const userver::server::http::HttpRequest& request,
        const userver::formats::json::Value& body,
        userver::server::request::RequestContext& context
    ) const override;

    GameSessionService& service_;
};

}  // namespace RumpelQuiz