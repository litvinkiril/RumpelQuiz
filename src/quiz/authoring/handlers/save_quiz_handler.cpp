#include "save_quiz_handler.hpp"

#include <optional>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

#include <boost/uuid/string_generator.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <userver/components/component_context.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/server/http/http_response.hpp>
#include <userver/server/http/http_status.hpp>

#include "auth/middleware/jwt/auth_context.hpp"
#include "quiz/authoring/handlers/serialization/quiz_json.hpp"
#include "quiz/quiz_models.hpp"

namespace RumpelQuiz {
namespace {

using HttpStatus = userver::server::http::HttpStatus;
using JsonValue = userver::formats::json::Value;
using JsonBuilder = userver::formats::json::ValueBuilder;

// Формирует единый формат ошибок запроса и сервиса.
JsonValue MakeErrorResponse(
    const userver::server::http::HttpRequest& request,
    HttpStatus status,
    std::string_view code,
    const std::vector<QuizFieldError>& details = {}
) {
    request.SetResponseStatus(status);

    JsonBuilder body;
    body["success"] = false;
    body["error"] = std::string{code};

    JsonBuilder errors(userver::formats::json::Type::kArray);

    for (const auto& detail : details) {
        JsonBuilder item;
        item["field"] = detail.field;
        item["message"] = detail.message;
        errors.PushBack(item.ExtractValue());
    }

    body["details"] = errors.ExtractValue();

    return body.ExtractValue();
}

JsonValue MakeServiceErrorResponse(
    const userver::server::http::HttpRequest& request,
    const SaveQuizFailure& failure
) {
    switch (failure.code) {
        case SaveQuizError::kAccessDenied:
            return MakeErrorResponse(
                request,
                HttpStatus::kForbidden,
                "quiz_access_denied"
            );

        case SaveQuizError::kNotFound:
            return MakeErrorResponse(
                request,
                HttpStatus::kNotFound,
                "quiz_not_found"
            );

        case SaveQuizError::kRevisionConflict:
            return MakeErrorResponse(
                request,
                HttpStatus::kConflict,
                "quiz_revision_conflict"
            );

        case SaveQuizError::kValidationFailed:
            return MakeErrorResponse(
                request,
                HttpStatus::kBadRequest,
                "quiz_validation_failed",
                failure.details
            );
    }

    throw std::logic_error("Unexpected SaveQuizError");
}

}  // namespace

SaveQuizHandler::SaveQuizHandler(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context
)
    : HttpHandlerJsonBase(config, context),
      quiz_service_(context.FindComponent<QuizService>()) {}

userver::formats::json::Value
SaveQuizHandler::HandleRequestJsonThrow(
    const userver::server::http::HttpRequest& request,
    const userver::formats::json::Value& request_body,
    userver::server::request::RequestContext& context
) const {
    request.GetHttpResponse().SetHeader(
        std::string_view{"Cache-Control"},
        "no-store"
    );

    const auto& user_id = GetAuthenticatedUserId(context);

    std::optional<boost::uuids::uuid> quiz_id;

    if (request.HasPathArg("quiz_id")) {
        try {
            quiz_id = boost::uuids::string_generator{}(
                request.GetPathArg("quiz_id")
            );
        } catch (const std::runtime_error&) {
            return MakeErrorResponse(
                request,
                HttpStatus::kBadRequest,
                "invalid_quiz_id"
            );
        }
    }

    const auto parsed = ParseSaveQuizRequest(request_body);

    if (const auto* error =
            std::get_if<QuizParseError>(&parsed)) {
        return MakeErrorResponse(
            request,
            HttpStatus::kBadRequest,
            "invalid_request",
            error->details
        );
    }

    const auto& input = std::get<SaveQuizRequest>(parsed);

    if (quiz_id && !input.revision.has_value()) {
        return MakeErrorResponse(
            request,
            HttpStatus::kBadRequest,
            "revision_required",
            {{
                "revision",
                "Передайте текущую версию квиза."
            }}
        );
    }

    if (!quiz_id && input.revision.has_value()) {
        return MakeErrorResponse(
            request,
            HttpStatus::kBadRequest,
            "revision_not_allowed",
            {{
                "revision",
                "При создании квиза revision передавать не нужно."
            }}
        );
    }

    const auto result = quiz_service_.SaveQuiz(
        user_id,
        quiz_id,
        input
    );

    if (const auto* failure =
            std::get_if<SaveQuizFailure>(&result)) {
        return MakeServiceErrorResponse(request, *failure);
    }

    const auto& saved = std::get<SaveQuizSuccess>(result);

    request.SetResponseStatus(
        quiz_id ? HttpStatus::kOk : HttpStatus::kCreated
    );

    JsonBuilder body;
    body["success"] = true;
    body["quiz_id"] = boost::uuids::to_string(saved.quiz_id);
    body["status"] = SerializeQuizStatus(saved.status);
    body["revision"] = saved.revision;

    return body.ExtractValue();
}

}  // namespace RumpelQuiz
