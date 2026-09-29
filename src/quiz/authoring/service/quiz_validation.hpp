#pragma once
#include "quiz/quiz_models.hpp"
namespace RumpelQuiz {
class QuizValidation {
 public:
  std::vector<QuizFieldError> CheckCommon(const SaveQuizRequest& input) const;
  std::vector<QuizFieldError> CheckReady(const SaveQuizRequest& input) const;
};
}  // namespace RumpelQuiz
