#pragma once
#include <string>
#include <userver/formats/json/value.hpp>
#include "quiz/quiz_models.hpp"
namespace RumpelQuiz {
ParseSaveQuizResult ParseSaveQuizRequest(
    const userver::formats::json::Value& body);
std::string SerializeQuizStatus(QuizStatus status);
}  // namespace RumpelQuiz
