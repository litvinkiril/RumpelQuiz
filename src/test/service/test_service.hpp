#pragma once

#include <optional>
#include <string_view>

#include <boost/uuid/uuid.hpp>

#include <userver/components/component_base.hpp>
#include <userver/storages/postgres/cluster.hpp>

#include "education/repository/education_repository.hpp"
#include "s3client/s3client_base.hpp"
#include "test/repository/test_play_repository.hpp"
#include "test/repository/test_repository.hpp"
#include "test/service/test_validation.hpp"
#include "test/test_models.hpp"

namespace RumpelQuiz {

class TestService final : public userver::components::ComponentBase {
 public:
  static constexpr std::string_view kName = "test-service";

  TestService(const userver::components::ComponentConfig& config,
              const userver::components::ComponentContext& context);

  SaveTestResult SaveTest(const boost::uuids::uuid& user_id,
                          const std::optional<boost::uuids::uuid>& test_id,
                          const SaveTestRequest& input) const;

  userver::formats::json::Value ReadTest(
      const boost::uuids::uuid& user_id,
      const boost::uuids::uuid& test_id) const;

  userver::formats::json::Value ListTests(
      const boost::uuids::uuid& user_id) const;

  userver::formats::json::Value Play(
      const boost::uuids::uuid& user,
      const std::optional<boost::uuids::uuid>& test,
      const std::string& operation,
      const std::optional<boost::uuids::uuid>& question = {},
      const std::vector<boost::uuids::uuid>& choices = {}) const;

 private:
  userver::storages::postgres::ClusterPtr pg_;
  EducationRepository education_repository_;
  TestRepository test_repository_;
  TestPlayRepository play_repository_;
  TestValidation test_validation_;
  const S3ClientBase& s3_client_;
};

}  // namespace RumpelQuiz
