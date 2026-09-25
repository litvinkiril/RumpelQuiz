#pragma once
#include <string>

namespace RumpelQuiz {
struct SaveImageResponse {
  bool success = false;
  std::string error_message;
  // Object address; access still depends on bucket permissions.
  std::string url;
};
}  // namespace RumpelQuiz
