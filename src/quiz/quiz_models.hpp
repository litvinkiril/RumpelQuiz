#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include <boost/uuid/uuid.hpp>

namespace RumpelQuiz {

enum class QuizStatus {
    kDraft,
    kReady
};

enum class QuestionType {
    kSingle,
    kMulty
};

// Вариант ответа: текст, картинка или оба.
// Для черновика разрешено оставить оба поля пустыми.
struct QuizAnswerInput {
    std::string text;
    bool is_correct = false;
    std::optional<boost::uuids::uuid> image_id;
};

struct QuizQuestionInput {
    std::string text;
    QuestionType type = QuestionType::kSingle;

    // nullopt — использовать default_time_seconds квиза.
    std::optional<std::int32_t> time_seconds;

    std::optional<boost::uuids::uuid> image_id;
    std::vector<QuizAnswerInput> answers;
};

struct SaveQuizRequest {
    boost::uuids::uuid university_id{};

    std::string name;
    std::string description;

    std::int32_t default_time_seconds = 60;
    QuizStatus status = QuizStatus::kDraft;

    // При создании отсутствует.
    // При обновлении — версия, которую получил фронт.
    std::optional<std::int64_t> revision;

    std::vector<QuizQuestionInput> questions;
};

// Ошибка конкретного поля для отображения на фронте.
struct QuizFieldError {
    // Например: questions[0].answers[1].text
    std::string field;
    std::string message;
};

// Ошибка структуры JSON или типов полей.
struct QuizParseError {
    std::vector<QuizFieldError> details;
};

using ParseSaveQuizResult =
    std::variant<SaveQuizRequest, QuizParseError>;

enum class SaveQuizError {
    kAccessDenied,
    kNotFound,
    kRevisionConflict,
    kPublished,
    kValidationFailed
};

struct SaveQuizFailure {
    SaveQuizError code;
    std::vector<QuizFieldError> details;
};

struct SaveQuizSuccess {
    boost::uuids::uuid quiz_id;
    QuizStatus status;
    std::int64_t revision;
};

using SaveQuizResult =
    std::variant<SaveQuizSuccess, SaveQuizFailure>;

}  // namespace RumpelQuiz
