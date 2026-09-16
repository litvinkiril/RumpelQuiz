#pragma once
#include <boost/uuid/uuid.hpp>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace RumpelQuiz {
struct Position {
    std::string university_name;
    std::string role;
    std::optional<std::string> group_name;
};
struct UserProfile {
    boost::uuids::uuid user_id;
    std::optional<std::string> first_name;
    std::optional<std::string> last_name;
    std::optional<std::string> middle_name;
    std::optional<std::string> avatar_url;
};
struct FullProfile {
    std::string email;
    std::optional<std::string> first_name;
    std::optional<std::string> last_name;
    std::optional<std::string> middle_name;
    std::optional<std::string> avatar_url;
    std::vector<Position> university_position;
};
enum class GetProfileError {
    kUserNotFound,
    kEmailNotVerified,
    kUserProfileNotFound,
    kUniversitiesNotFound
};
using GetProfileResult = std::variant<FullProfile, GetProfileError>;
}  // namespace RumpelQuiz
