#pragma once

#include <optional>
#include <string>
#include <variant>

#include <boost/uuid/uuid.hpp>

namespace RumpelQuiz {

struct CreateUserRequest {
  std::string email;
  std::string password;

  std::string first_name;
  std::string last_name;
  std::optional<std::string> middle_name;
  std::optional<std::string> description;
  boost::uuids::uuid university_id;
  std::string role;
  std::optional<boost::uuids::uuid> facultet_id;
};

struct CreateUserSuccess {
  boost::uuids::uuid user_id;
};

enum class CreateUserError {
  kInvalidRequest,
  kAccessDenied,
  kEmailAlreadyExists,
  kUniversityNotFound,
  kInvalidFaculty
};

using CreateUserResult = std::variant<CreateUserSuccess, CreateUserError>;

}  // namespace RumpelQuiz