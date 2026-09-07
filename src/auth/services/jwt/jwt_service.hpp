#pragma once

#include <boost/uuid/uuid.hpp>
#include <chrono>
#include <stdexcept>
#include <string>
#include <string_view>
#include <userver/components/component_base.hpp>
#include <userver/yaml_config/schema.hpp>

namespace RumpelQuiz {

class JwtError final : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

class JwtService {
 public:
  explicit JwtService(
      std::string secret,
      std::chrono::seconds access_token_lifetime = std::chrono::hours{1});

  std::string GenerateAccessToken(const boost::uuids::uuid& user_id) const;

  boost::uuids::uuid VerifyAccessToken(std::string_view token) const;

 private:
  std::string secret_;
  std::chrono::seconds access_token_lifetime_;
};

class JwtServiceComponent final : public userver::components::ComponentBase {
 public:
  static constexpr std::string_view kName = "jwt-service";

  JwtServiceComponent(const userver::components::ComponentConfig& config,
                      const userver::components::ComponentContext& context);

  const JwtService& GetService() const noexcept;

  static userver::yaml_config::Schema GetStaticConfigSchema();

 private:
  JwtService service_;
};

}  // namespace RumpelQuiz
