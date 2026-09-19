#pragma once

#include <string_view>

#include <boost/uuid/uuid.hpp>

#include <userver/components/component_base.hpp>
#include <userver/storages/postgres/cluster.hpp>

#include "education/education_models.hpp"
#include "education/repository/education_repository.hpp"

namespace RumpelQuiz {

class EducationService final
    : public userver::components::ComponentBase {
public:
    static constexpr std::string_view kName = "education-service";

    EducationService(
        const userver::components::ComponentConfig& config,
        const userver::components::ComponentContext& context);

    GetUniversityAdminsResult GetUniversityAdmins(
        const boost::uuids::uuid& user_id,
        const boost::uuids::uuid& university_id
    ) const;

private:
    userver::storages::postgres::ClusterPtr pg_;
    EducationRepository education_repository_;
};

}  // namespace RumpelQuiz