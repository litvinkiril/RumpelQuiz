#include "create_game_session_handler.hpp"

#include <variant>

#include <userver/components/component_context.hpp>

#include "auth/middleware/jwt/auth_context.hpp"
#include "game_handler_utils.hpp"

namespace RumpelQuiz {

CreateGameSessionHandler::CreateGameSessionHandler(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context
)
    : HttpHandlerJsonBase(config, context),
      service_(context.FindComponent<GameSessionService>()) {}

userver::formats::json::Value
CreateGameSessionHandler::HandleRequestJsonThrow(
    const userver::server::http::HttpRequest& request,
    const userver::formats::json::Value& body,
    userver::server::request::RequestContext& context
) const {
    GameHttp::NoStore(request);

    const auto& user_id = GetAuthenticatedUserId(context);

    const auto quiz_id = GameHttp::ReadUuid(body, "quiz_id");
    const auto session_id = GameHttp::ReadUuid(body, "session_id");

    if (!quiz_id || !session_id) {
        return GameHttp::Error(
            request,
            GameHttp::Status::kBadRequest,
            "invalid_request"
        );
    }

    const auto result = service_.Create(
        user_id,
        *quiz_id,
        *session_id
    );

    if (const auto* error =
            std::get_if<GameSessionFailure>(&result)) {
        return GameHttp::Failure(request, *error);
    }

    request.SetResponseStatus(GameHttp::Status::kOk);

    return GameHttp::SessionResponse(
        std::get<GameSession>(result)
    );
}

}  // namespace RumpelQuiz