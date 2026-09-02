#pragma once

#include <string>

#include <boost/uuid/uuid.hpp>

namespace RumpelQuiz {

struct UserData {
    boost::uuids::uuid id;
    std::string email;
    std::string password_hash;
    bool email_verified;
};
}