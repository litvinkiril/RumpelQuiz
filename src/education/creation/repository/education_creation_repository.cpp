#include "education_creation_repository.hpp"

#include <userver/storages/postgres/io/array_types.hpp>
#include <userver/storages/postgres/io/uuid.hpp>

namespace RumpelQuiz {

bool EducationCreationRepository::UniversityExists(
    userver::storages::postgres::Transaction& transaction,
    const boost::uuids::uuid& university_id) const {
  // Keep the university alive until the new resource is committed.
  const auto result = transaction.Execute(
      "SELECT id FROM education.universities WHERE id = $1 FOR KEY SHARE",
      university_id);
  return !result.IsEmpty();
}

std::vector<boost::uuids::uuid>
EducationCreationRepository::GetExistingFacultyIds(
    userver::storages::postgres::Transaction& transaction,
    const boost::uuids::uuid& university_id,
    const std::vector<boost::uuids::uuid>& faculty_ids) const {
  const auto result = transaction.Execute(
      "SELECT id FROM education.faculties "
      "WHERE university_id = $1 AND id = ANY($2::uuid[]) "
      "ORDER BY id FOR KEY SHARE",
      university_id, faculty_ids);

  std::vector<boost::uuids::uuid> ids;
  ids.reserve(result.Size());
  for (const auto& row : result) {
    ids.push_back(row["id"].As<boost::uuids::uuid>());
  }
  return ids;
}

std::optional<std::vector<boost::uuids::uuid>>
EducationCreationRepository::GetProgramFacultyIds(
    userver::storages::postgres::Transaction& transaction,
    const boost::uuids::uuid& university_id,
    const boost::uuids::uuid& program_id) const {
  // Faculty-link removals also lock the program in the database trigger.
  // Hold this lock through authorization and creation of the group.
  const auto program = transaction.Execute(
      "SELECT id FROM education.programs "
      "WHERE university_id = $1 AND id = $2 FOR SHARE",
      university_id, program_id);
  if (program.IsEmpty()) return std::nullopt;

  // Read the links after acquiring the lock, using a fresh statement snapshot.
  const auto result = transaction.Execute(
      "SELECT faculty_id FROM education.faculty_programs "
      "WHERE university_id = $1 AND program_id = $2 ORDER BY faculty_id",
      university_id, program_id);
  std::vector<boost::uuids::uuid> ids;
  ids.reserve(result.Size());
  for (const auto& row : result) {
    ids.push_back(row["faculty_id"].As<boost::uuids::uuid>());
  }
  return ids;
}

std::optional<boost::uuids::uuid> EducationCreationRepository::CreateFaculty(
    userver::storages::postgres::Transaction& transaction,
    const boost::uuids::uuid& university_id, std::string_view name) const {
  const auto result = transaction.Execute(
      "INSERT INTO education.faculties (university_id, name) VALUES ($1, $2) "
      "ON CONFLICT (university_id, name) DO NOTHING RETURNING id",
      university_id, name);
  if (result.IsEmpty()) return std::nullopt;
  return result[0]["id"].As<boost::uuids::uuid>();
}

boost::uuids::uuid EducationCreationRepository::CreateProgram(
    userver::storages::postgres::Transaction& transaction,
    const boost::uuids::uuid& university_id, std::string_view name) const {
  const auto result = transaction.Execute(
      "INSERT INTO education.programs (university_id, name) "
      "VALUES ($1, $2) RETURNING id",
      university_id, name);
  return result.AsSingleRow<boost::uuids::uuid>();
}

void EducationCreationRepository::AddProgramFaculties(
    userver::storages::postgres::Transaction& transaction,
    const boost::uuids::uuid& university_id,
    const boost::uuids::uuid& program_id,
    const std::vector<boost::uuids::uuid>& faculty_ids) const {
  transaction.Execute(
      "INSERT INTO education.faculty_programs "
      "(faculty_id, program_id, university_id) "
      "SELECT faculty_id, $2, $1 FROM unnest($3::uuid[]) AS ids(faculty_id)",
      university_id, program_id, faculty_ids);
}

std::optional<boost::uuids::uuid> EducationCreationRepository::CreateGroup(
    userver::storages::postgres::Transaction& transaction,
    const boost::uuids::uuid& university_id,
    const boost::uuids::uuid& program_id, std::string_view name) const {
  const auto result = transaction.Execute(
      "INSERT INTO education.study_groups (university_id, program_id, name) "
      "VALUES ($1, $2, $3) "
      "ON CONFLICT (program_id, name) DO NOTHING RETURNING id",
      university_id, program_id, name);
  if (result.IsEmpty()) return std::nullopt;
  return result[0]["id"].As<boost::uuids::uuid>();
}

}  // namespace RumpelQuiz
