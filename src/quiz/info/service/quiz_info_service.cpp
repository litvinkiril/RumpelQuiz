#include "quiz_info_service.hpp"

#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/storages/postgres/options.hpp>

namespace RumpelQuiz {

QuizInfoService::QuizInfoService(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      pg_(context.FindComponent<userver::components::Postgres>(
              "postgres-db-1").GetCluster()) {}

std::optional<userver::formats::json::Value>
QuizInfoService::ListQuizSessions(
    const boost::uuids::uuid& user_id,
    const boost::uuids::uuid& quiz_id) const {
  auto tx = pg_->Begin(
      userver::storages::postgres::ClusterHostType::kMaster,
      userver::storages::postgres::TransactionOptions{});

  if (!repository_.CanViewQuiz(tx, user_id, quiz_id))
    return std::nullopt;

  auto sessions = repository_.ListQuizSessions(tx, quiz_id);
  tx.Commit();
  return sessions;
}

SessionResultsResult QuizInfoService::GetSessionResults(
    const boost::uuids::uuid& user_id,
    const boost::uuids::uuid& session_id) const {
  auto tx = pg_->Begin(
      userver::storages::postgres::ClusterHostType::kMaster,
      userver::storages::postgres::TransactionOptions{});
  const auto access = repository_.GetSessionResultsAccess(tx, user_id, session_id);
  if (!access || !access->can_view) return SessionResultsError::kNotFound;
  if (access->status != "finished" && access->status != "cancelled")
    return SessionResultsError::kNotFinished;
  auto results = repository_.GetSessionResults(tx, session_id);
  tx.Commit();
  return results;
}

}  // namespace RumpelQuiz
