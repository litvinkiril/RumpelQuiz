#include "s3client_component.hpp"

#include <chrono>
#include <stdexcept>
#include <string>
#include <utility>

#include <userver/clients/http/component.hpp>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/s3api/authenticators/access_key.hpp>
#include <userver/s3api/models/secret.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace RumpelQuiz {
S3ClientComponent::S3ClientComponent(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context) {
  if (!config["enabled"].As<bool>(false)) return;

  const auto endpoint = config["endpoint"].As<std::string>();
  const auto bucket = config["bucket"].As<std::string>();
  auto access_key = config["access-key"].As<std::string>();
  auto secret_key = config["secret-key"].As<std::string>();
  const auto timeout_ms = config["timeout-ms"].As<int>(10000);
  if (endpoint.empty() ||
      endpoint.find_first_of("/\\?#@ \t\r\n") != std::string::npos ||
      bucket.empty() || access_key.empty() || secret_key.empty() ||
      timeout_ms <= 0) {
    throw std::invalid_argument(
        "Invalid S3 configuration: expected endpoint host, bucket, credentials "
        "and positive timeout");
  }

  auto& http_client =
      context.FindComponent<userver::components::HttpClient>().GetHttpClient();
  auto connection = userver::s3api::MakeS3Connection(
      http_client, userver::s3api::S3ConnectionType::kHttps, endpoint,
      userver::s3api::ConnectionCfg{std::chrono::milliseconds{timeout_ms}});
  auto authenticator =
      std::make_shared<userver::s3api::authenticators::AccessKey>(
          std::move(access_key), userver::s3api::Secret{std::move(secret_key)});
  client_ = std::make_unique<S3Client>(userver::s3api::GetS3Client(
      std::move(connection), std::move(authenticator), bucket), S3ClientConfig{endpoint, bucket});
}


const S3ClientBase& S3ClientComponent::GetClient() const noexcept {
  return *client_;
}

userver::yaml_config::Schema S3ClientComponent::GetStaticConfigSchema() {
  return userver::yaml_config::MergeSchemas<userver::components::ComponentBase>(
      R"(
type: object
description: Image storage using the userver S3 client.
additionalProperties: false
properties:
    enabled:
        type: boolean
        description: Enable uploads; disabled by default.
        default: false
    endpoint:
        type: string
        description: S3 host without scheme or path.
    bucket:
        type: string
        description: Image bucket name.
    access-key:
        type: string
        description: Static access key ID.
    secret-key:
        type: string
        description: Static access key secret.
    timeout-ms:
        type: integer
        description: Upload request timeout in milliseconds.
        default: 10000
        minimum: 1
)");
}

}  // namespace RumpelQuiz

