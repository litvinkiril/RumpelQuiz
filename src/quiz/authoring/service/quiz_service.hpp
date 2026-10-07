#pragma once

#include <optional>
#include <string_view>

#include <boost/uuid/uuid.hpp>

#include <userver/components/component_base.hpp>
#include <userver/storages/postgres/cluster.hpp>

#include "education/repository/education_repository.hpp"
#include "quiz/authoring/repository/quiz_repository.hpp"
#include "quiz/authoring/service/quiz_validation.hpp"
#include "quiz/quiz_models.hpp"
#include "s3client/s3client_base.hpp"

namespace RumpelQuiz {

class QuizService final : public userver::components::ComponentBase {
 public:
  static constexpr std::string_view kName = "quiz-service";

  QuizService(const userver::components::ComponentConfig& config,
              const userver::components::ComponentContext& context);

  SaveQuizResult SaveQuiz(const boost::uuids::uuid& user_id,
                          const std::optional<boost::uuids::uuid>& quiz_id,
                          const SaveQuizRequest& input) const;

  userver::formats::json::Value ReadQuiz(
      const boost::uuids::uuid& user_id,
      const boost::uuids::uuid& quiz_id) const;

  userver::formats::json::Value ListQuizzes(
      const boost::uuids::uuid& user_id) const;

 private:
  userver::storages::postgres::ClusterPtr pg_;

  EducationRepository education_repository_;
  QuizRepository quiz_repository_;
  QuizValidation quiz_validation_;
  const S3ClientBase& s3_client_;
};

}  // namespace RumpelQuiz
