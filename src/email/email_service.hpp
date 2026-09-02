#pragma once

#include <string_view>

#include <userver/components/component_base.hpp>
#include <userver/yaml_config/schema.hpp>

#include "postbox_client.hpp"

namespace RumpelQuiz {

class EmailService final : public userver::components::ComponentBase {
public:
    static constexpr std::string_view kName = "email-service";

    EmailService(
        const userver::components::ComponentConfig& config,
        const userver::components::ComponentContext& context
    );

    void SendVerificationCode(
        std::string_view email,
        std::string_view code
    ) const;

    static userver::yaml_config::Schema GetStaticConfigSchema();

private:
    PostboxClient& postbox_client_;
    bool send_enabled_;
};

}
