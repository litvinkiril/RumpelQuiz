#pragma once

#include <string_view>

#include <boost/uuid/uuid.hpp>
#include <userver/components/component_base.hpp>
#include <userver/storages/postgres/cluster.hpp>

#include "auth/repositories/user/user_repository.hpp"
#include "auth/services/password/password_hasher.hpp"
#include "education/repository/education_repository.hpp"
#include "education/service/education_service.hpp"
#include "user/creation/user_creation_models.hpp"
#include "user/profile/repository/profile_repository.hpp"

namespace RumpelQuiz {

class UserCreationService final : public userver::components::ComponentBase {
 public:
  static constexpr std::string_view kName = "user-creation-service";

  UserCreationService(const userver::components::ComponentConfig& config,
                      const userver::components::ComponentContext& context);

  CreateUserResult CreateUser(const boost::uuids::uuid& actor_user_id,
                              const CreateUserRequest& request) const;

 private:
  userver::storages::postgres::ClusterPtr pg_;
  const EducationService& education_service_;
  PasswordHasher password_hasher_;
  UserRepository user_repository_;
  ProfileRepository profile_repository_;
  EducationRepository education_repository_;
};

}  // namespace RumpelQuiz