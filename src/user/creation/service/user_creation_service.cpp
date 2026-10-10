#include "user_creation_service.hpp"

#include <optional>
#include <string>
#include <string_view>

#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/storages/postgres/exceptions.hpp>
#include <userver/storages/postgres/options.hpp>

namespace RumpelQuiz {

UserCreationService::UserCreationService(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      pg_(context.FindComponent<userver::components::Postgres>("postgres-db-1")
              .GetCluster()),
      education_service_(context.FindComponent<EducationService>()) {}

CreateUserResult UserCreationService::CreateUser(
    const boost::uuids::uuid& actor_user_id,
    const CreateUserRequest& request) const {
  const std::string_view email = request.email;
  const std::string_view password = request.password;

  if (email.empty() || email.size() > 254 ||
      email.find('@') == std::string_view::npos ||
      email.find_first_of(" \t\r\n") != std::string_view::npos ||
      email.find('\0') != std::string_view::npos || password.empty() ||
      password.size() > 72 || password.find('\0') != std::string_view::npos ||
      request.first_name.find_first_not_of(" \t\r\n") == std::string::npos ||
      request.last_name.find_first_not_of(" \t\r\n") == std::string::npos ||
      request.first_name.find('\0') != std::string::npos ||
      request.last_name.find('\0') != std::string::npos ||
      (request.middle_name &&
       request.middle_name->find('\0') != std::string::npos) ||
      (request.description &&
       request.description->find('\0') != std::string::npos)) {
    return CreateUserError::kInvalidRequest;
  }

  if (request.role != "student" && request.role != "teacher" &&
      request.role != "admin") {
    return CreateUserError::kInvalidRequest;
  }

  if (request.facultet_id && request.role != "admin") {
    return CreateUserError::kInvalidRequest;
  }

  try {
    auto transaction =
        pg_->Begin(userver::storages::postgres::ClusterHostType::kMaster,
                   userver::storages::postgres::TransactionOptions{});

    if (education_repository_
            .GetUniversitiesByIds(transaction, {request.university_id})
            .empty()) {
      return CreateUserError::kUniversityNotFound;
    }

    const bool can_manage_university = education_service_.CanManageUniversity(
        transaction, actor_user_id, request.university_id);
    if (!can_manage_university &&
        (!request.facultet_id ||
         !education_service_.CanManageFaculty(transaction, actor_user_id,
                                              request.university_id,
                                              *request.facultet_id))) {
      return CreateUserError::kAccessDenied;
    }

    if (request.facultet_id &&
        !education_repository_.FacultyExists(transaction, request.university_id,
                                             *request.facultet_id)) {
      return CreateUserError::kInvalidFaculty;
    }

    if (user_repository_.FindByEmail(transaction, email)) {
      return CreateUserError::kEmailAlreadyExists;
    }

    const auto password_hash = password_hasher_.HashPassword(password);

    const auto user_id =
        user_repository_.CreateUser(transaction, email, password_hash);

    user_repository_.VerifyUser(transaction, user_id);

    profile_repository_.Create(transaction, user_id, request.first_name,
                               request.last_name, request.middle_name,
                               std::nullopt, request.description);

    if (!education_repository_.AddMembership(
            transaction, user_id, request.university_id, request.role,
            request.facultet_id)) {
      return CreateUserError::kInvalidRequest;
    }

    transaction.Commit();

    return CreateUserSuccess{user_id};
  } catch (const userver::storages::postgres::UniqueViolation& error) {
    if (error.GetSchema() == "auth" && error.GetTable() == "users" &&
        error.GetConstraint() == "users_email_key") {
      return CreateUserError::kEmailAlreadyExists;
    }

    throw;
  }
}

}  // namespace RumpelQuiz
