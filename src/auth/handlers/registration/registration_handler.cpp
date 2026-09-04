#include "registration_handler.hpp"

#include <string>
#include <variant>

#include <boost/uuid/uuid_io.hpp>

#include <userver/components/component_context.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/server/http/http_status.hpp>

namespace RumpelQuiz {

RegisterHandler::RegisterHandler(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context
)
    : HttpHandlerJsonBase(config, context),
      registration_service_(
          context.FindComponent<RegistrationService>()
      ) {}

userver::formats::json::Value
RegisterHandler::HandleRequestJsonThrow(
    const userver::server::http::HttpRequest& request,
    const userver::formats::json::Value& request_body,
    userver::server::request::RequestContext&
) const {
    const auto email = request_body["email"].As<std::string>();
    const auto password = request_body["password"].As<std::string>();
    const auto password_confirmation =
        request_body["password_confirmation"].As<std::string>();

    const auto result = registration_service_.Register(
        email,
        password,
        password_confirmation
    );

    userver::formats::json::ValueBuilder response;

    if (const auto* success =
            std::get_if<RegistrationSuccess>(&result)) {
        request.SetResponseStatus(
            userver::server::http::HttpStatus::kCreated
        );
        response["success"] = true;
        response["verification_id"] =
            boost::uuids::to_string(success->verification_id);
        return response.ExtractValue();
    }

    response["success"] = false;

    switch (std::get<RegisterError>(result)) {
        case RegisterError::kPasswordsDoNotMatch:
            request.SetResponseStatus(
                userver::server::http::HttpStatus::kBadRequest
            );
            response["error"] = "passwords_do_not_match";
            break;
        case RegisterError::kEmailAlreadyExists:
            request.SetResponseStatus(
                userver::server::http::HttpStatus::kConflict
            );
            response["error"] = "email_already_exists";
            break;
    }

    return response.ExtractValue();
}

}
