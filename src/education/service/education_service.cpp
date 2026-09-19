#include "education_service.hpp"

#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/storages/postgres/options.hpp>

namespace RumpelQuiz {

EducationService::EducationService(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      pg_(context.FindComponent<userver::components::Postgres>(
              "postgres-db-1")
              .GetCluster()) {}

GetUniversityAdminsResult EducationService::GetUniversityAdmins(
    const boost::uuids::uuid& user_id,
    const boost::uuids::uuid& university_id) const {
    auto transaction = pg_->Begin(
        userver::storages::postgres::ClusterHostType::kMaster,
        userver::storages::postgres::TransactionOptions{});

    const bool has_access =
        education_repository_.HasActiveAdminMembership(
            transaction, user_id, university_id);

    if (!has_access) {
        return GetUniversityAdminsError::kAccessDenied;
    }

    auto admins =
        education_repository_.GetActiveAdminsByUniversityId(
            transaction, university_id);

    transaction.Commit();

    return admins;
}

bool EducationService::CanManageUniversity(
    userver::storages::postgres::Transaction& transaction,
    const boost::uuids::uuid& user_id, const boost::uuids::uuid& university_id) const {
    const auto access = education_repository_.GetAdminAccess(transaction, user_id, university_id);
    return access && access->CanManageUniversity();
}

bool EducationService::CanManageFaculty(
    userver::storages::postgres::Transaction& transaction,
    const boost::uuids::uuid& user_id, const boost::uuids::uuid& university_id,
    const boost::uuids::uuid& faculty_id) const {
    const auto access = education_repository_.GetAdminAccess(transaction, user_id, university_id);
    return access && access->CanManageFaculty(faculty_id) &&
        education_repository_.FacultyExists(transaction, university_id, faculty_id);
}

bool EducationService::CanManageGroup(
    userver::storages::postgres::Transaction& transaction,
    const boost::uuids::uuid& user_id, const boost::uuids::uuid& university_id,
    const boost::uuids::uuid& group_id) const {
    const auto access = education_repository_.GetAdminAccess(transaction, user_id, university_id);
    if (!access) return false;
    const auto faculties = education_repository_.GetGroupFacultyIds(transaction, university_id, group_id);
    return faculties && access->CanManageGroup(*faculties);
}

SearchUniversityPeopleResult EducationService::SearchUniversityPeople(
    const boost::uuids::uuid& user_id,
    const boost::uuids::uuid& university_id,
    const PeopleSearchParams& params) const {
    auto transaction = pg_->Begin(
        userver::storages::postgres::ClusterHostType::kMaster,
        userver::storages::postgres::TransactionOptions{}
    );

    if (!education_repository_.HasActiveMembership(
            transaction, user_id, university_id)) {
        return SearchUniversityPeopleError::kAccessDenied;
    }

    auto people =
        education_repository_.SearchPeopleByUniversityId(
            transaction,
            university_id,
            params
        );

    const bool has_more =
        people.size() > static_cast<std::size_t>(params.limit);

    if (has_more) {
        people.resize(static_cast<std::size_t>(params.limit));
    }

    SearchUniversityPeopleSuccess success{
        std::move(people),
        has_more,
        std::nullopt,
    };

    if (has_more) {
        success.next_offset =
            params.offset + static_cast<int>(success.people.size());
    }

    transaction.Commit();
    return success;
}


}  // namespace RumpelQuiz
