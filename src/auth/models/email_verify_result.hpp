#pragma once

#include <variant>

#include <boost/uuid/uuid.hpp>

namespace RumpelQuiz {

struct EmailVerifySuccess {
  boost::uuids::uuid user_id;
};

enum class EmailVerifyError { kCodeDoNotMatch, kCodeHasExpire };

using EmailVerifyResult = std::variant<EmailVerifySuccess, EmailVerifyError>;
}  // namespace RumpelQuiz
