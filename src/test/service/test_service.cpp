#include "test_service.hpp"
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
TestService::TestService(const userver::components::ComponentConfig& config,
                         const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      pg_(context.FindComponent<userver::components::Postgres>("postgres-db-1")
              .GetCluster()),
      s3_client_(context.FindComponent<S3ClientComponent>().GetClient()) {}
SaveTestResult TestService::SaveTest(
    const boost::uuids::uuid& user_id,
    const std::optional<boost::uuids::uuid>& test_id,
    const SaveTestRequest& input) const {
  auto errors = test_validation_.CheckCommon(input);
  if (test_id.has_value() != input.revision.has_value())
    errors.push_back({"revision", "Версия обязательна только при обновлении."});
  if (!errors.empty())
    return SaveTestFailure{SaveTestError::kValidationFailed, std::move(errors)};
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
    return SaveTestFailure{SaveTestError::kAccessDenied, {}};
  if (test_id) {
    const auto old = test_repository_.FindOwnedForUpdate(tx, *test_id, user_id);
    if (!old) return SaveTestFailure{SaveTestError::kNotFound, {}};
    if (old->university_id != input.university_id &&
        !can_author(old->university_id))
      return SaveTestFailure{SaveTestError::kAccessDenied, {}};
    // The row lock serializes publication with all concurrent edits.
    if (old->status != "draft")
      return SaveTestFailure{SaveTestError::kPublished, {}};
    if (old->revision != *input.revision)
      return SaveTestFailure{SaveTestError::kRevisionConflict, {}};
  }
  if (input.status != TestStatus::kDraft) {
    errors = test_validation_.CheckReady(input);
    if (!errors.empty())
      return SaveTestFailure{SaveTestError::kValidationFailed,
                             std::move(errors)};
  }
  std::set<boost::uuids::uuid> images;
  for (const auto& q : input.questions) {
    if (q.image_id) images.insert(*q.image_id);
    for (const auto& a : q.answers)
      if (a.image_id) images.insert(*a.image_id);
  }
  if (!test_repository_.CheckImages(tx, user_id,
                                    {images.begin(), images.end()}))
    return SaveTestFailure{
        SaveTestError::kValidationFailed,
        {{"questions", "Картинка недоступна или ещё не загружена."}}};
  const auto id = test_id ? *test_id : boost::uuids::random_generator{}();
  const auto revision = test_repository_.Save(tx, id, user_id, input, !test_id);
  test_repository_.ReplaceQuestions(tx, id, input);
  tx.Commit();
  return SaveTestSuccess{id, input.status, revision};
}
userver::formats::json::Value TestService::ReadTest(
    const boost::uuids::uuid& user, const boost::uuids::uuid& id) const {
  auto tx = pg_->Begin(userver::storages::postgres::ClusterHostType::kMaster,
                       userver::storages::postgres::TransactionOptions{});
  const auto value = test_repository_.Read(tx, id, user);
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
userver::formats::json::Value TestService::ListTests(
    const boost::uuids::uuid& user) const {
  auto tx = pg_->Begin(userver::storages::postgres::ClusterHostType::kMaster,
                       userver::storages::postgres::TransactionOptions{});
  auto value = test_repository_.List(tx, user);
  tx.Commit();
  return value;
}
userver::formats::json::Value TestService::Play(
    const boost::uuids::uuid& user,
    const std::optional<boost::uuids::uuid>& test, const std::string& operation,
    const std::optional<boost::uuids::uuid>& question,
    const std::vector<boost::uuids::uuid>& choices) const {
  auto tx = pg_->Begin(userver::storages::postgres::ClusterHostType::kMaster,
                       userver::storages::postgres::TransactionOptions{});
  userver::formats::json::ValueBuilder out;
  out["success"] = true;
  if (operation == "available") {
    out["tests"] = play_repository_.Available(tx, user);
  } else if (operation == "results") {
    out["results"] = play_repository_.Results(tx, user, test.value());
  } else {
    if (operation == "start") play_repository_.Start(tx, user, test.value());
    if (operation == "answers" &&
        !play_repository_.Submit(tx, user, test.value(), question.value(),
                                 choices)) {
      tx.Commit();
      throw TestPlayError("test_attempt_finished");
    }
    auto value = play_repository_.Read(tx, user, test.value());
    out = userver::formats::json::ValueBuilder(value);
    if (!value["current_question"].IsNull()) {
      const auto sign = [&](auto item, const auto& source) {
        const auto key =
            source["image_key"].template As<std::optional<std::string>>();
        item.Remove("image_key");
        item["image_url"] = key ? s3_client_.DownloadUrl(*key) : std::string{};
      };
      sign(out["current_question"], value["current_question"]);
      for (std::size_t i = 0;
           i < value["current_question"]["answers"].GetSize(); ++i)
        sign(out["current_question"]["answers"][i],
             value["current_question"]["answers"][i]);
    }
  }
  tx.Commit();
  return out.ExtractValue();
}
}  // namespace RumpelQuiz
