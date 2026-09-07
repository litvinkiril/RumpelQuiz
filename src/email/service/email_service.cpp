#include "email_service.hpp"

#include <chrono>
#include <stdexcept>
#include <string>
#include <string_view>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/logging/log.hpp>
#include <userver/yaml_config/merge_schemas.hpp>
#include <userver/yaml_config/schema.hpp>

namespace RumpelQuiz {

EmailService::EmailService(const userver::components::ComponentConfig& config,
                           const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      postbox_client_(
          context.FindComponent<PostboxClientComponent>().GetClient()),
      send_enabled_(config["send-enabled"].As<bool>()),
      log_codes_(config["log-codes"].As<bool>(false)) {
  if (send_enabled_ && !postbox_client_.IsConfigured()) {
    throw std::runtime_error("Yandex Postbox credentials are not configured");
  }
}

void EmailService::SendVerificationCode(std::string_view email,
                                        std::string_view code,
                                        std::chrono::seconds lifetime) const {
  if (log_codes_) {
    LOG_INFO() << "[DEV AUTH CODE] purpose=verify-email email=" << email
               << " code=" << code << " ttl_seconds=" << lifetime.count();
  }
  if (!send_enabled_) {
    return;
  }

  const std::string subject = "RumpelQuiz — подтверждение почты";

  const std::string text = "Ваш код подтверждения: " + std::string{code} +
                           "\n\nКод действует " +
                           std::to_string(lifetime.count()) + " секунд.";

  postbox_client_.SendEmail(email, subject, text);
}

userver::yaml_config::Schema EmailService::GetStaticConfigSchema() {
  return userver::yaml_config::MergeSchemas<userver::components::ComponentBase>(
      R"(
type: object
description: Verification email service configuration.
additionalProperties: false
properties:
    log-codes:
        type: boolean
        description: Print plaintext codes to server console for local development only.
        defaultDescription: false
    send-enabled:
        type: boolean
        description: Enables delivery through Yandex Cloud Postbox.
)");
}

}  // namespace RumpelQuiz

namespace RumpelQuiz {
void EmailService::SendPasswordResetCode(std::string_view email,
                                         std::string_view code,
                                         std::chrono::seconds lifetime) const {
  if (log_codes_) {
    LOG_INFO() << "[DEV AUTH CODE] purpose=reset-password email=" << email
               << " code=" << code << " ttl_seconds=" << lifetime.count();
  }
  if (!send_enabled_) return;
  postbox_client_.SendEmail(email, "RumpelQuiz — восстановление пароля",
                            "Ваш код для сброса пароля: " + std::string{code} +
                                "\n\nКод действует " +
                                std::to_string(lifetime.count()) + " секунд.");
}
}  // namespace RumpelQuiz
