#pragma once
#include <boost/uuid/uuid.hpp>
#include <string>

namespace RumpelQuiz {
struct University {
    boost::uuids::uuid id;
    std::string name;
};
struct StudyGroup {
    boost::uuids::uuid id;
    boost::uuids::uuid university_id;
    std::string name;
};
struct Membership {
    boost::uuids::uuid id;
    boost::uuids::uuid user_id;
    boost::uuids::uuid university_id;
    std::string role;
    std::string status;
};
struct StudentGroup {
    boost::uuids::uuid membership_id;
    boost::uuids::uuid university_id;
    boost::uuids::uuid group_id;
    std::string name;
};
}  // namespace RumpelQuiz
