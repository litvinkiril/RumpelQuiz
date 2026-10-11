#pragma once

#include <string_view>
#include <vector>

#include <boost/uuid/uuid.hpp>

#include <userver/components/component_base.hpp>
#include <userver/storages/postgres/cluster.hpp>

#include "education/education_models.hpp"
#include "education/structure/repository/education_structure_repository.hpp"

namespace RumpelQuiz {

class EducationStructureService final
    : public userver::components::ComponentBase {
public:
    static constexpr std::string_view kName =
        "education-structure-service";

    EducationStructureService(
        const userver::components::ComponentConfig& config,
        const userver::components::ComponentContext& context);

    std::vector<Faculty> GetFaculties(
        const boost::uuids::uuid& university_id
    ) const;

    std::vector<Program> GetPrograms(
        const StructureFilter& filter
    ) const;

    std::vector<StudyGroup> GetGroups(
        const StructureFilter& filter
    ) const;

private:
    userver::storages::postgres::ClusterPtr pg_;
    EducationStructureRepository repository_;
};

}  // namespace RumpelQuiz