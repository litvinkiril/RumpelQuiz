#include "email_service.hpp"

#include <stdexcept>
#include <string>

#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace RumpelQuiz {

EmailService::EmailService(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context
)
    : ComponentBase(config, context),
      postbox_client_(
          context
              .FindComponent<PostboxClientComponent>()
              .GetClient()
      ),
      send_enabled_(config["send-enabled"].As<bool>()) {
    if (send_enabled_ && !postbox_client_.IsConfigured()) {
        throw std::runtime_error(
            "Yandex Postbox credentials are not configured"
        );
    }
}

void EmailService::SendVerificationCode(
    std::string_view email,
    std::string_view code
) const {
    if (!send_enabled_) {
        return;
    }

    const std::string subject =
        "RumpelQuiz — подтверждение почты";

    const std::string text =
        "Ваш код подтверждения: " +
        std::string{code} +
        "\n\nКод действует 15 минут.";

    postbox_client_.SendEmail(
        email,
        subject,
        text
    );
}

userver::yaml_config::Schema EmailService::GetStaticConfigSchema() {
    return userver::yaml_config::MergeSchemas<
        userver::components::ComponentBase
    >(R"(
type: object
description: Verification email service configuration.
additionalProperties: false
properties:
    send-enabled:
        type: boolean
        description: Enables delivery through Yandex Cloud Postbox.
)");
}

}
