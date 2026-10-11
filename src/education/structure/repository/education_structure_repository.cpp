#include "education_structure_repository.hpp"

#include <optional>
#include <string>

#include <userver/storages/postgres/io/optional.hpp>
#include <userver/storages/postgres/io/uuid.hpp>

namespace RumpelQuiz {

std::vector<Faculty>
EducationStructureRepository::GetFacultiesByUniversityId(
    userver::storages::postgres::Transaction& transaction,
    const boost::uuids::uuid& university_id) const {
    const auto result = transaction.Execute(
        R"(
            SELECT id, university_id, name
            FROM education.faculties
            WHERE university_id = $1
            ORDER BY name, id
        )",
        university_id
    );

    std::vector<Faculty> faculties;
    faculties.reserve(result.Size());

    for (const auto& row : result) {
        faculties.push_back(Faculty{
            row["id"].As<boost::uuids::uuid>(),
            row["university_id"].As<boost::uuids::uuid>(),
            row["name"].As<std::string>(),
        });
    }

    return faculties;
}

std::vector<Program>
EducationStructureRepository::GetProgramsByUniversityId(
    userver::storages::postgres::Transaction& transaction,
    const boost::uuids::uuid& university_id) const {
    const auto result = transaction.Execute(
        R"(
            SELECT id, university_id, name
            FROM education.programs
            WHERE university_id = $1
            ORDER BY name, id
        )",
        university_id
    );

    std::vector<Program> programs;
    programs.reserve(result.Size());

    for (const auto& row : result) {
        programs.push_back(Program{
            row["id"].As<boost::uuids::uuid>(),
            row["university_id"].As<boost::uuids::uuid>(),
            row["name"].As<std::string>(),
        });
    }

    return programs;
}

std::vector<Program>
EducationStructureRepository::GetProgramsByFacultyId(
    userver::storages::postgres::Transaction& transaction,
    const boost::uuids::uuid& faculty_id) const {
    const auto result = transaction.Execute(
        R"(
            SELECT p.id, p.university_id, p.name
            FROM education.programs AS p
            JOIN education.faculty_programs AS fp
                ON fp.program_id = p.id
                AND fp.university_id = p.university_id
            WHERE fp.faculty_id = $1
            ORDER BY p.name, p.id
        )",
        faculty_id
    );

    std::vector<Program> programs;
    programs.reserve(result.Size());

    for (const auto& row : result) {
        programs.push_back(Program{
            row["id"].As<boost::uuids::uuid>(),
            row["university_id"].As<boost::uuids::uuid>(),
            row["name"].As<std::string>(),
        });
    }

    return programs;
}

std::vector<StudyGroup>
EducationStructureRepository::GetGroupsByUniversityId(
    userver::storages::postgres::Transaction& transaction,
    const boost::uuids::uuid& university_id) const {
    const auto result = transaction.Execute(
        R"(
            SELECT id, university_id, name, program_id
            FROM education.study_groups
            WHERE university_id = $1
            ORDER BY name, id
        )",
        university_id
    );

    std::vector<StudyGroup> groups;
    groups.reserve(result.Size());

    for (const auto& row : result) {
        groups.push_back(StudyGroup{
            row["id"].As<boost::uuids::uuid>(),
            row["university_id"].As<boost::uuids::uuid>(),
            row["name"].As<std::string>(),
            row["program_id"]
                .As<std::optional<boost::uuids::uuid>>(),
        });
    }

    return groups;
}

std::vector<StudyGroup>
EducationStructureRepository::GetGroupsByFacultyId(
    userver::storages::postgres::Transaction& transaction,
    const boost::uuids::uuid& faculty_id) const {
    const auto result = transaction.Execute(
        R"(
            SELECT g.id, g.university_id, g.name, g.program_id
            FROM education.study_groups AS g
            JOIN education.faculty_programs AS fp
                ON fp.program_id = g.program_id
                AND fp.university_id = g.university_id
            WHERE fp.faculty_id = $1
            ORDER BY g.name, g.id
        )",
        faculty_id
    );

    std::vector<StudyGroup> groups;
    groups.reserve(result.Size());

    for (const auto& row : result) {
        groups.push_back(StudyGroup{
            row["id"].As<boost::uuids::uuid>(),
            row["university_id"].As<boost::uuids::uuid>(),
            row["name"].As<std::string>(),
            row["program_id"]
                .As<std::optional<boost::uuids::uuid>>(),
        });
    }

    return groups;
}

std::vector<StudyGroup>
EducationStructureRepository::GetGroupsByProgramId(
    userver::storages::postgres::Transaction& transaction,
    const boost::uuids::uuid& program_id) const {
    const auto result = transaction.Execute(
        R"(
            SELECT id, university_id, name, program_id
            FROM education.study_groups
            WHERE program_id = $1
            ORDER BY name, id
        )",
        program_id
    );

    std::vector<StudyGroup> groups;
    groups.reserve(result.Size());

    for (const auto& row : result) {
        groups.push_back(StudyGroup{
            row["id"].As<boost::uuids::uuid>(),
            row["university_id"].As<boost::uuids::uuid>(),
            row["name"].As<std::string>(),
            row["program_id"]
                .As<std::optional<boost::uuids::uuid>>(),
        });
    }

    return groups;
}

}  // namespace RumpelQuiz