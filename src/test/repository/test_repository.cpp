#include "test_repository.hpp"
#include <boost/uuid/random_generator.hpp>
#include <stdexcept>
#include <userver/formats/json.hpp>
#include <userver/storages/postgres/io/array_types.hpp>
#include <userver/storages/postgres/io/optional.hpp>
#include <userver/storages/postgres/io/row_types.hpp>
#include <userver/storages/postgres/io/uuid.hpp>
namespace RumpelQuiz {
namespace {
struct QuestionRow {
  boost::uuids::uuid id, test_id;
  std::string text, type;
  std::optional<boost::uuids::uuid> image;
  std::int32_t position;
};
struct AnswerRow {
  boost::uuids::uuid id, question_id;
  std::string text;
  bool correct;
  std::optional<boost::uuids::uuid> image;
  std::int32_t position;
};
}  // namespace
std::optional<StoredTest> TestRepository::FindOwnedForUpdate(
    userver::storages::postgres::Transaction& tx, const boost::uuids::uuid& id,
    const boost::uuids::uuid& owner) const {
  auto r = tx.Execute(
      "SELECT university_id,revision,status FROM test.tests WHERE id=$1 AND "
      "author_id=$2 FOR UPDATE",
      id, owner);
  if (r.IsEmpty()) return std::nullopt;
  return r.AsSingleRow<StoredTest>(userver::storages::postgres::kRowTag);
}
bool TestRepository::CheckImages(
    userver::storages::postgres::Transaction& tx,
    const boost::uuids::uuid& owner,
    const std::vector<boost::uuids::uuid>& ids) const {
  if (ids.empty()) return true;
  return tx.Execute(
               "SELECT id FROM media.images WHERE id=ANY($1::uuid[]) AND "
               "owner_id=$2 AND status='ready' ORDER BY id FOR SHARE",
               ids, owner)
             .Size() == ids.size();
}
std::int64_t TestRepository::Save(userver::storages::postgres::Transaction& tx,
                                  const boost::uuids::uuid& id,
                                  const boost::uuids::uuid& owner,
                                  const SaveTestRequest& in,
                                  bool create) const {
  const std::string status = in.status == TestStatus::kDraft    ? "draft"
                             : in.status == TestStatus::kPublic ? "public"
                                                                : "private";
  if (create) {
    tx.Execute(
        "INSERT INTO "
        "test.tests(id,author_id,university_id,name,description,time_to_"
        "complete,status) VALUES($1,$2,$3,$4,$5,$6,$7)",
        id, owner, in.university_id, in.name, in.description,
        in.time_to_complete, status);
    return 1;
  }
  auto r = tx.Execute(
      "UPDATE test.tests SET "
      "university_id=$3,name=$4,description=$5,time_to_complete=$6,status=$"
      "7,revision=revision+1 WHERE id=$1 AND "
      "author_id=$2 AND revision=$8 RETURNING revision",
      id, owner, in.university_id, in.name, in.description, in.time_to_complete,
      status, in.revision.value());
  if (r.IsEmpty()) throw std::logic_error("Locked test unexpectedly changed");
  return r.AsSingleRow<std::int64_t>();
}
void TestRepository::ReplaceQuestions(
    userver::storages::postgres::Transaction& tx, const boost::uuids::uuid& id,
    const SaveTestRequest& in) const {
  tx.Execute("DELETE FROM test.questions WHERE test_id=$1", id);
  boost::uuids::random_generator generate;
  std::vector<QuestionRow> qs;
  std::vector<AnswerRow> as;
  for (std::size_t i = 0; i < in.questions.size(); ++i) {
    const auto& q = in.questions[i];
    const auto qid = generate();
    qs.push_back({qid, id, q.text,
                  q.type == QuestionType::kSingle ? "single" : "multy",
                  q.image_id, static_cast<std::int32_t>(i)});
    for (std::size_t j = 0; j < q.answers.size(); ++j) {
      const auto& a = q.answers[j];
      as.push_back({generate(), qid, a.text, a.is_correct, a.image_id,
                    static_cast<std::int32_t>(j)});
    }
  }
  if (!qs.empty())
    tx.ExecuteDecompose(
        "INSERT INTO "
        "test.questions(id,test_id,text,type,image_id,position) "
        "SELECT * FROM "
        "UNNEST($1::uuid[],$2::uuid[],$3::text[],$4::text[],$5::uuid[],$6::"
        "integer[])",
        qs);
  if (!as.empty())
    tx.ExecuteDecompose(
        "INSERT INTO "
        "test.answers(id,question_id,text,is_correct,image_id,position) SELECT "
        "* FROM "
        "UNNEST($1::uuid[],$2::uuid[],$3::text[],$4::boolean[],$5::uuid[],$6::"
        "integer[])",
        as);
}
userver::formats::json::Value TestRepository::Read(
    userver::storages::postgres::Transaction& tx, const boost::uuids::uuid& id,
    const boost::uuids::uuid& owner) const {
  auto r = tx.Execute(R"(
 SELECT jsonb_build_object('id',k.id,'university_id',k.university_id,'name',k.name,'description',k.description,
 'time_to_complete',k.time_to_complete,'status',k.status,'revision',k.revision,
 'questions',COALESCE((SELECT jsonb_agg(jsonb_build_object('text',q.text,'type',q.type,
 'image_id',q.image_id,'image_key',m.storage_key,'answers',COALESCE((SELECT jsonb_agg(jsonb_build_object(
 'text',a.text,'is_correct',a.is_correct,'image_id',a.image_id,'image_key',am.storage_key) ORDER BY a.position)
 FROM test.answers a LEFT JOIN media.images am ON am.id=a.image_id WHERE a.question_id=q.id),'[]'::jsonb)) ORDER BY q.position)
 FROM test.questions q LEFT JOIN media.images m ON m.id=q.image_id WHERE q.test_id=k.id),'[]'::jsonb))::text
 FROM test.tests k WHERE k.id=$1 AND k.author_id=$2
 AND EXISTS(SELECT 1 FROM education.memberships e WHERE e.user_id=$2 AND e.university_id=k.university_id AND e.status='active' AND e.role IN ('teacher','admin'))
 )",
                      id, owner);
  if (r.IsEmpty()) return userver::formats::json::ValueBuilder{}.ExtractValue();
  return userver::formats::json::FromString(r.AsSingleRow<std::string>());
}
userver::formats::json::Value TestRepository::List(
    userver::storages::postgres::Transaction& tx,
    const boost::uuids::uuid& owner) const {
  return userver::formats::json::FromString(tx.Execute(R"(
 SELECT COALESCE(jsonb_agg(to_jsonb(t) ORDER BY t.created_at DESC,t.id),'[]'::jsonb)::text FROM (
 SELECT k.id,k.name,k.status,k.revision,k.university_id,u.name AS university_name,k.created_at
 FROM test.tests k JOIN education.universities u ON u.id=k.university_id WHERE k.author_id=$1
 AND EXISTS(SELECT 1 FROM education.memberships e WHERE e.user_id=$1 AND e.university_id=k.university_id AND e.status='active' AND e.role IN ('teacher','admin'))
 ORDER BY k.created_at DESC,k.id LIMIT 100) t
 )",
                                                       owner)
                                                .AsSingleRow<std::string>());
}
}  // namespace RumpelQuiz
