#pragma once

#include <optional>

#include <boost/uuid/uuid.hpp>
#include <userver/storages/postgres/transaction.hpp>

#include "education/info/info_models.hpp"

namespace RumpelQuiz {

class EducationInfoRepository {
public:
    // nullopt means the group does not exist; an existing group may be empty.
    std::optional<GroupMembers> GetGroupMembers(
        userver::storages::postgres::Transaction& transaction,
        const boost::uuids::uuid& group_id
    ) const;
};

}  // namespace RumpelQuiz
