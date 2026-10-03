#pragma once

#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/server/http/http_response_body_stream_fwd.hpp>

#include "game/service/game_events.hpp"
#include "game/service/game_session_service.hpp"

namespace RumpelQuiz {

class GameEventsHandler final : public userver::server::handlers::HttpHandlerBase {
 public:
  static constexpr std::string_view kName = "handler-game-events";
  GameEventsHandler(const userver::components::ComponentConfig&,
                    const userver::components::ComponentContext&);

 private:
  void HandleStreamRequest(userver::server::http::HttpRequest&,
                           userver::server::request::RequestContext&,
                           userver::server::http::ResponseBodyStream&) const override;

  const GameSessionService& service_;
  GameEvents& events_;
};

}  // namespace RumpelQuiz
