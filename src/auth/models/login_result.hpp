#pragma once

#include <variant>

#include <boost/uuid/uuid.hpp>

namespace RumpelQuiz {

struct LoginSuccess {
  boost::uuids::uuid user_id;
};

enum class LoginError { kEmailDoesnotExist, kPasswordsDoNotMatch};

using LoginResult = std::variant<LoginSuccess, LoginError>;
}  // namespace RumpelQuiz
