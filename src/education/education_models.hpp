#pragma once
#include <boost/uuid/uuid.hpp>
#include <optional>
#include <string>
#include <variant>
#include <vector>

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

struct Position {
    boost::uuids::uuid university_id;
    std::string university_name;
    std::string role;
    std::optional<boost::uuids::uuid> group_id;
    std::optional<std::string> group_name;
};

struct UniversityAdmin {
    boost::uuids::uuid membership_id;
    std::optional<std::string> first_name;
    std::optional<std::string> last_name;
    std::optional<std::string> middle_name;
    std::optional<std::string> avatar_url;
    std::string email;
};

enum class GetUniversityAdminsError {
    kAccessDenied
};

using GetUniversityAdminsResult = std::variant<
    std::vector<UniversityAdmin>,
    GetUniversityAdminsError
>;
}  // namespace RumpelQuiz
