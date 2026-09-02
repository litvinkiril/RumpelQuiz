#pragma once

#include<iostream>
#include <string>
#include <string_view>

#include <userver/clients/http/client.hpp>
#include <userver/components/component_base.hpp>
#include <userver/yaml_config/schema.hpp>

#include "aws_signature_v4.hpp"

namespace RumpelQuiz {

class PostboxClient final {
public:
    PostboxClient(
        userver::clients::http::Client& http_client,
        std::string key_id,
        std::string secret_key,
        std::string from_email,
        std::string url
    );

    void SendEmail(
        std::string_view to,
        std::string_view subject,
        std::string_view text
    ) const;

    bool IsConfigured() const noexcept;

private:
    userver::clients::http::Client& http_client_;

    bool is_configured_;

    AwsSignatureV4 signer_;

    std::string from_email_;
    std::string url_;
};

class PostboxClientComponent final
    : public userver::components::ComponentBase {
public:
    static constexpr std::string_view kName = "postbox-client";

    PostboxClientComponent(
        const userver::components::ComponentConfig& config,
        const userver::components::ComponentContext& context
    );

    PostboxClient& GetClient() noexcept;
    const PostboxClient& GetClient() const noexcept;

    static userver::yaml_config::Schema GetStaticConfigSchema();

private:
    PostboxClient client_;
};

}
