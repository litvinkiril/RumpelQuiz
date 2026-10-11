#pragma once

#include <vector>

#include <boost/uuid/uuid.hpp>

#include <userver/storages/postgres/transaction.hpp>

#include "education/education_models.hpp"

namespace RumpelQuiz {

class EducationStructureRepository {
public:
    std::vector<Faculty> GetFacultiesByUniversityId(
        userver::storages::postgres::Transaction& transaction,
        const boost::uuids::uuid& university_id
    ) const;

    std::vector<Program> GetProgramsByUniversityId(
        userver::storages::postgres::Transaction& transaction,
        const boost::uuids::uuid& university_id
    ) const;

    std::vector<Program> GetProgramsByFacultyId(
        userver::storages::postgres::Transaction& transaction,
        const boost::uuids::uuid& faculty_id
    ) const;

    std::vector<StudyGroup> GetGroupsByUniversityId(
        userver::storages::postgres::Transaction& transaction,
        const boost::uuids::uuid& university_id
    ) const;

    std::vector<StudyGroup> GetGroupsByFacultyId(
        userver::storages::postgres::Transaction& transaction,
        const boost::uuids::uuid& faculty_id
    ) const;

    std::vector<StudyGroup> GetGroupsByProgramId(
        userver::storages::postgres::Transaction& transaction,
        const boost::uuids::uuid& program_id
    ) const;
};

}  // namespace RumpelQuiz