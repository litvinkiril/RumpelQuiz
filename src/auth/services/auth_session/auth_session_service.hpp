#pragma once

#include <chrono>
#include <stdexcept>
#include <string>
#include <string_view>
#include <userver/components/component_base.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/yaml_config/schema.hpp>

#include "auth/repositories/session/session_repository.hpp"
#include "auth/services/jwt/jwt_service.hpp"
#include "auth/services/refresh_token/refresh_token_service.hpp"

namespace RumpelQuiz {
struct TokenPair {
  std::string access_token;
  std::string refresh_token;
  boost::uuids::uuid session_id;
};

class AuthSessionError final : public std::runtime_error {
 public:
  AuthSessionError() : std::runtime_error("Invalid or expired session") {}
};

class AuthSessionService final : public userver::components::ComponentBase {
 public:
  static constexpr std::string_view kName = "auth-session-service";
  AuthSessionService(const userver::components::ComponentConfig& config,
                     const userver::components::ComponentContext& context);
  TokenPair CreateSession(const boost::uuids::uuid& user_id) const;
  TokenPair RefreshSession(const boost::uuids::uuid& session_id,
                           std::string_view refresh_token) const;
  void RevokeSession(const boost::uuids::uuid& session_id) const;
  void RevokeAllSessions(const boost::uuids::uuid& user_id) const;
  static userver::yaml_config::Schema GetStaticConfigSchema();

 private:
  userver::storages::postgres::ClusterPtr pg_;
  const JwtService& jwt_service_;
  std::chrono::seconds session_lifetime_;
  SessionRepository repository_;
  RefreshTokenService refresh_tokens_;
};
}  // namespace RumpelQuiz
