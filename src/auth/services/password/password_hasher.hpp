#pragma once

#include <string>
#include <string_view>

namespace RumpelQuiz {

class PasswordHasher {
 public:
  std::string HashPassword(std::string_view password) const;

  bool VerifyPassword(std::string_view password,
                      std::string_view password_hash) const;
};

}  // namespace RumpelQuiz
