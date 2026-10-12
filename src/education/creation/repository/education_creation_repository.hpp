#pragma once

#include <optional>
#include <string_view>
#include <vector>

#include <boost/uuid/uuid.hpp>
#include <userver/storages/postgres/transaction.hpp>

namespace RumpelQuiz {

class EducationCreationRepository {
 public:
  bool UniversityExists(userver::storages::postgres::Transaction& transaction,
                        const boost::uuids::uuid& university_id) const;

  std::vector<boost::uuids::uuid> GetExistingFacultyIds(
      userver::storages::postgres::Transaction& transaction,
      const boost::uuids::uuid& university_id,
      const std::vector<boost::uuids::uuid>& faculty_ids) const;

  std::optional<std::vector<boost::uuids::uuid>> GetProgramFacultyIds(
      userver::storages::postgres::Transaction& transaction,
      const boost::uuids::uuid& university_id,
      const boost::uuids::uuid& program_id) const;

  // An empty result means the name is already used in this university.
  std::optional<boost::uuids::uuid> CreateFaculty(
      userver::storages::postgres::Transaction& transaction,
      const boost::uuids::uuid& university_id, std::string_view name) const;

  // Program name conflicts are checked when linking its faculties.
  boost::uuids::uuid CreateProgram(
      userver::storages::postgres::Transaction& transaction,
      const boost::uuids::uuid& university_id, std::string_view name) const;

  void AddProgramFaculties(
      userver::storages::postgres::Transaction& transaction,
      const boost::uuids::uuid& university_id,
      const boost::uuids::uuid& program_id,
      const std::vector<boost::uuids::uuid>& faculty_ids) const;

  // An empty result means the name is already used in this program.
  std::optional<boost::uuids::uuid> CreateGroup(
      userver::storages::postgres::Transaction& transaction,
      const boost::uuids::uuid& university_id,
      const boost::uuids::uuid& program_id, std::string_view name) const;
};

}  // namespace RumpelQuiz
