#include "game_events_handler.hpp"

#include <algorithm>
#include <chrono>
#include <userver/components/component_context.hpp>
#include <userver/engine/task/cancel.hpp>
#include <userver/formats/json/serialize.hpp>
#include <userver/server/http/http_response_body_stream.hpp>

#include "auth/middleware/jwt/auth_context.hpp"
#include "game_handler_utils.hpp"

namespace RumpelQuiz {

GameEventsHandler::GameEventsHandler(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : HttpHandlerBase(config, context),
      service_(context.FindComponent<GameSessionService>()),
      events_(context.FindComponent<GameEvents>()) {}

void GameEventsHandler::HandleStreamRequest(
    userver::server::http::HttpRequest& request,
    userver::server::request::RequestContext& context,
    userver::server::http::ResponseBodyStream& stream) const {
  const auto session = GameHttp::ParseUuid(request.GetPathArg("session_id"));
  if (!session) {
    stream.SetStatusCode(userver::server::http::HttpStatus::kBadRequest);
    stream.SetEndOfHeaders();
    return;
  }

  const auto& user = GetAuthenticatedUserId(context);
  // Read before sending headers, so an unauthorized stream can return 403.
  userver::formats::json::Value state;
  try {
    state = service_.Read(user, *session);
  } catch (const GamePlayError&) {
    stream.SetStatusCode(userver::server::http::HttpStatus::kForbidden);
    stream.SetEndOfHeaders();
    return;
  }
  const bool is_host = state["session"]["is_host"].As<bool>();
  auto subscriber = events_.Subscribe(*session, is_host);
  // Read again after subscribing: a write between the first read and the
  // subscription cannot leave the client with an obsolete initial state.
  try {
    state = service_.Read(user, *session);
  } catch (const GamePlayError&) {
    stream.SetStatusCode(userver::server::http::HttpStatus::kForbidden);
    stream.SetEndOfHeaders();
    return;
  }

  stream.SetHeader(std::string_view{"Content-Type"}, "text/event-stream; charset=utf-8");
  stream.SetHeader(std::string_view{"Cache-Control"}, "no-store");
  stream.SetHeader(std::string_view{"X-Accel-Buffering"}, "no");
  stream.SetEndOfHeaders();
  const auto ended = [](const auto& value) {
    const auto status = value["session"]["status"].template As<std::string>();
    return status == "finished" || status == "cancelled";
  };
  const auto send_state = [&](bool terminal) {
    stream.PushBodyChunk(
        std::string{"event: "} + (terminal ? "results" : "snapshot") +
            "\ndata: " + userver::formats::json::ToString(state) + "\n\n",
        userver::engine::Deadline::FromDuration(std::chrono::seconds{5}));
  };
  send_state(ended(state));
  if (ended(state)) return;

  // Refresh the bearer token at reconnect and bound the lifetime of a stream.
  const auto token_expiry = std::chrono::system_clock::time_point{
      std::chrono::seconds{GetAuthenticatedTokenExpiry(context)}};
  const auto stop_at = std::min(token_expiry,
                                std::chrono::system_clock::now() +
                                    std::chrono::minutes{5});
  while (!userver::engine::current_task::ShouldCancel() &&
         std::chrono::system_clock::now() < stop_at) {
    if (auto message = GameEvents::Pop(subscriber); !message.empty()) {
      // Resolve final results per connection after the closing transaction
      // commits. Also handle a terminal notification coalesced into resync.
      if (message.starts_with("event: results\n") ||
          message.starts_with("event: resync\n")) {
        state = service_.Read(user, *session);
        if (ended(state)) {
          send_state(true);
          return;  // No heartbeat or queued event may follow the results.
        }
      }
      stream.PushBodyChunk(
          std::move(message),
          userver::engine::Deadline::FromDuration(std::chrono::seconds{5}));
      continue;
    }
    if (!subscriber->wake.WaitForEventFor(std::chrono::seconds{15})) {
      if (userver::engine::current_task::ShouldCancel()) break;
      stream.PushBodyChunk(
          std::string{": ping\n\n"},
          userver::engine::Deadline::FromDuration(std::chrono::seconds{5}));
    }
  }
}

}  // namespace RumpelQuiz
