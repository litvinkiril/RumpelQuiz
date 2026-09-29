#include "quiz_json.hpp"
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
ParseSaveQuizResult ParseSaveQuizRequest(const Json& body) {
  std::string field = "$";
  try {
    Require(body.IsObject(), "Ожидается объект.");
    SaveQuizRequest input;
    field = "university_id";
    input.university_id = Uuid(body[field]);
    field = "name";
    input.name = body[field].As<std::string>("");
    field = "description";
    input.description = body[field].As<std::string>("");
    field = "default_time_seconds";
    input.default_time_seconds = body[field].As<std::int32_t>(60);
    field = "status";
    const auto status = body[field].As<std::string>();
    Require(status == "draft" || status == "ready", "Допустимы draft и ready.");
    input.status = status == "draft" ? QuizStatus::kDraft : QuizStatus::kReady;
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
        QuizQuestionInput q;
        field = prefix + ".text";
        q.text = item["text"].As<std::string>("");
        field = prefix + ".type";
        const auto type = item["type"].As<std::string>("single");
        Require(type == "single" || type == "multy",
                "Допустимы single и multy.");
        q.type =
            type == "single" ? QuestionType::kSingle : QuestionType::kMulty;
        field = prefix + ".time_seconds";
        if (!item["time_seconds"].IsMissing() && !item["time_seconds"].IsNull())
          q.time_seconds = item["time_seconds"].As<std::int32_t>();
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
            QuizAnswerInput answer;
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
    return QuizParseError{{{field, e.message}}};
  } catch (const userver::formats::json::Exception&) {
    return QuizParseError{
        {{field, "Поле отсутствует или имеет неверный тип."}}};
  }
}
std::string SerializeQuizStatus(QuizStatus status) {
  switch (status) {
    case QuizStatus::kDraft:
      return "draft";
    case QuizStatus::kReady:
      return "ready";
  }
  throw std::logic_error("Invalid quiz status");
}
}  // namespace RumpelQuiz
