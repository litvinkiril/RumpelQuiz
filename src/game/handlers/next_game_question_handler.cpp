#include "next_game_question_handler.hpp"

#include <optional>
#include <variant>

#include <userver/components/component_context.hpp>

#include "auth/middleware/jwt/auth_context.hpp"
#include "game_handler_utils.hpp"

namespace RumpelQuiz {

NextGameQuestionHandler::NextGameQuestionHandler(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context
)
    : HttpHandlerJsonBase(config, context),
      service_(context.FindComponent<GameSessionService>()) {}

userver::formats::json::Value
NextGameQuestionHandler::HandleRequestJsonThrow(
    const userver::server::http::HttpRequest& request,
    const userver::formats::json::Value& body,
    userver::server::request::RequestContext& context
) const {
    GameHttp::NoStore(request);

    const auto& user_id = GetAuthenticatedUserId(context);

    const auto session_id = GameHttp::ParseUuid(
        request.GetPathArg("session_id")
    );

    if (!session_id) {
        return GameHttp::Error(
            request,
            GameHttp::Status::kBadRequest,
            "invalid_session_id"
        );
    }

    // Поле обязательно:
    // null — запуск первого вопроса;
    // UUID — переключение с отображаемого вопроса.
    if (!body.IsObject() ||
        !body.HasMember("expected_question_id")) {
        return GameHttp::Error(
            request,
            GameHttp::Status::kBadRequest,
            "invalid_request"
        );
    }

    std::optional<boost::uuids::uuid> expected_question_id;

    if (!body["expected_question_id"].IsNull()) {
        expected_question_id = GameHttp::ReadUuid(
            body,
            "expected_question_id"
        );

        if (!expected_question_id) {
            return GameHttp::Error(
                request,
                GameHttp::Status::kBadRequest,
                "invalid_request"
            );
        }
    }

    const auto result = service_.NextQuestion(
        user_id,
        *session_id,
        expected_question_id
    );

    if (const auto* error =
            std::get_if<GameSessionFailure>(&result)) {
        return GameHttp::Failure(request, *error);
    }

    const auto& success =
        std::get<NextGameQuestionSuccess>(result);

    const auto& question = success.question;

    GameHttp::Builder response;

    response["success"] = true;

    response["session_id"] =
        boost::uuids::to_string(*session_id);

    response["status"] = "running";

    response["question_id"] =
        boost::uuids::to_string(question.question_id);

    response["opened_at_ms"] =
        GameHttp::Milliseconds(question.opened_at);

    response["deadline_at_ms"] =
        GameHttp::Milliseconds(question.deadline_at);

    response["server_time_ms"] =
        GameHttp::Milliseconds(success.server_time);

    response["has_next"] = question.has_next;

    request.SetResponseStatus(GameHttp::Status::kOk);

    return response.ExtractValue();
}

}  // namespace RumpelQuiz