#include "quiz_service.hpp"
#include <algorithm>
#include <boost/uuid/random_generator.hpp>
#include <set>
#include <userver/components/component_context.hpp>
#include <userver/formats/json.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/storages/postgres/options.hpp>
#include <utility>
#include "s3client/s3client_component.hpp"
namespace RumpelQuiz {
QuizService::QuizService(const userver::components::ComponentConfig& config,
                         const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      pg_(context.FindComponent<userver::components::Postgres>("postgres-db-1")
              .GetCluster()),
      s3_client_(context.FindComponent<S3ClientComponent>().GetClient()) {}
SaveQuizResult QuizService::SaveQuiz(
    const boost::uuids::uuid& user_id,
    const std::optional<boost::uuids::uuid>& quiz_id,
    const SaveQuizRequest& input) const {
  auto errors = quiz_validation_.CheckCommon(input);
  if (quiz_id.has_value() != input.revision.has_value())
    errors.push_back({"revision", "Версия обязательна только при обновлении."});
  if (!errors.empty())
    return SaveQuizFailure{SaveQuizError::kValidationFailed, std::move(errors)};
  auto tx = pg_->Begin(userver::storages::postgres::ClusterHostType::kMaster,
                       userver::storages::postgres::TransactionOptions{});
  const auto can_author = [&](const auto& university) {
    const auto roles =
        education_repository_.GetActiveRoles(tx, user_id, university);
    return std::any_of(roles.begin(), roles.end(), [](const auto& r) {
      return r == "teacher" || r == "admin";
    });
  };
  if (!can_author(input.university_id))
    return SaveQuizFailure{SaveQuizError::kAccessDenied, {}};
  if (quiz_id) {
    const auto old = quiz_repository_.FindOwnedForUpdate(tx, *quiz_id, user_id);
    if (!old) return SaveQuizFailure{SaveQuizError::kNotFound, {}};
    if (old->university_id != input.university_id &&
        !can_author(old->university_id))
      return SaveQuizFailure{SaveQuizError::kAccessDenied, {}};
    // The row lock serializes publication with all concurrent edits.
    if (old->status == "ready")
      return SaveQuizFailure{SaveQuizError::kPublished, {}};
    if (old->revision != *input.revision)
      return SaveQuizFailure{SaveQuizError::kRevisionConflict, {}};
  }
  if (input.status == QuizStatus::kReady) {
    errors = quiz_validation_.CheckReady(input);
    if (!errors.empty())
      return SaveQuizFailure{SaveQuizError::kValidationFailed,
                             std::move(errors)};
  }
  std::set<boost::uuids::uuid> images;
  for (const auto& q : input.questions) {
    if (q.image_id) images.insert(*q.image_id);
    for (const auto& a : q.answers)
      if (a.image_id) images.insert(*a.image_id);
  }
  if (!quiz_repository_.CheckImages(tx, user_id,
                                    {images.begin(), images.end()}))
    return SaveQuizFailure{
        SaveQuizError::kValidationFailed,
        {{"questions", "Картинка недоступна или ещё не загружена."}}};
  const auto id = quiz_id ? *quiz_id : boost::uuids::random_generator{}();
  const auto revision = quiz_repository_.Save(tx, id, user_id, input, !quiz_id);
  quiz_repository_.ReplaceQuestions(tx, id, input);
  tx.Commit();
  return SaveQuizSuccess{id, input.status, revision};
}
userver::formats::json::Value QuizService::ReadQuiz(
    const boost::uuids::uuid& user, const boost::uuids::uuid& id) const {
  auto tx = pg_->Begin(userver::storages::postgres::ClusterHostType::kMaster,
                       userver::storages::postgres::TransactionOptions{});
  const auto value = quiz_repository_.Read(tx, id, user);
  tx.Commit();
  if (value.IsNull()) return value;
  userver::formats::json::ValueBuilder out(value);
  const auto sign = [&](auto item, const auto& source) {
    const auto key =
        source["image_key"].template As<std::optional<std::string>>();
    item.Remove("image_key");
    item["image_url"] = key ? s3_client_.DownloadUrl(*key) : std::string{};
  };
  for (std::size_t i = 0; i < value["questions"].GetSize(); ++i) {
    sign(out["questions"][i], value["questions"][i]);
    for (std::size_t j = 0; j < value["questions"][i]["answers"].GetSize(); ++j)
      sign(out["questions"][i]["answers"][j],
           value["questions"][i]["answers"][j]);
  }
  return out.ExtractValue();
}
userver::formats::json::Value QuizService::ListQuizzes(
    const boost::uuids::uuid& user) const {
  auto tx = pg_->Begin(userver::storages::postgres::ClusterHostType::kMaster,
                       userver::storages::postgres::TransactionOptions{});
  auto value = quiz_repository_.List(tx, user);
  tx.Commit();
  return value;
}
}  // namespace RumpelQuiz
