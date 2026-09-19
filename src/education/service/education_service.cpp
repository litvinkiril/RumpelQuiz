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

}  // namespace RumpelQuiz