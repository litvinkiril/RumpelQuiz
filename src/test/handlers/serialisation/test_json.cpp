#include "test_json.hpp"
#include <boost/uuid/string_generator.hpp>
#include <stdexcept>
#include <userver/formats/json.hpp>
#include <utility>
namespace RumpelQuiz {
namespace {
using Json = userver::formats::json::Value;
struct BadField {
  std::string message;
};
void Require(bool ok, const char* message) {
  if (!ok) throw BadField{message};
}
boost::uuids::uuid Uuid(const Json& value) {
  const auto text = value.As<std::string>();
  boost::uuids::uuid id;
  try {
    id = boost::uuids::string_generator{}(text);
  } catch (const std::runtime_error&) {
    throw BadField{"Некорректный UUID."};
  }
  Require(!id.is_nil(), "UUID не должен быть пустым.");
  return id;
}
std::optional<boost::uuids::uuid> Image(const Json& value) {
  if (value.IsMissing() || value.IsNull()) return std::nullopt;
  return Uuid(value);
}
}  // namespace
ParseSaveTestResult ParseSaveTestRequest(const Json& body) {
  std::string field = "$";
  try {
    Require(body.IsObject(), "Ожидается объект.");
    SaveTestRequest input;
    field = "university_id";
    input.university_id = Uuid(body[field]);
    field = "name";
    input.name = body[field].As<std::string>("");
    field = "description";
    input.description = body[field].As<std::string>("");
    field = "time_to_complete";
    input.time_to_complete = body[field].As<std::int32_t>();
    field = "status";
    const auto status = body[field].As<std::string>();
    Require(status == "draft" || status == "public" || status == "private",
            "Допустимы draft, public и private.");
    input.status = status == "draft"    ? TestStatus::kDraft
                   : status == "public" ? TestStatus::kPublic
                                        : TestStatus::kPrivate;
    field = "revision";
    if (!body[field].IsMissing() && !body[field].IsNull())
      input.revision = body[field].As<std::int64_t>();
    field = "questions";
    const auto questions = body[field];
    if (!questions.IsMissing()) {
      Require(questions.IsArray(), "Ожидается массив.");
      Require(questions.GetSize() <= 100, "Не более 100 вопросов.");
      for (std::size_t i = 0; i < questions.GetSize(); ++i) {
        const auto prefix = "questions[" + std::to_string(i) + "]";
        field = prefix;
        const auto item = questions[i];
        Require(item.IsObject(), "Ожидается объект.");
        TestQuestionInput q;
        field = prefix + ".text";
        q.text = item["text"].As<std::string>("");
        field = prefix + ".type";
        const auto type = item["type"].As<std::string>("single");
        Require(type == "single" || type == "multy",
                "Допустимы single и multy.");
        q.type =
            type == "single" ? QuestionType::kSingle : QuestionType::kMulty;
        field = prefix + ".time_seconds";
        Require(item["time_seconds"].IsMissing(),
                "Время задаётся только для всего теста.");
        field = prefix + ".image_id";
        q.image_id = Image(item["image_id"]);
        field = prefix + ".answers";
        const auto answers = item["answers"];
        if (!answers.IsMissing()) {
          Require(answers.IsArray(), "Ожидается массив.");
          Require(answers.GetSize() <= 20, "Не более 20 ответов.");
          for (std::size_t j = 0; j < answers.GetSize(); ++j) {
            const auto ap = prefix + ".answers[" + std::to_string(j) + "]";
            field = ap;
            const auto a = answers[j];
            Require(a.IsObject(), "Ожидается объект.");
            TestAnswerInput answer;
            field = ap + ".text";
            answer.text = a["text"].As<std::string>("");
            field = ap + ".is_correct";
            answer.is_correct = a["is_correct"].As<bool>(false);
            field = ap + ".image_id";
            answer.image_id = Image(a["image_id"]);
            q.answers.push_back(std::move(answer));
          }
        }
        input.questions.push_back(std::move(q));
      }
    }
    return input;
  } catch (const BadField& e) {
    return TestParseError{{{field, e.message}}};
  } catch (const userver::formats::json::Exception&) {
    return TestParseError{
        {{field, "Поле отсутствует или имеет неверный тип."}}};
  }
}
std::string SerializeTestStatus(TestStatus status) {
  switch (status) {
    case TestStatus::kDraft:
      return "draft";
    case TestStatus::kPublic:
      return "public";
    case TestStatus::kPrivate:
      return "private";
  }
  throw std::logic_error("Invalid test status");
}
}  // namespace RumpelQuiz
