#pragma once

#include <boost/uuid/uuid.hpp>
#include <variant>

namespace RumpelQuiz {

struct RegistrationSuccess {
  boost::uuids::uuid verification_id;
};

enum class RegisterError {
  kInvalidRequest,
  kPasswordsDoNotMatch,
  kEmailAlreadyExists
};

using RegisterResult = std::variant<RegistrationSuccess, RegisterError>;
}  // namespace RumpelQuiz
