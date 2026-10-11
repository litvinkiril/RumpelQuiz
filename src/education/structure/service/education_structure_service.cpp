#include "education_structure_service.hpp"

#include <stdexcept>

#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/storages/postgres/options.hpp>

namespace RumpelQuiz {

EducationStructureService::EducationStructureService(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      pg_(
          context.FindComponent<userver::components::Postgres>(
              "postgres-db-1"
          ).GetCluster()
      ) {}

std::vector<Faculty> EducationStructureService::GetFaculties(
    const boost::uuids::uuid& university_id) const {
    auto transaction = pg_->Begin(
        userver::storages::postgres::ClusterHostType::kMaster,
        userver::storages::postgres::TransactionOptions{}
    );

    auto faculties = repository_.GetFacultiesByUniversityId(
        transaction,
        university_id
    );

    transaction.Commit();

    return faculties;
}

std::vector<Program> EducationStructureService::GetPrograms(
    const StructureFilter& filter) const {

    std::vector<Program> programs;

    auto transaction = pg_->Begin(
        userver::storages::postgres::ClusterHostType::kMaster,
        userver::storages::postgres::TransactionOptions{}
    );

    switch (filter.kind) {
        case StructureFilterKind::kUniversity:
            programs = repository_.GetProgramsByUniversityId(
                transaction,
                filter.id
            );
            break;

        case StructureFilterKind::kFaculty:
            programs = repository_.GetProgramsByFacultyId(
                transaction,
                filter.id
            );
            break;

        default:
            throw std::invalid_argument(
                "Unsupported programs filter"
            );
    }

    transaction.Commit();

    return programs;
}

std::vector<StudyGroup> EducationStructureService::GetGroups(
    const StructureFilter& filter) const {

    std::vector<StudyGroup> groups;

    auto transaction = pg_->Begin(
        userver::storages::postgres::ClusterHostType::kMaster,
        userver::storages::postgres::TransactionOptions{}
    );

    switch (filter.kind) {
        case StructureFilterKind::kUniversity:
            groups = repository_.GetGroupsByUniversityId(
                transaction,
                filter.id
            );
            break;

        case StructureFilterKind::kFaculty:
            groups = repository_.GetGroupsByFacultyId(
                transaction,
                filter.id
            );
            break;

        case StructureFilterKind::kProgram:
            groups = repository_.GetGroupsByProgramId(
                transaction,
                filter.id
            );
            break;

        default:
            throw std::invalid_argument(
                "Unsupported groups filter"
            );
    }

    transaction.Commit();

    return groups;
}

}  // namespace RumpelQuiz
