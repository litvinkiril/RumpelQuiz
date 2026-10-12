#include "education_creation_service.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>

#include <unicode/uchar.h>
#include <unicode/utf8.h>

#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/storages/postgres/exceptions.hpp>
#include <userver/storages/postgres/options.hpp>

namespace RumpelQuiz {
namespace {

bool IsValidName(const std::string& name) {
  if (name.find('\0') != std::string::npos ||
      name.size() >
          static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())) {
    return false;
  }

  const auto length = static_cast<std::int32_t>(name.size());
  std::int32_t offset = 0;
  bool has_text = false;
  while (offset < length) {
    UChar32 character;
    U8_NEXT(name.data(), offset, length, character);
    if (character < 0) return false;
    if (!u_isUWhiteSpace(character)) has_text = true;
  }
  return has_text;
}

}  // namespace

EducationCreationService::EducationCreationService(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      pg_(context.FindComponent<userver::components::Postgres>("postgres-db-1")
              .GetCluster()) {}

EducationCreationResult EducationCreationService::CreateFaculty(
    const boost::uuids::uuid& actor_user_id,
    const CreateFacultyRequest& request) const {
  if (request.university_id.is_nil() || !IsValidName(request.name)) {
    return EducationCreationError::kInvalidRequest;
  }

  auto transaction =
      pg_->Begin(userver::storages::postgres::ClusterHostType::kMaster,
                 userver::storages::postgres::TransactionOptions{});

  if (!creation_repository_.UniversityExists(transaction,
                                             request.university_id)) {
    return EducationCreationError::kUniversityNotFound;
  }

  const auto access = education_repository_.GetAdminAccess(
      transaction, actor_user_id, request.university_id);
  if (!access || !access->CanManageUniversity()) {
    return EducationCreationError::kAccessDenied;
  }

  const auto id = creation_repository_.CreateFaculty(
      transaction, request.university_id, request.name);
  if (!id) return EducationCreationError::kNameAlreadyExists;

  transaction.Commit();
  return EducationCreationSuccess{*id};
}

EducationCreationResult EducationCreationService::CreateProgram(
    const boost::uuids::uuid& actor_user_id,
    const CreateProgramRequest& request) const {
  if (request.university_id.is_nil() || !IsValidName(request.name) ||
      request.faculty_ids.empty() ||
      std::any_of(request.faculty_ids.begin(), request.faculty_ids.end(),
                  [](const auto& id) { return id.is_nil(); })) {
    return EducationCreationError::kInvalidRequest;
  }

  auto faculty_ids = request.faculty_ids;
  std::sort(faculty_ids.begin(), faculty_ids.end());
  if (std::adjacent_find(faculty_ids.begin(), faculty_ids.end()) !=
      faculty_ids.end()) {
    return EducationCreationError::kInvalidRequest;
  }

  auto transaction =
      pg_->Begin(userver::storages::postgres::ClusterHostType::kMaster,
                 userver::storages::postgres::TransactionOptions{});

  if (!creation_repository_.UniversityExists(transaction,
                                             request.university_id)) {
    return EducationCreationError::kUniversityNotFound;
  }

  const auto access = education_repository_.GetAdminAccess(
      transaction, actor_user_id, request.university_id);
  if (!access) return EducationCreationError::kAccessDenied;

  const auto existing_faculties = creation_repository_.GetExistingFacultyIds(
      transaction, request.university_id, faculty_ids);
  if (existing_faculties.size() != faculty_ids.size()) {
    return EducationCreationError::kInvalidFaculty;
  }

  if (!std::all_of(faculty_ids.begin(), faculty_ids.end(), [&](const auto& id) {
        return access->CanManageFaculty(id);
      })) {
    return EducationCreationError::kAccessDenied;
  }

  const auto id = creation_repository_.CreateProgram(
      transaction, request.university_id, request.name);
  try {
    creation_repository_.AddProgramFaculties(transaction, request.university_id,
                                             id, faculty_ids);
  } catch (const userver::storages::postgres::UniqueViolation& error) {
    if (error.GetSchema() == "education" &&
        error.GetTable() == "faculty_programs" &&
        error.GetConstraint() == "faculty_programs_faculty_name_key") {
      // Returning without Commit rolls back the new program and all its links.
      return EducationCreationError::kNameAlreadyExists;
    }
    throw;
  }

  transaction.Commit();
  return EducationCreationSuccess{id};
}

EducationCreationResult EducationCreationService::CreateGroup(
    const boost::uuids::uuid& actor_user_id,
    const CreateGroupRequest& request) const {
  if (request.university_id.is_nil() || request.program_id.is_nil() ||
      !IsValidName(request.name)) {
    return EducationCreationError::kInvalidRequest;
  }

  auto transaction =
      pg_->Begin(userver::storages::postgres::ClusterHostType::kMaster,
                 userver::storages::postgres::TransactionOptions{});

  if (!creation_repository_.UniversityExists(transaction,
                                             request.university_id)) {
    return EducationCreationError::kUniversityNotFound;
  }

  const auto access = education_repository_.GetAdminAccess(
      transaction, actor_user_id, request.university_id);
  if (!access) return EducationCreationError::kAccessDenied;

  const auto faculty_ids = creation_repository_.GetProgramFacultyIds(
      transaction, request.university_id, request.program_id);
  if (!faculty_ids) return EducationCreationError::kInvalidProgram;

  if (!access->CanManageGroup(*faculty_ids)) {
    return EducationCreationError::kAccessDenied;
  }

  const auto id = creation_repository_.CreateGroup(
      transaction, request.university_id, request.program_id, request.name);
  if (!id) return EducationCreationError::kNameAlreadyExists;

  transaction.Commit();
  return EducationCreationSuccess{*id};
}

}  // namespace RumpelQuiz
