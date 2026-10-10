#pragma once
#include <boost/uuid/uuid.hpp>
#include <optional>
#include <string>
#include <variant>
#include <vector>
#include "education/education_models.hpp"

namespace RumpelQuiz {


struct UserWithProfile {
    std::string email;
    bool email_verified;
    std::optional<boost::uuids::uuid> profile_user_id;
    std::optional<std::string> first_name;
    std::optional<std::string> last_name;
    std::optional<std::string> middle_name;
    std::optional<std::string> avatar_url;
    std::optional<std::string> description;
};

struct UserProfile {
    boost::uuids::uuid user_id;
    std::optional<std::string> first_name;
    std::optional<std::string> last_name;
    std::optional<std::string> middle_name;
    std::optional<std::string> avatar_url;
    std::optional<std::string> description;
};
struct FullProfile {
    std::string email;
    std::optional<std::string> first_name;
    std::optional<std::string> last_name;
    std::optional<std::string> middle_name;
    std::optional<std::string> avatar_url;
    std::optional<std::string> description;
    std::vector<Position> university_position;
};
enum class GetProfileError {
    kUserNotFound,
    kEmailNotVerified,
    kUserProfileNotFound
};
using GetProfileResult = std::variant<FullProfile, GetProfileError>;
}  // namespace RumpelQuiz
