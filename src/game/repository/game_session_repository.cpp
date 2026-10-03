#include "game_session_repository.hpp"

#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>

#include <openssl/rand.h>

#include <userver/storages/postgres/io/optional.hpp>
#include <userver/storages/postgres/io/row_types.hpp>
#include <userver/storages/postgres/io/uuid.hpp>

namespace RumpelQuiz {
namespace pg = userver::storages::postgres;

namespace {

struct AuthorQuiz {
    boost::uuids::uuid university_id;
    std::string status;
    bool has_questions;
};

struct PreviousQuestion {
    boost::uuids::uuid id;
    std::int32_t position;
};

struct NextQuestionDefinition {
    boost::uuids::uuid id;
    std::int32_t position;
    std::int32_t time_seconds;
};

std::string GenerateJoinCode() {
    std::array<unsigned char, 3> bytes{};

    for (;;) {
        if (RAND_bytes(
                bytes.data(),
                static_cast<int>(bytes.size())
            ) != 1) {
            throw std::runtime_error(
                "Failed to generate game join code"
            );
        }

        const auto number =
            (static_cast<std::uint32_t>(bytes[0]) << 16) |
            (static_cast<std::uint32_t>(bytes[1]) << 8) |
            static_cast<std::uint32_t>(bytes[2]);

        // Отбрасываем хвост диапазона для равномерного распределения.
        if (number < 16000000) {
            // Всегда шесть символов, включая ведущие нули.
            return std::to_string(
                1000000 + number % 1000000
            ).substr(1);
        }
    }
}

}  // namespace

std::optional<GameSession> GameSessionRepository::LockOwnedSession(
    pg::Transaction& tx,
    const boost::uuids::uuid& user_id,
    const boost::uuids::uuid& session_id
) const {
    const auto result = tx.Execute(R"(
        SELECT
            s.id,
            s.quiz_id,
            s.host_user_id,
            s.university_id,
            s.join_code,
            s.name,
            s.status,
            s.created_at,
            s.started_at,
            s.ended_at
        FROM game.sessions s
        WHERE s.id = $1
          AND s.host_user_id = $2
          AND EXISTS (
              SELECT 1
              FROM education.memberships m
              WHERE m.user_id = $2
                AND m.university_id = s.university_id
                AND m.status = 'active'
                AND m.role IN ('teacher', 'admin')
          )
        FOR UPDATE OF s
    )", session_id, user_id);

    if (result.IsEmpty()) {
        return std::nullopt;
    }

    return result.AsSingleRow<GameSession>(pg::kRowTag);
}

GameSessionResult GameSessionRepository::Create(
    pg::Transaction& tx,
    const boost::uuids::uuid& user_id,
    const boost::uuids::uuid& quiz_id,
    const boost::uuids::uuid& session_id,
    const std::string& name
) const {
    if (user_id.is_nil() || quiz_id.is_nil() || session_id.is_nil()) {
        return GameSessionFailure{
            GameSessionError::kInvalidRequest
        };
    }

    // Одновременные повторы с одним session_id выполняются по очереди.
    tx.Execute(R"(
        SELECT pg_advisory_xact_lock(
            hashtextextended($1::uuid::text, 0)
        )
    )", session_id);

    // В этой версии ведущий запускает собственный квиз.
    const auto quiz_result = tx.Execute(R"(
        SELECT
            q.university_id,
            q.status,
            EXISTS (
                SELECT 1
                FROM quiz.questions
                WHERE quiz_id = q.id
            )
        FROM quiz.quizzes q
        WHERE q.id = $1
          AND q.author_id = $2
          AND EXISTS (
              SELECT 1
              FROM education.memberships m
              WHERE m.user_id = $2
                AND m.university_id = q.university_id
                AND m.status = 'active'
                AND m.role IN ('teacher', 'admin')
          )
        FOR SHARE OF q
    )", quiz_id, user_id);

    if (quiz_result.IsEmpty()) {
        return GameSessionFailure{
            GameSessionError::kAccessDenied
        };
    }

    const auto quiz =
        quiz_result.AsSingleRow<AuthorQuiz>(pg::kRowTag);

    const auto existing = tx.Execute(R"(
        SELECT
            id,
            quiz_id,
            host_user_id,
            university_id,
            join_code,
            name,
            status,
            created_at,
            started_at,
            ended_at
        FROM game.sessions
        WHERE id = $1
    )", session_id);

    if (!existing.IsEmpty()) {
        const auto session =
            existing.AsSingleRow<GameSession>(pg::kRowTag);

        if (session.host_user_id != user_id ||
            session.quiz_id != quiz_id || session.name != name) {
            return GameSessionFailure{
                GameSessionError::kRequestIdConflict
            };
        }

        return session;
    }

    if (quiz.status != "ready" || !quiz.has_questions) {
        return GameSessionFailure{
            GameSessionError::kQuizNotReady
        };
    }

    for (int attempt = 0; attempt < 32; ++attempt) {
        const auto result = tx.Execute(R"(
            INSERT INTO game.sessions (
                id,
                quiz_id,
                host_user_id,
                university_id,
                join_code,
                name
            )
            VALUES ($1, $2, $3, $4, $5, $6)
            ON CONFLICT (join_code)
                WHERE status IN ('waiting', 'running')
            DO NOTHING
            RETURNING
                id,
                quiz_id,
                host_user_id,
                university_id,
                join_code,
                name,
                status,
                created_at,
                started_at,
                ended_at
        )",
            session_id,
            quiz_id,
            user_id,
            quiz.university_id,
            GenerateJoinCode(),
            name
        );

        if (!result.IsEmpty()) {
            return result.AsSingleRow<GameSession>(pg::kRowTag);
        }
    }

