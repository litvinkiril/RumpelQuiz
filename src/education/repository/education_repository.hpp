#pragma once
#include <boost/uuid/uuid.hpp>
#include <vector>
#include <userver/storages/postgres/transaction.hpp>
#include "education/education_models.hpp"

namespace RumpelQuiz {
class EducationRepository {
public:
    std::vector<Membership> GetAllActiveMembershipsByUserId(
        userver::storages::postgres::Transaction& transaction,
        const boost::uuids::uuid& user_id) const;
    std::vector<University> GetUniversitiesByIds(
        userver::storages::postgres::Transaction& transaction,
        const std::vector<boost::uuids::uuid>& university_ids) const;
    std::vector<StudentGroup> GetGroupsStudentbyMemberships(
        userver::storages::postgres::Transaction& transaction,
        const std::vector<Membership>& memberships) const;
};
}  // namespace RumpelQuiz
