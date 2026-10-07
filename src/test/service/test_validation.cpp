#include "test_validation.hpp"
#include <unicode/uchar.h>
#include <unicode/utf8.h>
#include <algorithm>
#include <limits>
namespace RumpelQuiz {
namespace {
bool HasText(const std::string& text) {
  int32_t i = 0;
  while (i < static_cast<int32_t>(text.size())) {
    UChar32 c;
    U8_NEXT(text.data(), i, static_cast<int32_t>(text.size()), c);
    if (c >= 0 && !u_isUWhiteSpace(c)) return true;
  }
  return false;
}
void Text(std::vector<TestFieldError>& errors, const std::string& path,
          const std::string& value, std::size_t limit) {
  if (value.size() > limit || value.find('\0') != std::string::npos)
    errors.push_back({path, "Недопустимый размер или содержимое текста."});
}
}  // namespace
std::vector<TestFieldError> TestValidation::CheckCommon(
    const SaveTestRequest& in) const {
  std::vector<TestFieldError> e;
  if (in.university_id.is_nil())
    e.push_back({"university_id", "Выберите вуз."});
  if (in.status != TestStatus::kDraft && in.status != TestStatus::kPublic &&
      in.status != TestStatus::kPrivate)
    e.push_back({"status", "Некорректный статус."});
  if (in.time_to_complete <= 0)
    e.push_back({"time_to_complete", "Время должно быть больше нуля."});
  if (in.revision && (*in.revision <= 0 ||
                      *in.revision == std::numeric_limits<std::int64_t>::max()))
    e.push_back({"revision", "Некорректная версия."});
  Text(e, "name", in.name, 1000);
  Text(e, "description", in.description, 20000);
  if (in.questions.size() > 100)
    e.push_back({"questions", "Не более 100 вопросов."});
  for (std::size_t i = 0; i < in.questions.size(); ++i) {
    const auto& q = in.questions[i];
    const auto p = "questions[" + std::to_string(i) + "]";
    Text(e, p + ".text", q.text, 20000);
    if (q.type != QuestionType::kSingle && q.type != QuestionType::kMulty)
      e.push_back({p + ".type", "Некорректный тип."});
    if (q.answers.size() > 20)
      e.push_back({p + ".answers", "Не более 20 ответов."});
    for (std::size_t j = 0; j < q.answers.size(); ++j)
      Text(e, p + ".answers[" + std::to_string(j) + "].text", q.answers[j].text,
           10000);
  }
  return e;
}
std::vector<TestFieldError> TestValidation::CheckReady(
    const SaveTestRequest& in) const {
  std::vector<TestFieldError> e;
  if (!HasText(in.name)) e.push_back({"name", "Введите название теста."});
  if (in.questions.empty())
    e.push_back({"questions", "Добавьте хотя бы один вопрос."});
  for (std::size_t i = 0; i < in.questions.size(); ++i) {
    const auto& q = in.questions[i];
    const auto p = "questions[" + std::to_string(i) + "]";
    if (!HasText(q.text)) e.push_back({p + ".text", "Введите текст вопроса."});
    if (q.answers.size() < 2)
      e.push_back({p + ".answers", "Добавьте минимум два ответа."});
    for (std::size_t j = 0; j < q.answers.size(); ++j)
      if (!HasText(q.answers[j].text) && !q.answers[j].image_id)
        e.push_back({p + ".answers[" + std::to_string(j) + "]",
                     "Добавьте текст или картинку."});
    const auto count =
        std::count_if(q.answers.begin(), q.answers.end(),
                      [](const auto& a) { return a.is_correct; });
    if ((q.type == QuestionType::kSingle && count != 1) ||
        (q.type == QuestionType::kMulty && count == 0))
      e.push_back(
          {p + ".answers", q.type == QuestionType::kSingle
                               ? "Выберите ровно один правильный ответ."
                               : "Выберите хотя бы один правильный ответ."});
  }
  return e;
}
}  // namespace RumpelQuiz