    return GameSessionFailure{
        GameSessionError::kJoinCodeUnavailable
    };
}

NextGameQuestionResult GameSessionRepository::NextQuestion(
    pg::Transaction& tx,
    const boost::uuids::uuid& user_id,
    const boost::uuids::uuid& session_id,
    const std::optional<boost::uuids::uuid>& expected_question_id
) const {
    const auto session = LockOwnedSession(tx, user_id, session_id);

    if (!session) {
        return GameSessionFailure{
            GameSessionError::kAccessDenied
        };
    }

    if (session->status != "waiting" &&
        session->status != "running") {
        return GameSessionFailure{
            GameSessionError::kSessionClosed
        };
    }

    const auto previous_result = tx.Execute(R"(
        SELECT q.id, q.position
        FROM game.session_questions sq
        JOIN quiz.questions q ON q.id = sq.question_id
        WHERE sq.session_id = $1
        ORDER BY q.position DESC
        LIMIT 1
    )", session_id);

    std::optional<boost::uuids::uuid> current_question_id;
    std::int32_t current_position = -1;

    if (!previous_result.IsEmpty()) {
        const auto previous =
            previous_result.AsSingleRow<PreviousQuestion>(
                pg::kRowTag
            );

        current_question_id = previous.id;
        current_position = previous.position;
    }

    // Повторный запрос не должен открыть ещё один вопрос.
    if (current_question_id != expected_question_id) {
        return GameSessionFailure{
            GameSessionError::kStateChanged,
            current_question_id
        };
    }

    const auto next_result = tx.Execute(R"(
        SELECT
            q.id,
            q.position,
            COALESCE(q.time_seconds, k.default_time_seconds)
        FROM quiz.questions q
        JOIN quiz.quizzes k ON k.id = q.quiz_id
        WHERE q.quiz_id = $1
          AND q.position > $2
        ORDER BY q.position
        LIMIT 1
    )", session->quiz_id, current_position);

    if (next_result.IsEmpty()) {
        return GameSessionFailure{
            GameSessionError::kNoMoreQuestions
        };
    }

    const auto next =
        next_result.AsSingleRow<NextQuestionDefinition>(
            pg::kRowTag
        );

    // Время получаем из БД после блокировки сессии.
    const auto now = tx.Execute("SELECT clock_timestamp()")
        .AsSingleRow<pg::TimePointTz>();

    tx.Execute(R"(
        UPDATE game.session_questions
        SET closed_at = LEAST($2::timestamptz, deadline_at)
        WHERE session_id = $1
          AND closed_at IS NULL
    )", session_id, now);

    const auto opened = tx.Execute(R"(
        INSERT INTO game.session_questions (
            session_id,
            question_id,
            quiz_id,
            opened_at,
            deadline_at
        )
        VALUES (
            $1,
            $2,
            $3,
            $4,
            $4::timestamptz + $5::integer * INTERVAL '1 second'
        )
        RETURNING
            question_id,
            opened_at,
            deadline_at,
            EXISTS (
                SELECT 1
                FROM quiz.questions q
                WHERE q.quiz_id = $3
                  AND q.position > $6
            )
    )",
        session_id,
        next.id,
        session->quiz_id,
        now,
        next.time_seconds,
        next.position
    );

    tx.Execute(R"(
        UPDATE game.sessions
        SET status = 'running',
            started_at = COALESCE(started_at, $2)
        WHERE id = $1
    )", session_id, now);

    return opened.AsSingleRow<OpenedGameQuestion>(pg::kRowTag);
}

GameSessionResult GameSessionRepository::Close(
    pg::Transaction& tx,
    const boost::uuids::uuid& user_id,
    const boost::uuids::uuid& session_id
) const {
    const auto session = LockOwnedSession(tx, user_id, session_id);

    if (!session) {
        return GameSessionFailure{
            GameSessionError::kAccessDenied
        };
    }

    // Повторное закрытие возвращает уже сохранённое состояние.
    if (session->status == "finished" ||
        session->status == "cancelled") {
        return *session;
    }

    const auto now = tx.Execute("SELECT clock_timestamp()")
        .AsSingleRow<pg::TimePointTz>();

    tx.Execute(R"(
        UPDATE game.session_questions
        SET closed_at = LEAST($2::timestamptz, deadline_at)
        WHERE session_id = $1
          AND closed_at IS NULL
    )", session_id, now);

    const auto result = tx.Execute(R"(
        UPDATE game.sessions
        SET status = CASE
                WHEN started_at IS NULL THEN 'cancelled'
                ELSE 'finished'
            END,
            ended_at = $2
        WHERE id = $1
        RETURNING
            id,
            quiz_id,
            host_user_id,
            university_id,
            join_code,
            name,
            status,
            created_at,
            started_at,
            ended_at
    )", session_id, now);

    return result.AsSingleRow<GameSession>(pg::kRowTag);
}

}  // namespace RumpelQuiz