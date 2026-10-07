#pragma once
#include "test/test_models.hpp"

namespace RumpelQuiz {
class TestValidation {
 public:
  std::vector<TestFieldError> CheckCommon(const SaveTestRequest& input) const;
  std::vector<TestFieldError> CheckReady(const SaveTestRequest& input) const;
};
}  // namespace RumpelQuiz
