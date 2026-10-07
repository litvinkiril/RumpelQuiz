#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include <boost/uuid/uuid.hpp>
#include <quiz/quiz_models.hpp>

namespace RumpelQuiz {

enum class TestStatus { kDraft, kPublic, kPrivate };

// Вариант ответа: текст, картинка или оба.
// Для черновика разрешено оставить оба поля пустыми.
struct TestAnswerInput {
  std::string text;
  bool is_correct = false;
  std::optional<boost::uuids::uuid> image_id;
};

struct TestQuestionInput {
  std::string text;
  QuestionType type = QuestionType::kSingle;

  std::optional<boost::uuids::uuid> image_id;
  std::vector<TestAnswerInput> answers;
};

struct SaveTestRequest {
  boost::uuids::uuid university_id{};

  std::string name;
  std::string description;

  std::int32_t time_to_complete = 60;
  TestStatus status = TestStatus::kDraft;

  // При создании отсутствует.
  // При обновлении — версия, которую получил фронт.
  std::optional<std::int64_t> revision;

  std::vector<TestQuestionInput> questions;
};

// Ошибка конкретного поля для отображения на фронте.
struct TestFieldError {
  // Например: questions[0].answers[1].text
  std::string field;
  std::string message;
};

// Ошибка структуры JSON или типов полей.
struct TestParseError {
  std::vector<TestFieldError> details;
};

using ParseSaveTestResult = std::variant<SaveTestRequest, TestParseError>;

enum class SaveTestError {
  kAccessDenied,
  kNotFound,
  kRevisionConflict,
  kPublished,
  kValidationFailed
};

struct SaveTestFailure {
  SaveTestError code;
  std::vector<TestFieldError> details;
};

struct SaveTestSuccess {
  boost::uuids::uuid test_id;
  TestStatus status;
  std::int64_t revision;
};

using SaveTestResult = std::variant<SaveTestSuccess, SaveTestFailure>;

}  // namespace RumpelQuiz