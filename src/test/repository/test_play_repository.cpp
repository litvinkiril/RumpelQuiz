#include "test_play_repository.hpp"
#include <algorithm>
#include <boost/uuid/random_generator.hpp>
#include <optional>
#include <userver/formats/json.hpp>
#include <userver/storages/postgres/io/array_types.hpp>
#include <userver/storages/postgres/io/optional.hpp>
#include <userver/storages/postgres/io/row_types.hpp>
#include <userver/storages/postgres/io/uuid.hpp>
namespace RumpelQuiz {
namespace {
using Tx = userver::storages::postgres::Transaction;
using Id = boost::uuids::uuid;
void StudentAccess(Tx& tx, const Id& user, const Id& test) {
  // Publication and authoring use the same parent row lock.
  if (tx.Execute("SELECT id FROM test.tests WHERE id=$1 AND status IN "
                 "('public','private') FOR SHARE",
                 test)
          .IsEmpty())
    throw TestPlayError("test_not_found");
  if (tx.Execute(R"(SELECT t.id FROM test.tests t WHERE t.id=$1 AND EXISTS(
    SELECT 1 FROM education.memberships m WHERE m.university_id=t.university_id
    AND m.user_id=$2 AND m.status='active' AND m.role='student'))",
                 test, user)
          .IsEmpty())
    throw TestPlayError("test_access_denied");
}
struct Progress {
  Id id;
  std::optional<Id> question;
  std::string status;
};
Progress Lock(Tx& tx, const Id& user, const Id& test) {
  auto r = tx.Execute(
      "SELECT id,current_question_id,status FROM test.test_in_process WHERE "
      "test_id=$1 AND student_id=$2 FOR UPDATE",
      test, user);
  if (r.IsEmpty()) throw TestPlayError("test_not_started");
  auto p = r.AsSingleRow<Progress>(userver::storages::postgres::kRowTag);
  // clock_timestamp is checked after acquiring the row lock.
  if (p.status == "in_progress" && !tx.Execute(R"(UPDATE test.test_in_process
    SET status='expired',current_question_id=NULL,finished_at=clock_timestamp()
    WHERE id=$1 AND deadline_at<=clock_timestamp() RETURNING id)",
                                               p.id)
                                        .IsEmpty()) {
    p.status = "expired";
    p.question.reset();
  }
  return p;
}
userver::formats::json::Value Json(Tx& tx, const std::string& sql,
                                   const Id& id) {
  return userver::formats::json::FromString(
      tx.Execute(sql, id).AsSingleRow<std::string>());
}
// Exact set equality awards one point; skipped and partially correct answers
// score zero.
const std::string kScores = R"(
 SELECT p.id,p.student_id,p.status,p.started_at,p.deadline_at,p.finished_at,
 (SELECT count(*) FROM test.questions q WHERE q.test_id=p.test_id) AS question_count,
 (SELECT count(*) FROM test.student_answers s WHERE s.process_id=p.id) AS answered_count,
 (SELECT count(*) FROM test.student_answers s WHERE s.process_id=p.id AND NOT EXISTS(
   SELECT 1 FROM test.answers a WHERE a.question_id=s.question_id AND a.is_correct IS DISTINCT FROM
   EXISTS(SELECT 1 FROM test.student_answer_choices c WHERE c.student_answer_id=s.id AND c.answer_id=a.id))) AS score
 FROM test.test_in_process p
)";
}  // namespace
userver::formats::json::Value TestPlayRepository::Available(
    Tx& tx, const Id& user) const {
  return Json(
      tx,
      R"(SELECT COALESCE(jsonb_agg(to_jsonb(r) ORDER BY r.created_at DESC,r.id),'[]'::jsonb)::text FROM(
    SELECT t.id,t.name,t.description,t.time_to_complete,t.created_at,u.name AS university_name,
    p.status AS attempt_status FROM test.tests t JOIN education.universities u ON u.id=t.university_id
    LEFT JOIN test.test_in_process p ON p.test_id=t.id AND p.student_id=$1
    WHERE (t.status='public' OR p.id IS NOT NULL) AND t.status<>'draft' AND EXISTS(
      SELECT 1 FROM education.memberships m WHERE m.user_id=$1 AND m.university_id=t.university_id
      AND m.role='student' AND m.status='active') ORDER BY t.created_at DESC,t.id LIMIT 100) r)",
      user);
}
void TestPlayRepository::Start(Tx& tx, const Id& user, const Id& test) const {
  StudentAccess(tx, user, test);
  auto r = tx.Execute(
      R"(INSERT INTO test.test_in_process(test_id,student_id,current_question_id,started_at,deadline_at)
    SELECT t.id,$2,q.id,v.now,v.now+t.time_to_complete*interval '1 second'
    FROM test.tests t JOIN test.questions q ON q.test_id=t.id AND q.position=0
    CROSS JOIN LATERAL(SELECT clock_timestamp() AS now) v WHERE t.id=$1
    ON CONFLICT(test_id,student_id) DO NOTHING RETURNING id)",
      test, user);
  if (r.IsEmpty() && tx.Execute("SELECT id FROM test.test_in_process WHERE "
                                "test_id=$1 AND student_id=$2",
                                test, user)
                         .IsEmpty())
    throw TestPlayError("test_not_ready");
}
userver::formats::json::Value TestPlayRepository::Read(Tx& tx, const Id& user,
                                                       const Id& test) const {
  StudentAccess(tx, user, test);
  auto p = Lock(tx, user, test);
  auto state = Json(tx, R"(SELECT jsonb_build_object(
    'test',jsonb_build_object('id',t.id,'name',t.name,'time_to_complete',t.time_to_complete),
    'attempt',jsonb_build_object('id',p.id,'status',p.status,
    'started_at_ms',(extract(epoch from p.started_at)*1000)::bigint,
    'deadline_at_ms',(extract(epoch from p.deadline_at)*1000)::bigint),
    'server_now_ms',(extract(epoch from clock_timestamp())*1000)::bigint,
    'question_count',(SELECT count(*) FROM test.questions WHERE test_id=t.id),
    'answered_count',(SELECT count(*) FROM test.student_answers WHERE process_id=p.id),
    'current_question',(SELECT jsonb_build_object('id',q.id,'position',q.position,'text',q.text,'type',q.type,
      'image_id',q.image_id,'image_key',m.storage_key,'answers',COALESCE((SELECT jsonb_agg(jsonb_build_object(
      'id',a.id,'text',a.text,'image_id',a.image_id,'image_key',am.storage_key) ORDER BY a.position)
      FROM test.answers a LEFT JOIN media.images am ON am.id=a.image_id WHERE a.question_id=q.id),'[]'::jsonb))
      FROM test.questions q LEFT JOIN media.images m ON m.id=q.image_id WHERE q.id=p.current_question_id))::text
    FROM test.test_in_process p JOIN test.tests t ON t.id=p.test_id WHERE p.id=$1)",
                    p.id);
  userver::formats::json::ValueBuilder out(state);
  out["success"] = true;
  if (p.status != "in_progress")
    out["result"] = Json(
        tx, "SELECT to_jsonb(r)::text FROM (" + kScores + " WHERE p.id=$1) r",
        p.id);
  return out.ExtractValue();
}
bool TestPlayRepository::Submit(Tx& tx, const Id& user, const Id& test,
                                const Id& question,
                                std::vector<Id> choices) const {
  StudentAccess(tx, user, test);
  std::sort(choices.begin(), choices.end());
  if (choices.empty() || choices.size() > 20 ||
      std::adjacent_find(choices.begin(), choices.end()) != choices.end())
    throw TestPlayError("invalid_request");
  auto p = Lock(tx, user, test);
  auto old = tx.Execute(
      "SELECT id FROM test.student_answers WHERE process_id=$1 AND "
      "question_id=$2",
      p.id, question);
  if (!old.IsEmpty()) {
    auto saved =
        tx.Execute(
              "SELECT answer_id FROM test.student_answer_choices WHERE "
              "student_answer_id=$1 ORDER BY answer_id",
              old.AsSingleRow<Id>())
            .AsContainer<std::vector<Id>>();
    if (saved != choices) throw TestPlayError("test_answer_already_saved");
    return true;
  }
  if (p.status != "in_progress") return false;
  if (p.question != question) throw TestPlayError("test_question_changed");
  auto type =
      tx.Execute("SELECT type FROM test.questions WHERE id=$1", question)
          .AsSingleRow<std::string>();
  if ((type == "single" && choices.size() != 1) ||
      tx.Execute("SELECT id FROM test.answers WHERE question_id=$1 AND "
                 "id=ANY($2::uuid[])",
                 question, choices)
              .Size() != choices.size())
    throw TestPlayError("invalid_request");
  // Recheck the deadline immediately before writing, after all validation
  // queries.
  if (!tx.Execute(
             R"(UPDATE test.test_in_process SET status='expired',current_question_id=NULL,finished_at=clock_timestamp()
     WHERE id=$1 AND deadline_at<=clock_timestamp() RETURNING id)",
             p.id)
           .IsEmpty())
    return false;
  auto submission = boost::uuids::random_generator{}();
  tx.Execute(
      "INSERT INTO test.student_answers(id,process_id,test_id,question_id) "
      "VALUES($1,$2,$3,$4)",
      submission, p.id, test, question);
  tx.Execute(
      "INSERT INTO "
      "test.student_answer_choices(student_answer_id,question_id,answer_id) "
      "SELECT "
      "$1,$2,unnest($3::uuid[])",
      submission, question, choices);
  auto next = tx.Execute(
      R"(SELECT n.id FROM test.questions n JOIN test.questions q ON q.test_id=n.test_id
      WHERE q.id=$1 AND n.position=q.position+1)",
      question);
  if (next.IsEmpty())
    tx.Execute(
        "UPDATE test.test_in_process SET "
        "current_question_id=NULL,status='completed',finished_at=clock_"
        "timestamp() WHERE id=$1",
        p.id);
  else
    tx.Execute(
        "UPDATE test.test_in_process SET current_question_id=$2 WHERE id=$1",
        p.id, next.AsSingleRow<Id>());
  return true;
}
userver::formats::json::Value TestPlayRepository::Results(
    Tx& tx, const Id& user, const Id& test) const {
  if (tx.Execute(
            R"(SELECT t.id FROM test.tests t WHERE t.id=$1 AND t.author_id=$2 AND EXISTS(
     SELECT 1 FROM education.memberships m WHERE m.user_id=$2 AND m.university_id=t.university_id
     AND m.status='active' AND m.role IN ('teacher','admin')))",
            test, user)
          .IsEmpty())
    throw TestPlayError("test_not_found");
  // Acquire locks in a stable order before marking overdue attempts.
  tx.Execute(
      "SELECT id FROM test.test_in_process WHERE test_id=$1 ORDER BY id FOR "
      "UPDATE",
      test);
  tx.Execute(
      "UPDATE test.test_in_process SET "
      "status='expired',current_question_id=NULL,finished_at=clock_timestamp() "
      "WHERE test_id=$1 AND status='in_progress' AND "
      "deadline_at<=clock_timestamp()",
      test);
  return Json(
      tx,
      "SELECT COALESCE(jsonb_agg(to_jsonb(r) || jsonb_build_object('name',"
      "COALESCE(NULLIF(btrim(concat_ws(' "
      "',pr.last_name,pr.first_name,pr.middle_name)),''),'Участник')) "
      "ORDER BY r.started_at DESC,r.id),'[]'::jsonb)::text FROM (" +
          kScores +
          " WHERE p.test_id=$1) r LEFT JOIN users.profiles pr ON "
          "pr.user_id=r.student_id",
      test);
}
}  // namespace RumpelQuiz
