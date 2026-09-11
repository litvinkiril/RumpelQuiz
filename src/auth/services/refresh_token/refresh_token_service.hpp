#pragma once

#include <string>
#include <string_view>

namespace RumpelQuiz {

class RefreshTokenService {
 public:
  std::string Generate() const;
  std::string Hash(std::string_view token) const;
  bool Verify(std::string_view token, std::string_view token_hash) const;
};

}  // namespace RumpelQuiz
