#pragma once
#include <boost/uuid/uuid.hpp>
#include <vector>
#include <userver/storages/postgres/transaction.hpp>
#include "education/education_models.hpp"

namespace RumpelQuiz {
class EducationRepository {
public:
    std::optional<AdminAccess> GetAdminAccess(
        userver::storages::postgres::Transaction& transaction,
        const boost::uuids::uuid& user_id,
        const boost::uuids::uuid& university_id) const;
    bool FacultyExists(
        userver::storages::postgres::Transaction& transaction,
        const boost::uuids::uuid& university_id,
        const boost::uuids::uuid& faculty_id) const;
    std::optional<std::vector<boost::uuids::uuid>> GetGroupFacultyIds(
        userver::storages::postgres::Transaction& transaction,
        const boost::uuids::uuid& university_id,
        const boost::uuids::uuid& group_id) const;
    std::vector<Membership> GetAllActiveMembershipsByUserId(
        userver::storages::postgres::Transaction& transaction,
        const boost::uuids::uuid& user_id) const;
    std::vector<University> GetUniversitiesByIds(
        userver::storages::postgres::Transaction& transaction,
        const std::vector<boost::uuids::uuid>& university_ids) const;
    std::vector<StudentGroup> GetGroupsStudentbyMemberships(
        userver::storages::postgres::Transaction& transaction,
        const std::vector<Membership>& memberships) const;
    std::vector<Position> GetPositionsByUserId(
        userver::storages::postgres::Transaction& transaction,
        const boost::uuids::uuid& user_id) const;

    bool HasActiveAdminMembership(
        userver::storages::postgres::Transaction& transaction,
        const boost::uuids::uuid& user_id,
        const boost::uuids::uuid& university_id
    ) const;

    bool HasActiveMembership(
        userver::storages::postgres::Transaction& transaction,
        const boost::uuids::uuid& user_id,
        const boost::uuids::uuid& university_id
    ) const;


    std::vector<UniversityAdmin> GetActiveAdminsByUniversityId(
        userver::storages::postgres::Transaction& transaction,
        const boost::uuids::uuid& university_id
    ) const;

    std::vector<std::string> GetActiveRoles(
        userver::storages::postgres::Transaction& transaction,
        const boost::uuids::uuid& user_id,
        const boost::uuids::uuid& university_id
    ) const;

    std::vector<UniversityPerson> SearchPeopleByUniversityId(
        userver::storages::postgres::Transaction& transaction,
        const boost::uuids::uuid& university_id,
        const PeopleSearchParams& params
    ) const;
};
}  // namespace RumpelQuiz
