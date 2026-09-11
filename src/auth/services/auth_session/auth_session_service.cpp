#include "auth_session_service.hpp"

#include <cstdint>
#include <utility>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/storages/postgres/options.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace RumpelQuiz {
namespace pg = userver::storages::postgres;

AuthSessionService::AuthSessionService(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      pg_(context.FindComponent<userver::components::Postgres>("postgres-db-1")
              .GetCluster()),
      jwt_service_(context.FindComponent<JwtServiceComponent>().GetService()),
      session_lifetime_(config["session-lifetime-seconds"].As<std::int64_t>()) {
  if (session_lifetime_ <= std::chrono::seconds::zero())
    throw std::invalid_argument("Session lifetime must be positive");
}

TokenPair AuthSessionService::CreateSession(
    const boost::uuids::uuid& user_id) const {
  auto transaction =
      pg_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
  auto refresh_token = refresh_tokens_.Generate();
  const auto id = repository_.Create(
      transaction, user_id, refresh_tokens_.Hash(refresh_token),
      pg::TimePointTz{std::chrono::system_clock::now() + session_lifetime_});
  TokenPair result{jwt_service_.GenerateAccessToken(user_id, id),
                   std::move(refresh_token), id};
  transaction.Commit();
  return result;
}

TokenPair AuthSessionService::RefreshSession(
    const boost::uuids::uuid& session_id,
    std::string_view refresh_token) const {
  auto transaction =
      pg_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
  // Lock through commit: a concurrent refresh reads the rotated hash and fails.
  const auto session = repository_.FindById(transaction, session_id);
  if (!session || std::chrono::system_clock::now() >= session->expires_at ||
      !refresh_tokens_.Verify(refresh_token, session->refresh_token_hash))
    throw AuthSessionError{};
  auto next_token = refresh_tokens_.Generate();
  repository_.UpdateRefreshToken(transaction, session_id,
                                 refresh_tokens_.Hash(next_token));
  TokenPair result{
      jwt_service_.GenerateAccessToken(session->user_id, session_id),
      std::move(next_token), session_id};
  transaction.Commit();
  return result;
}

void AuthSessionService::RevokeSession(
    const boost::uuids::uuid& session_id) const {
  auto transaction =
      pg_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
  repository_.Delete(transaction, session_id);
  transaction.Commit();
}

void AuthSessionService::RevokeAllSessions(
    const boost::uuids::uuid& user_id) const {
  auto transaction =
      pg_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
  repository_.DeleteAllByUserId(transaction, user_id);
  transaction.Commit();
}

userver::yaml_config::Schema AuthSessionService::GetStaticConfigSchema() {
  return userver::yaml_config::MergeSchemas<userver::components::ComponentBase>(
      R"(
type: object
description: Authentication session configuration.
additionalProperties: false
properties:
    session-lifetime-seconds:
        type: integer
        minimum: 1
        description: Absolute session lifetime in seconds, never extended on refresh.
required:
  - session-lifetime-seconds
)");
}
}  // namespace RumpelQuiz
