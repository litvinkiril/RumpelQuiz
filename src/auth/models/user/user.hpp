#pragma once

#include <boost/uuid/uuid.hpp>
#include <string>

namespace RumpelQuiz {

struct UserData {
  boost::uuids::uuid id;
  std::string email;
  std::string password_hash;
  bool email_verified;
};
}  // namespace RumpelQuiz
