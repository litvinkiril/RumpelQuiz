#pragma once

#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <boost/uuid/uuid.hpp>
#include <userver/components/component_base.hpp>
#include <userver/storages/postgres/cluster.hpp>

#include "education/creation/repository/education_creation_repository.hpp"
#include "education/repository/education_repository.hpp"

namespace RumpelQuiz {

struct CreateFacultyRequest {
  boost::uuids::uuid university_id;
  std::string name;
};

struct CreateProgramRequest {
  boost::uuids::uuid university_id;
  std::string name;
  std::vector<boost::uuids::uuid> faculty_ids;
};

struct CreateGroupRequest {
  boost::uuids::uuid university_id;
  boost::uuids::uuid program_id;
  std::string name;
};

struct EducationCreationSuccess {
  boost::uuids::uuid id;
};

enum class EducationCreationError {
  kInvalidRequest,
  kAccessDenied,
  kUniversityNotFound,
  kInvalidFaculty,
  kInvalidProgram,
  kNameAlreadyExists,
};

using EducationCreationResult =
    std::variant<EducationCreationSuccess, EducationCreationError>;

class EducationCreationService final
    : public userver::components::ComponentBase {
 public:
  static constexpr std::string_view kName = "education-creation-service";

  EducationCreationService(
      const userver::components::ComponentConfig& config,
      const userver::components::ComponentContext& context);

  EducationCreationResult CreateFaculty(
      const boost::uuids::uuid& actor_user_id,
      const CreateFacultyRequest& request) const;

  EducationCreationResult CreateProgram(
      const boost::uuids::uuid& actor_user_id,
      const CreateProgramRequest& request) const;

  EducationCreationResult CreateGroup(const boost::uuids::uuid& actor_user_id,
                                      const CreateGroupRequest& request) const;

 private:
  userver::storages::postgres::ClusterPtr pg_;
  EducationRepository education_repository_;
  EducationCreationRepository creation_repository_;
};

}  // namespace RumpelQuiz
