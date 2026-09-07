#pragma once

#include <boost/uuid/uuid.hpp>
#include <string>
#include <variant>

namespace RumpelQuiz {

struct ForgotPasswordEmailSuccess {
  boost::uuids::uuid verification_id;
};

enum class ForgotPasswordEmailError {
  kEmailNotFound,
  kEmailNotVerified,
  kTooSoon
};

using ForgotPasswordEmailResult =
    std::variant<ForgotPasswordEmailSuccess, ForgotPasswordEmailError>;

struct ForgotPasswordVerifySuccess {
  std::string reset_token;
};

enum class ForgotPasswordVerifyError {
  kVerificationNotFound,
  kCodeExpired,
  kInvalidCode
};

using ForgotPasswordVerifyResult =
    std::variant<ForgotPasswordVerifySuccess, ForgotPasswordVerifyError>;

struct ForgotPasswordUpdateSuccess {
  boost::uuids::uuid user_id;
};

enum class ForgotPasswordUpdateError {
  kResetTokenNotFound,
  kResetTokenExpired,
  kInvalidPassword,
  kPasswordsDoNotMatch
};

using ForgotPasswordUpdateResult =
    std::variant<ForgotPasswordUpdateSuccess, ForgotPasswordUpdateError>;

}  // namespace RumpelQuiz
