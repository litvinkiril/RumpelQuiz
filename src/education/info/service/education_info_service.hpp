#pragma once

#include <optional>
#include <string_view>

#include <boost/uuid/uuid.hpp>
#include <userver/components/component_base.hpp>
#include <userver/storages/postgres/cluster.hpp>

#include "education/info/info_models.hpp"
#include "education/info/repository/education_info_repository.hpp"

namespace RumpelQuiz {

class EducationInfoService final
    : public userver::components::ComponentBase {
public:
    static constexpr std::string_view kName = "education-info-service";

    EducationInfoService(
        const userver::components::ComponentConfig& config,
        const userver::components::ComponentContext& context);

    std::optional<GroupMembers> GetGroupMembers(
        const boost::uuids::uuid& group_id
    ) const;

private:
    userver::storages::postgres::ClusterPtr pg_;
    EducationInfoRepository repository_;
};

}  // namespace RumpelQuiz
