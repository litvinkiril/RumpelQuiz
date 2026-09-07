#pragma once

#include <boost/uuid/uuid.hpp>
#include <variant>

namespace RumpelQuiz {

struct ResendCodeSuccess {
  boost::uuids::uuid verification_id;
};

enum class ResendCodeError { kVerificationNotFound, kTooSoon };

using ResendCodeResult = std::variant<ResendCodeSuccess, ResendCodeError>;

}  // namespace RumpelQuiz
