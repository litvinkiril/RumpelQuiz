#include "quiz_info_repository.hpp"

#include <string>

#include <userver/formats/json/serialize.hpp>
#include <userver/storages/postgres/io/row_types.hpp>
#include <userver/storages/postgres/io/uuid.hpp>

namespace RumpelQuiz {

bool QuizInfoRepository::CanViewQuiz(
    userver::storages::postgres::Transaction& tx,
    const boost::uuids::uuid& user_id,
    const boost::uuids::uuid& quiz_id) const {
  return tx.Execute(R"(
    SELECT EXISTS (
      SELECT 1
      FROM quiz.quizzes q
      WHERE q.id = $1
        AND EXISTS (
          SELECT 1
          FROM education.memberships m
          WHERE m.user_id = $2
            AND m.university_id = q.university_id
            AND m.status = 'active'
            AND m.role IN ('teacher', 'admin')
        )
    )
  )", quiz_id, user_id).AsSingleRow<bool>();
}

userver::formats::json::Value
QuizInfoRepository::ListQuizSessions(
    userver::storages::postgres::Transaction& tx,
    const boost::uuids::uuid& quiz_id) const {
  const auto json = tx.Execute(R"(
    SELECT COALESCE(
      jsonb_agg(
        jsonb_build_object(
          'session_id', s.id,
          'name', s.name,
          'status', s.status,
          'question_count', (SELECT count(*) FROM quiz.questions WHERE quiz_id = s.quiz_id),
          'participants_count', (
            SELECT count(*)
            FROM game.participants p
            WHERE p.session_id = s.id
          ),
          'created_at_ms',
            (extract(epoch FROM s.created_at) * 1000)::bigint,
          'host_name',
            COALESCE(
              NULLIF(
                btrim(concat_ws(
                  ' ',
                  NULLIF(btrim(pr.last_name), ''),
                  NULLIF(btrim(pr.first_name), ''),
                  NULLIF(btrim(pr.middle_name), '')
                )),
                ''
              ),
              'Преподаватель'
            )
        )
        ORDER BY s.created_at DESC, s.id DESC
      ),
      '[]'::jsonb
    )::text
    FROM game.sessions s
    LEFT JOIN users.profiles pr ON pr.user_id = s.host_user_id
    WHERE s.quiz_id = $1 AND s.status IN ('finished', 'cancelled')
  )", quiz_id).AsSingleRow<std::string>();

  return userver::formats::json::FromString(json);
}

std::optional<SessionResultsAccess> QuizInfoRepository::GetSessionResultsAccess(
    userver::storages::postgres::Transaction& tx,
    const boost::uuids::uuid& user_id,
    const boost::uuids::uuid& session_id) const {
  const auto row = tx.Execute(R"(
    SELECT s.status, EXISTS (
      SELECT 1 FROM education.memberships m
      WHERE m.user_id=$2 AND m.university_id=s.university_id AND m.status='active'
        AND (m.role IN ('teacher','admin') OR (m.role='student' AND EXISTS (
          SELECT 1 FROM game.participants p WHERE p.session_id=s.id AND p.user_id=$2
        )))
    ) FROM game.sessions s WHERE s.id=$1 FOR SHARE OF s
  )", session_id, user_id);
  if (row.IsEmpty()) return std::nullopt;
  return row.AsSingleRow<SessionResultsAccess>(userver::storages::postgres::kRowTag);
}

userver::formats::json::Value QuizInfoRepository::GetSessionResults(
    userver::storages::postgres::Transaction& tx,
    const boost::uuids::uuid& session_id) const {
  // A submission is correct only when its selected set exactly matches the
  // correct set. Missing choices and additional choices both score zero.
  const auto json = tx.Execute(R"(
    WITH scored AS (
      SELECT sub.user_id, NOT EXISTS (
        SELECT 1 FROM quiz.answers a WHERE a.question_id=sub.question_id
          AND a.is_correct <> EXISTS (
            SELECT 1 FROM game.submission_choices c
            WHERE c.submission_id=sub.id AND c.answer_id=a.id
          )
      ) AS correct
      FROM game.submissions sub WHERE sub.session_id=$1
    ), totals AS (
      SELECT user_id, count(*) AS answered_count,
             count(*) FILTER (WHERE correct) AS correct_count
      FROM scored GROUP BY user_id
    ), ranked AS (
      SELECT p.user_id, p.joined_at, s.quiz_id, s.status,
             COALESCE(t.answered_count, 0) AS answered_count,
             COALESCE(t.correct_count, 0) AS correct_count,
             CASE WHEN s.status='finished' THEN
               rank() OVER (ORDER BY COALESCE(t.correct_count, 0) DESC)
             END AS rank
      FROM game.participants p
      JOIN game.sessions s ON s.id=p.session_id
      LEFT JOIN totals t ON t.user_id=p.user_id
      WHERE p.session_id=$1 AND s.status IN ('finished','cancelled')
    )
    SELECT COALESCE(jsonb_agg(jsonb_build_object(
      'user_id', r.user_id,
      'name', COALESCE(NULLIF(btrim(concat_ws(' ',
        NULLIF(btrim(pr.last_name), ''), NULLIF(btrim(pr.first_name), ''),
        NULLIF(btrim(pr.middle_name), ''))), ''), 'Участник'),
      'answered_count', r.answered_count,
      'correct_count', r.correct_count,
      'score', r.correct_count,
      'rank', r.rank,
      'question_count', (SELECT count(*) FROM quiz.questions WHERE quiz_id=r.quiz_id)
    ) ORDER BY r.correct_count DESC, r.joined_at, r.user_id), '[]'::jsonb)::text
    FROM ranked r
    LEFT JOIN users.profiles pr ON pr.user_id=r.user_id
  )", session_id).AsSingleRow<std::string>();
  return userver::formats::json::FromString(json);
}

}  // namespace RumpelQuiz
