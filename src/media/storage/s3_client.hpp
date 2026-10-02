#pragma once

#include <chrono>
#include <stdexcept>
#include <string>
#include <string_view>

#include <userver/clients/http/client.hpp>
#include <userver/components/component_base.hpp>
#include <userver/yaml_config/schema.hpp>

namespace RumpelQuiz {

class S3Error final : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class S3Client final {
public:
    S3Client(
        userver::clients::http::Client& http_client,
        std::string bucket,
        std::string access_key_id,
        std::string secret_access_key
    );

    void PutObject(
        std::string_view key,
        std::string_view contents,
        std::string_view content_type
    ) const;

    void DeleteObject(std::string_view key) const;

    std::string MakeDownloadUrl(
        std::string_view key,
        std::chrono::seconds lifetime = std::chrono::minutes{15}
    ) const;

private:
    std::string MakePath(std::string_view key) const;

    void SendRequest(
        std::string_view method,
        std::string_view key,
        std::string_view contents,
        std::string_view content_type
    ) const;

    userver::clients::http::Client& http_client_;
    std::string bucket_;
    std::string access_key_id_;
    std::string secret_access_key_;
};

class S3ClientComponent final
    : public userver::components::ComponentBase {
public:
    static constexpr std::string_view kName = "s3-client";

    S3ClientComponent(
        const userver::components::ComponentConfig& config,
        const userver::components::ComponentContext& context
    );

    S3Client& GetClient() noexcept;
    const S3Client& GetClient() const noexcept;

    static userver::yaml_config::Schema GetStaticConfigSchema();

private:
    S3Client client_;
};

}  // namespace RumpelQuiz