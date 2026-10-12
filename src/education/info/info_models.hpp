#pragma once

#include <optional>
#include <string>
#include <vector>

#include <boost/uuid/uuid.hpp>

namespace RumpelQuiz {

struct GroupMember {
    boost::uuids::uuid id;
    std::optional<std::string> first_name;
    std::optional<std::string> last_name;
    std::optional<std::string> middle_name;
    std::optional<std::string> avatar_url;
};

struct GroupMembers {
    std::vector<GroupMember> teachers;
    std::vector<GroupMember> students;
};

}  // namespace RumpelQuiz
