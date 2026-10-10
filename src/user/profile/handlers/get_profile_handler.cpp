#include "get_profile_handler.hpp"

#include <chrono>
#include <string_view>
#include <variant>

#include <boost/uuid/uuid_io.hpp>

#include <userver/components/component_context.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/logging/log.hpp>
#include <userver/server/http/http_status.hpp>

#include "auth/middleware/jwt/auth_context.hpp"

namespace RumpelQuiz {

GetProfileHandler::GetProfileHandler(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : HttpHandlerJsonBase(config, context),
      profile_service_(context.FindComponent<ProfileService>()) {}

userver::formats::json::Value
GetProfileHandler::HandleRequestJsonThrow(
    const userver::server::http::HttpRequest& request,
    const userver::formats::json::Value&,
    userver::server::request::RequestContext& context) const {
    const auto started_at = std::chrono::steady_clock::now();

    const auto& user_id = GetAuthenticatedUserId(context);
    const auto result = profile_service_.GetProfile(user_id);

    userver::formats::json::ValueBuilder response;
    request.GetHttpResponse().SetHeader(
        std::string_view{"Cache-Control"}, "no-store");

    if (const auto* error = std::get_if<GetProfileError>(&result)) {
        using userver::server::http::HttpStatus;
        response["success"] = false;

        switch (*error) {
            case GetProfileError::kUserNotFound:
                request.SetResponseStatus(HttpStatus::kNotFound);
                response["error"] = "user_not_found";
                break;

            case GetProfileError::kEmailNotVerified:
                request.SetResponseStatus(HttpStatus::kForbidden);
                response["error"] = "email_not_verified";
                break;

            case GetProfileError::kUserProfileNotFound:
                request.SetResponseStatus(HttpStatus::kNotFound);
                response["error"] = "user_profile_not_found";
                break;
        }
    } else {
        const auto& profile = std::get<FullProfile>(result);

        response["success"] = true;
        response["email"] = profile.email;

        response["first_name"] = profile.first_name
            ? userver::formats::json::ValueBuilder{*profile.first_name}
            : userver::formats::json::ValueBuilder{};

        response["last_name"] = profile.last_name
            ? userver::formats::json::ValueBuilder{*profile.last_name}
            : userver::formats::json::ValueBuilder{};

        response["middle_name"] = profile.middle_name
            ? userver::formats::json::ValueBuilder{*profile.middle_name}
            : userver::formats::json::ValueBuilder{};

        response["avatar_url"] = profile.avatar_url
            ? userver::formats::json::ValueBuilder{*profile.avatar_url}
            : userver::formats::json::ValueBuilder{};

        response["description"] = profile.description
            ? userver::formats::json::ValueBuilder{*profile.description}
            : userver::formats::json::ValueBuilder{};

        userver::formats::json::ValueBuilder positions(
            userver::formats::json::Type::kArray);

        for (const auto& position : profile.university_position) {
            userver::formats::json::ValueBuilder item;

            item["university_id"] =
                boost::uuids::to_string(position.university_id);
            item["university_name"] = position.university_name;
            item["role"] = position.role;
            item["admin_scope"] = position.admin_scope
                ? userver::formats::json::ValueBuilder{*position.admin_scope}
                : userver::formats::json::ValueBuilder{};

            if (position.group_id) {
                item["group_id"] =
                    boost::uuids::to_string(*position.group_id);
            } else {
                item["group_id"] =
                    userver::formats::json::ValueBuilder{};
            }

            if (position.group_name) {
                item["group_name"] = *position.group_name;
            } else {
                item["group_name"] =
                    userver::formats::json::ValueBuilder{};
            }

            positions.PushBack(item.ExtractValue());
        }

        response["university_position"] = positions.ExtractValue();
    }

    auto response_json = response.ExtractValue();

    const double elapsed_ms =
        std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - started_at)
            .count();

    LOG_INFO() << "GetProfileHandler duration_ms=" << elapsed_ms;

    return response_json;
}

}  // namespace RumpelQuiz
