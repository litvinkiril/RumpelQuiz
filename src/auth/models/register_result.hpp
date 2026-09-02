#pragma once

#include <variant>

#include <boost/uuid/uuid.hpp>

namespace RumpelQuiz {

struct RegistrationSuccess {
    boost::uuids::uuid verification_id;
};

enum class RegisterError {
    kPasswordsDoNotMatch,
    kEmailAlreadyExists
};

using RegisterResult =
    std::variant<RegistrationSuccess, RegisterError>;

}
