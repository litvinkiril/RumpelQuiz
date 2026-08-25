#pragma once

#include <string>
#include <string_view>

namespace RumpelQuiz {

enum class UserType { kFirstTime, kKnown };

std::string SayHelloTo(std::string_view name, UserType type);

}  // namespace RumpelQuiz