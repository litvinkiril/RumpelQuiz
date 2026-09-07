#pragma once

#include <boost/uuid/uuid.hpp>
#include <variant>

namespace RumpelQuiz {

struct EmailVerifySuccess {
  boost::uuids::uuid user_id;
};

enum class EmailVerifyError {
  kVerificationNotFound,
  kCodeDoesNotMatch,
  kCodeExpired
};

using EmailVerifyResult = std::variant<EmailVerifySuccess, EmailVerifyError>;
}  // namespace RumpelQuiz
