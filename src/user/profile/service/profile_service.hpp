#pragma once
#include <boost/uuid/uuid.hpp>
#include <string_view>
#include <userver/components/component_base.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include "education/repository/education_repository.hpp"
#include "user/profile/profile_models.hpp"
#include "user/profile/repository/profile_repository.hpp"

namespace RumpelQuiz {
class ProfileService final : public userver::components::ComponentBase {
public:
    static constexpr std::string_view kName = "profile-service";
    ProfileService(const userver::components::ComponentConfig& config,
                   const userver::components::ComponentContext& context);
    GetProfileResult GetProfile(const boost::uuids::uuid& user_id) const;
private:
    userver::storages::postgres::ClusterPtr pg_;
    ProfileRepository profile_repository_;
    EducationRepository education_repository_;
};
}  // namespace RumpelQuiz
