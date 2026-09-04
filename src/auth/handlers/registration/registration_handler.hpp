#pragma once

#include <userver/server/handlers/http_handler_json_base.hpp>

#include "auth/services/registration/registration_service.hpp"

namespace RumpelQuiz {

class RegisterHandler final
    : public userver::server::handlers::HttpHandlerJsonBase {
public:
    static constexpr std::string_view kName = "handler-auth-register";

    RegisterHandler(
        const userver::components::ComponentConfig& config,
        const userver::components::ComponentContext& context
    );

    userver::formats::json::Value HandleRequestJsonThrow(
        const userver::server::http::HttpRequest& request,
        const userver::formats::json::Value& request_body,
        userver::server::request::RequestContext& context
    ) const override;

private:
    RegistrationService& registration_service_;
};

}
