#include "education_info_service.hpp"

#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/storages/postgres/options.hpp>

namespace RumpelQuiz {

EducationInfoService::EducationInfoService(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      pg_(context.FindComponent<userver::components::Postgres>(
          "postgres-db-1").GetCluster()) {}

std::optional<GroupMembers> EducationInfoService::GetGroupMembers(
    const boost::uuids::uuid& group_id) const {
    auto transaction = pg_->Begin(
        userver::storages::postgres::ClusterHostType::kMaster,
        userver::storages::postgres::TransactionOptions{}
    );
    auto members = repository_.GetGroupMembers(transaction, group_id);
    transaction.Commit();
    return members;
}

}  // namespace RumpelQuiz
