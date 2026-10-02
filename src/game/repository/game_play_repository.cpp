#include "game_play_repository.hpp"
#include <algorithm>
#include <userver/formats/json.hpp>
#include <userver/storages/postgres/io/array_types.hpp>
#include <userver/storages/postgres/io/row_types.hpp>
#include <userver/storages/postgres/io/uuid.hpp>
namespace RumpelQuiz {
namespace pg = userver::storages::postgres;
namespace {
struct SessionAccess {
  boost::uuids::uuid id, university, host;
  std::string status;
};
bool IsStudent(pg::Transaction& tx, const boost::uuids::uuid& user,
               const boost::uuids::uuid& uni) {
  return tx
      .Execute(
          "SELECT EXISTS(SELECT 1 FROM education.memberships WHERE user_id=$1 "
          "AND university_id=$2 AND role='student' AND status='active')",
          user, uni)
      .AsSingleRow<bool>();
}
}  // namespace
boost::uuids::uuid GamePlayRepository::Join(pg::Transaction& tx,
                                            const boost::uuids::uuid& user,
                                            const std::string& code) const {
  const auto found = tx.Execute(
      "SELECT id,university_id,host_user_id,status FROM game.sessions WHERE "
      "join_code=$1 AND status IN ('waiting','running') FOR UPDATE",
      code);
  if (found.IsEmpty()) throw GamePlayError("game_not_found");
  const auto s = found.AsSingleRow<SessionAccess>(pg::kRowTag);
  if (!IsStudent(tx, user, s.university))
    throw GamePlayError("game_university_mismatch");
  tx.Execute(
      "INSERT INTO game.participants(session_id,user_id) VALUES($1,$2) ON "
      "CONFLICT DO NOTHING",
      s.id, user);
  return s.id;
}
userver::formats::json::Value GamePlayRepository::Read(
    pg::Transaction& tx, const boost::uuids::uuid& user,
    const boost::uuids::uuid& session) const {
  // Shared lock gives a coherent state while next/close/submit lock the same
  // row.
  auto allowed = tx.Execute(R"(
    SELECT s.id FROM game.sessions s WHERE s.id=$1 AND (
      (s.host_user_id=$2 AND EXISTS(SELECT 1 FROM education.memberships m WHERE m.user_id=$2 AND m.university_id=s.university_id AND m.status='active' AND m.role IN ('teacher','admin')))
      OR (EXISTS(SELECT 1 FROM game.participants p WHERE p.session_id=s.id AND p.user_id=$2)
          AND EXISTS(SELECT 1 FROM education.memberships m WHERE m.user_id=$2 AND m.university_id=s.university_id AND m.role='student' AND m.status='active'))
    ) FOR SHARE OF s
  )",
                            session, user);
  if (allowed.IsEmpty()) throw GamePlayError("game_access_denied");
  auto result = tx.Execute(R"(
SELECT jsonb_build_object(
 'success',true,'server_time_ms',floor(extract(epoch FROM clock_timestamp())*1000)::bigint,
 'session',jsonb_build_object('id',s.id,'quiz_id',s.quiz_id,'name',k.name,
   'join_code',s.join_code,'status',s.status,'is_host',s.host_user_id=$2,
   'question_count',(SELECT count(*) FROM quiz.questions WHERE quiz_id=s.quiz_id),
   'participants_count',(SELECT count(*) FROM game.participants WHERE session_id=s.id)),
 'participants',CASE WHEN s.host_user_id=$2 THEN COALESCE((
   SELECT jsonb_agg(jsonb_build_object('user_id',p.user_id,
     'name',COALESCE(NULLIF(trim(concat_ws(' ',pr.last_name,pr.first_name)),''),'Участник')) ORDER BY p.joined_at,p.user_id)
   FROM game.participants p LEFT JOIN users.profiles pr ON pr.user_id=p.user_id WHERE p.session_id=s.id
 ),'[]'::jsonb) ELSE '[]'::jsonb END,
 'current_question',CASE WHEN cq.id IS NULL THEN NULL ELSE jsonb_build_object(
   'id',cq.id,'position',cq.position,'text',cq.text,'type',cq.type,
   'image_key',mi.storage_key,
   'opened_at_ms',floor(extract(epoch FROM cq.opened_at)*1000)::bigint,
   'deadline_at_ms',floor(extract(epoch FROM cq.deadline_at)*1000)::bigint,
   'accepting_answers',s.status='running' AND cq.closed_at IS NULL AND clock_timestamp()<cq.deadline_at,
   'has_next',EXISTS(SELECT 1 FROM quiz.questions WHERE quiz_id=s.quiz_id AND position>cq.position),
   'answered_count',(SELECT count(*) FROM game.submissions WHERE session_id=s.id AND question_id=cq.id),
   'answers',COALESCE((SELECT jsonb_agg(jsonb_build_object('id',a.id,'text',a.text,'image_key',am.storage_key) ORDER BY a.position)
      FROM quiz.answers a LEFT JOIN media.images am ON am.id=a.image_id WHERE a.question_id=cq.id),'[]'::jsonb),
   'submitted',EXISTS(SELECT 1 FROM game.submissions WHERE session_id=s.id AND question_id=cq.id AND user_id=$2),
   'selected_ids',COALESCE((SELECT jsonb_agg(c.answer_id ORDER BY c.answer_id) FROM game.submission_choices c
      JOIN game.submissions sub ON sub.id=c.submission_id WHERE sub.session_id=s.id AND sub.user_id=$2 AND sub.question_id=cq.id),'[]'::jsonb)
 ) END,
 'results',CASE WHEN s.status='finished' THEN COALESCE((
   SELECT jsonb_agg(jsonb_build_object('user_id',p.user_id,
     'name',COALESCE(NULLIF(trim(concat_ws(' ',pr.last_name,pr.first_name)),''),'Участник'),
     'answered_count',(SELECT count(*) FROM game.submissions sub WHERE sub.session_id=s.id AND sub.user_id=p.user_id)
   ) ORDER BY p.joined_at,p.user_id)
   FROM game.participants p LEFT JOIN users.profiles pr ON pr.user_id=p.user_id
   WHERE p.session_id=s.id AND (s.host_user_id=$2 OR p.user_id=$2)
 ),'[]'::jsonb) ELSE '[]'::jsonb END
)::text
FROM game.sessions s JOIN quiz.quizzes k ON k.id=s.quiz_id
LEFT JOIN LATERAL (
 SELECT q.id,q.position,q.text,q.type,q.image_id,sq.opened_at,sq.deadline_at,sq.closed_at
 FROM game.session_questions sq JOIN quiz.questions q ON q.id=sq.question_id
 WHERE sq.session_id=s.id ORDER BY q.position DESC LIMIT 1
) cq ON true LEFT JOIN media.images mi ON mi.id=cq.image_id WHERE s.id=$1
  )",
                           session, user);
  return userver::formats::json::FromString(result.AsSingleRow<std::string>());
}
void GamePlayRepository::Submit(
    pg::Transaction& tx, const boost::uuids::uuid& user,
    const boost::uuids::uuid& session, const boost::uuids::uuid& question,
    const std::vector<boost::uuids::uuid>& choices) const {
  auto selected = choices;
  std::sort(selected.begin(), selected.end());
  if (selected.empty() || selected.size() > 20 ||
      std::adjacent_find(selected.begin(), selected.end()) != selected.end())
    throw GamePlayError("invalid_request");
  auto found = tx.Execute(
      "SELECT id,university_id,host_user_id,status FROM game.sessions WHERE "
      "id=$1 FOR UPDATE",
      session);
  if (found.IsEmpty()) throw GamePlayError("game_access_denied");
  const auto s = found.AsSingleRow<SessionAccess>(pg::kRowTag);
  if (!IsStudent(tx, user, s.university) ||
      !tx.Execute("SELECT EXISTS(SELECT 1 FROM game.participants WHERE "
                  "session_id=$1 AND user_id=$2)",
                  session, user)
           .AsSingleRow<bool>())
    throw GamePlayError("game_access_denied");
  // Lost acknowledgements can be recovered even after the question closes.
  auto old = tx.Execute(
      "SELECT id FROM game.submissions WHERE session_id=$1 AND user_id=$2 AND "
      "question_id=$3",
      session, user, question);
  if (!old.IsEmpty()) {
    auto stored = tx.Execute(
        "SELECT answer_id FROM game.submission_choices WHERE submission_id=$1 "
        "ORDER BY answer_id",
        old.AsSingleRow<boost::uuids::uuid>());
    if (stored.AsContainer<std::vector<boost::uuids::uuid>>() != selected)
      throw GamePlayError("answer_already_saved");
    return;
  }
  if (s.status != "running") throw GamePlayError("session_closed");
  auto active = tx.Execute(R"(
    SELECT q.type FROM game.session_questions sq JOIN quiz.questions q ON q.id=sq.question_id
    WHERE sq.session_id=$1 AND sq.question_id=$2 AND sq.closed_at IS NULL AND clock_timestamp()<sq.deadline_at
  )",
                           session, question);
  if (active.IsEmpty()) throw GamePlayError("question_closed");
  if (active.AsSingleRow<std::string>() == "single" && selected.size() != 1)
    throw GamePlayError("invalid_request");
  if (tx.Execute("SELECT id FROM quiz.answers WHERE question_id=$1 AND "
                 "id=ANY($2::uuid[])",
                 question, selected)
          .Size() != selected.size())
    throw GamePlayError("invalid_request");
  auto id =
      tx.Execute(
            "INSERT INTO game.submissions(session_id,user_id,question_id) "
            "VALUES($1,$2,$3) RETURNING id",
            session, user, question)
          .AsSingleRow<boost::uuids::uuid>();
  tx.Execute(
      "INSERT INTO "
      "game.submission_choices(submission_id,question_id,answer_id) SELECT "
      "$1,$2,unnest($3::uuid[])",
      id, question, selected);
}
}  // namespace RumpelQuiz
