#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

#include <boost/uuid/string_generator.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <userver/formats/json.hpp>
#include <userver/server/http/http_request.hpp>
#include <userver/server/http/http_response.hpp>
#include <userver/server/http/http_status.hpp>

#include "game/game_models.hpp"

namespace RumpelQuiz::GameHttp {

using Json = userver::formats::json::Value;
using Builder = userver::formats::json::ValueBuilder;
using Request = userver::server::http::HttpRequest;
using Status = userver::server::http::HttpStatus;

inline void NoStore(const Request& request) {
    request.GetHttpResponse().SetHeader(
        std::string_view{"Cache-Control"},
        "no-store"
    );
}

inline std::optional<boost::uuids::uuid> ParseUuid(
    std::string_view text
) {
    if (text.size() != 36) {
        return std::nullopt;
    }

    try {
        const auto id =
            boost::uuids::string_generator{}(std::string{text});

        if (id.is_nil()) {
            return std::nullopt;
        }

        return id;
    } catch (const std::runtime_error&) {
        return std::nullopt;
    }
}

inline std::optional<boost::uuids::uuid> ReadUuid(
    const Json& body,
    std::string_view field
) {
    if (!body.IsObject() ||
        !body.HasMember(field) ||
        !body[field].IsString()) {
        return std::nullopt;
    }

    return ParseUuid(body[field].As<std::string>());
}

inline std::int64_t Milliseconds(
    userver::storages::postgres::TimePointTz value
) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        value.GetUnderlying().time_since_epoch()
    ).count();
}

inline Json Error(
    const Request& request,
    Status status,
    std::string_view code
) {
    request.SetResponseStatus(status);

    Builder body;
    body["success"] = false;
    body["error"] = std::string{code};

    return body.ExtractValue();
}

inline Json Failure(
    const Request& request,
    const GameSessionFailure& error
) {
    Status status = Status::kConflict;
    std::string_view code;

    switch (error.code) {
        case GameSessionError::kInvalidRequest:
            status = Status::kBadRequest;
            code = "invalid_request";
            break;

        case GameSessionError::kAccessDenied:
            status = Status::kForbidden;
            code = "game_access_denied";
            break;

        case GameSessionError::kQuizNotReady:
            code = "quiz_not_ready";
            break;

        case GameSessionError::kRequestIdConflict:
            code = "request_id_conflict";
            break;

        case GameSessionError::kJoinCodeUnavailable:
            status = Status::kServiceUnavailable;
            code = "join_code_unavailable";
            break;

        case GameSessionError::kSessionClosed:
            code = "session_closed";
            break;

        case GameSessionError::kStateChanged:
            code = "session_state_changed";
            break;

        case GameSessionError::kNoMoreQuestions:
            code = "no_more_questions";
            break;

        default:
            throw std::logic_error("Unexpected GameSessionError");
    }

    Builder body(Error(request, status, code));

    if (error.code == GameSessionError::kStateChanged) {
        body["current_question_id"] = nullptr;

        if (error.current_question_id) {
            body["current_question_id"] =
                boost::uuids::to_string(*error.current_question_id);
        }
    }

    return body.ExtractValue();
}

inline Json SessionResponse(const GameSession& session) {
    Builder body;

    body["success"] = true;

    body["session"]["id"] =
        boost::uuids::to_string(session.id);

    body["session"]["quiz_id"] =
        boost::uuids::to_string(session.quiz_id);

    body["session"]["host_user_id"] =
        boost::uuids::to_string(session.host_user_id);

    body["session"]["university_id"] =
        boost::uuids::to_string(session.university_id);

    body["session"]["join_code"] = session.join_code;
    body["session"]["status"] = session.status;

    body["session"]["created_at_ms"] =
        Milliseconds(session.created_at);

    body["session"]["started_at_ms"] = nullptr;
    body["session"]["ended_at_ms"] = nullptr;

    if (session.started_at) {
        body["session"]["started_at_ms"] =
            Milliseconds(*session.started_at);
    }

    if (session.ended_at) {
        body["session"]["ended_at_ms"] =
            Milliseconds(*session.ended_at);
    }

    return body.ExtractValue();
}

}  // namespace RumpelQuiz::GameHttp