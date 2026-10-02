#pragma once

#include <optional>
#include <string>
#include <variant>

#include <boost/uuid/uuid.hpp>
#include <userver/storages/postgres/io/chrono.hpp>

namespace RumpelQuiz {

struct GameSession {
    boost::uuids::uuid id;
    boost::uuids::uuid quiz_id;
    boost::uuids::uuid host_user_id;
    boost::uuids::uuid university_id;

    std::string join_code;

    // waiting / running / finished / cancelled
    std::string status;

    userver::storages::postgres::TimePointTz created_at;
    std::optional<userver::storages::postgres::TimePointTz> started_at;
    std::optional<userver::storages::postgres::TimePointTz> ended_at;
};

struct OpenedGameQuestion {
    boost::uuids::uuid question_id;

    userver::storages::postgres::TimePointTz opened_at;
    userver::storages::postgres::TimePointTz deadline_at;

    bool has_next;
};

enum class GameSessionError {
    kInvalidRequest,
    kAccessDenied,
    kQuizNotReady,
    kRequestIdConflict,
    kJoinCodeUnavailable,
    kSessionClosed,
    kStateChanged,
    kNoMoreQuestions
};

struct GameSessionFailure {
    GameSessionError code;

    // При kStateChanged — фактический последний открытый вопрос.
    std::optional<boost::uuids::uuid> current_question_id = std::nullopt;
};

using GameSessionResult =
    std::variant<GameSession, GameSessionFailure>;

using NextGameQuestionResult =
    std::variant<OpenedGameQuestion, GameSessionFailure>;


struct NextGameQuestionSuccess {
    OpenedGameQuestion question;
    userver::storages::postgres::TimePointTz server_time;
};

using NextGameQuestionServiceResult =
    std::variant<NextGameQuestionSuccess, GameSessionFailure>;

}  // namespace RumpelQuiz