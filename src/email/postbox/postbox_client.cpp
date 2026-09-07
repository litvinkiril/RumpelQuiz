#include "postbox_client.hpp"

#include <chrono>
#include <stdexcept>
#include <string>
#include <string_view>
#include <userver/clients/http/component.hpp>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/formats/json/inline.hpp>
#include <userver/formats/json/serialize.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/yaml_config/merge_schemas.hpp>
#include <userver/yaml_config/schema.hpp>
#include <utility>

namespace RumpelQuiz {

PostboxClient::PostboxClient(userver::clients::http::Client& http_client,
                             std::string key_id, std::string secret_key,
                             std::string from_email, std::string url)
    : http_client_(http_client),
      is_configured_(!key_id.empty() && !secret_key.empty() &&
                     !from_email.empty()),
      signer_(std::move(key_id), std::move(secret_key)),
      from_email_(std::move(from_email)),
      url_(std::move(url)) {}

void PostboxClient::SendEmail(std::string_view to, std::string_view subject,
                              std::string_view text) const {
  if (!IsConfigured()) {
    throw std::runtime_error("Yandex Postbox credentials are not configured");
  }

  userver::formats::json::ValueBuilder body;

  body["FromEmailAddress"] = from_email_;

  body["Destination"]["ToAddresses"] =
      userver::formats::json::MakeArray(std::string{to});

  body["Content"]["Simple"]["Subject"]["Data"] = std::string{subject};

  body["Content"]["Simple"]["Subject"]["Charset"] = "UTF-8";

  body["Content"]["Simple"]["Body"]["Text"]["Data"] = std::string{text};

  body["Content"]["Simple"]["Body"]["Text"]["Charset"] = "UTF-8";

  const std::string payload =
      userver::formats::json::ToString(body.ExtractValue());

  const auto signed_headers = signer_.SignPostboxRequest(payload);

  const auto response =
      http_client_.CreateRequest()
          .post(url_, payload)
          .headers({{"Content-Type", "application/json"},
                    {"X-Amz-Date", signed_headers.amz_date},
                    {"X-Amz-Content-Sha256", signed_headers.content_sha256},
                    {"Authorization", signed_headers.authorization}})
          .timeout(std::chrono::seconds{10})
          .perform();

  if (!response->IsOk()) {
    throw std::runtime_error("Yandex Postbox failed: HTTP " +
                             std::to_string(response->status_code()));
  }
}

bool PostboxClient::IsConfigured() const noexcept { return is_configured_; }

PostboxClientComponent::PostboxClientComponent(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      client_(context.FindComponent<userver::components::HttpClient>()
                  .GetHttpClient(),
              config["key-id"].As<std::string>(),
              config["secret-key"].As<std::string>(),
              config["from-email"].As<std::string>(),
              config["url"].As<std::string>()) {}

PostboxClient& PostboxClientComponent::GetClient() noexcept { return client_; }

const PostboxClient& PostboxClientComponent::GetClient() const noexcept {
  return client_;
}

userver::yaml_config::Schema PostboxClientComponent::GetStaticConfigSchema() {
  return userver::yaml_config::MergeSchemas<userver::components::ComponentBase>(
      R"(
type: object
description: Yandex Cloud Postbox HTTP client configuration.
additionalProperties: false
properties:
    key-id:
        type: string
        description: Static access key identifier.
    secret-key:
        type: string
        description: Static access key secret.
    from-email:
        type: string
        description: Verified sender email address.
    url:
        type: string
        description: Yandex Postbox endpoint or a testsuite mock URL.
)");
}

}  // namespace RumpelQuiz
