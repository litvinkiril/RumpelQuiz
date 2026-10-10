#include "profile_service.hpp"

#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/storages/postgres/options.hpp>

namespace RumpelQuiz {

ProfileService::ProfileService(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
    pg_(context.FindComponent<userver::components::Postgres>("postgres-db-1")
              .GetCluster()) {}


GetProfileResult ProfileService::GetProfile(const boost::uuids::uuid& user_id) const {
    auto transaction =
        pg_->Begin(userver::storages::postgres::ClusterHostType::kMaster,
                   userver::storages::postgres::TransactionOptions{});

    const auto user_profile = profile_repository_.FindUserWithProfileById(transaction, user_id);

    if (!user_profile) return GetProfileError::kUserNotFound;
    if (!user_profile->email_verified) return GetProfileError::kEmailNotVerified;
    if (!user_profile->profile_user_id) return GetProfileError::kUserProfileNotFound;

    auto university_position =
        education_repository_.GetPositionsByUserId(
            transaction, user_id
        );

    transaction.Commit();
    return FullProfile {
        user_profile->email,
        user_profile->first_name,
        user_profile->last_name,
        user_profile->middle_name,
        user_profile->avatar_url,
        user_profile->description,
        std::move(university_position)
    };
}
}
