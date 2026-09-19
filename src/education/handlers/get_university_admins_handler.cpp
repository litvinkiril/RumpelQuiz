#include "get_university_admins_handler.hpp"

#include <stdexcept>
#include <string_view>
#include <variant>
#include <vector>

#include <boost/uuid/string_generator.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <userver/components/component_context.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/server/http/http_status.hpp>

#include "auth/middleware/jwt/auth_context.hpp"
#include "education/education_models.hpp"

namespace RumpelQuiz {

GetUniversityAdminsHandler::GetUniversityAdminsHandler(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : HttpHandlerJsonBase(config, context),
      education_service_(
          context.FindComponent<EducationService>()) {}

userver::formats::json::Value
GetUniversityAdminsHandler::HandleRequestJsonThrow(
    const userver::server::http::HttpRequest& request,
    const userver::formats::json::Value&,
    userver::server::request::RequestContext& context) const {
    using userver::server::http::HttpStatus;

    request.GetHttpResponse().SetHeader(
        std::string_view{"Cache-Control"}, "no-store");

    userver::formats::json::ValueBuilder response;

    boost::uuids::uuid university_id;
    const auto& university_id_text =
        request.GetPathArg("university_id");

    try {
        university_id =
            boost::uuids::string_generator{}(university_id_text);
    } catch (const std::runtime_error&) {
        request.SetResponseStatus(HttpStatus::kBadRequest);
        response["success"] = false;
        response["error"] = "invalid_university_id";
        return response.ExtractValue();
    }

    const auto& user_id = GetAuthenticatedUserId(context);

    const auto result = education_service_.GetUniversityAdmins(
        user_id, university_id);

    if (const auto* error =
            std::get_if<GetUniversityAdminsError>(&result)) {
        response["success"] = false;

        switch (*error) {
            case GetUniversityAdminsError::kAccessDenied:
                request.SetResponseStatus(HttpStatus::kForbidden);
                response["error"] = "university_access_denied";
                break;
        }

        return response.ExtractValue();
    }

    const auto& admins =
        std::get<std::vector<UniversityAdmin>>(result);

    userver::formats::json::ValueBuilder items(
        userver::formats::json::Type::kArray);

    for (const auto& admin : admins) {
        userver::formats::json::ValueBuilder item;

        item["membership_id"] =
            boost::uuids::to_string(admin.membership_id);

        item["first_name"] = admin.first_name
            ? userver::formats::json::ValueBuilder{*admin.first_name}
            : userver::formats::json::ValueBuilder{};

        item["last_name"] = admin.last_name
            ? userver::formats::json::ValueBuilder{*admin.last_name}
            : userver::formats::json::ValueBuilder{};

        item["middle_name"] = admin.middle_name
            ? userver::formats::json::ValueBuilder{*admin.middle_name}
            : userver::formats::json::ValueBuilder{};

        item["avatar_url"] = admin.avatar_url
            ? userver::formats::json::ValueBuilder{*admin.avatar_url}
            : userver::formats::json::ValueBuilder{};

        item["email"] = admin.email;

        items.PushBack(item.ExtractValue());
    }

    response["success"] = true;
    response["admins"] = items.ExtractValue();

    return response.ExtractValue();
}

}  // namespace RumpelQuiz