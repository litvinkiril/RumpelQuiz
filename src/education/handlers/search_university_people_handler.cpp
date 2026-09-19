#include "search_university_people_handler.hpp"

#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>

#include <boost/uuid/string_generator.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <userver/components/component_context.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/server/http/http_status.hpp>

#include "auth/middleware/jwt/auth_context.hpp"
#include "education/education_models.hpp"
#include "education/handlers/search_university_people_params_parser.hpp"

namespace RumpelQuiz {
namespace {

std::optional<std::string_view> GetOptionalArgument(
    const userver::server::http::HttpRequest& request,
    std::string_view name) {
    if (!request.HasArg(name)) {
        return std::nullopt;
    }

    return request.GetArg(name);
}

userver::formats::json::Value MakeOptionalString(
    const std::optional<std::string>& value) {
    if (!value) {
        return userver::formats::json::ValueBuilder{}
            .ExtractValue();
    }

    return userver::formats::json::ValueBuilder{*value}
        .ExtractValue();
}

userver::formats::json::Value MakeBadRequest(
    const userver::server::http::HttpRequest& request,
    std::string_view error) {
    request.SetResponseStatus(
        userver::server::http::HttpStatus::kBadRequest
    );

    userver::formats::json::ValueBuilder response;
    response["success"] = false;
    response["error"] = error;

    return response.ExtractValue();
}

userver::formats::json::Value MakePersonJson(
    const UniversityPerson& person) {
    userver::formats::json::ValueBuilder result;

    result["user_id"] =
        boost::uuids::to_string(person.user_id);
    result["first_name"] =
        MakeOptionalString(person.first_name);
    result["last_name"] =
        MakeOptionalString(person.last_name);
    result["middle_name"] =
        MakeOptionalString(person.middle_name);
    result["avatar_url"] =
        MakeOptionalString(person.avatar_url);
    result["email"] = person.email;

    userver::formats::json::ValueBuilder roles(
        userver::formats::json::Type::kArray
    );

    for (const auto& role : person.roles) {
        roles.PushBack(role);
    }

    result["roles"] = roles.ExtractValue();

    if (!person.student_details) {
        result["student_details"] =
            userver::formats::json::ValueBuilder{}
                .ExtractValue();

        return result.ExtractValue();
    }

    userver::formats::json::ValueBuilder student_details;

    if (person.student_details->group) {
        userver::formats::json::ValueBuilder group;

        group["id"] = boost::uuids::to_string(
            person.student_details->group->id
        );
        group["name"] =
            person.student_details->group->name;

        student_details["group"] =
            group.ExtractValue();
    } else {
        student_details["group"] =
            userver::formats::json::ValueBuilder{}
                .ExtractValue();
    }

    userver::formats::json::ValueBuilder faculties(
        userver::formats::json::Type::kArray
    );

    for (const auto& faculty :
         person.student_details->faculties) {
        userver::formats::json::ValueBuilder value;

        value["id"] =
            boost::uuids::to_string(faculty.id);
        value["name"] = faculty.name;

        faculties.PushBack(value.ExtractValue());
    }

    student_details["faculties"] =
        faculties.ExtractValue();

    result["student_details"] =
        student_details.ExtractValue();

    return result.ExtractValue();
}

}  // namespace

SearchUniversityPeopleHandler::SearchUniversityPeopleHandler(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : HttpHandlerJsonBase(config, context),
      education_service_(
          context.FindComponent<EducationService>()) {}

userver::formats::json::Value
SearchUniversityPeopleHandler::HandleRequestJsonThrow(
    const userver::server::http::HttpRequest& request,
    const userver::formats::json::Value&,
    userver::server::request::RequestContext& context) const {
    using userver::server::http::HttpStatus;

    request.GetHttpResponse().SetHeader(
        std::string_view{"Cache-Control"},
        "no-store"
    );

    boost::uuids::uuid university_id;

    try {
        university_id = boost::uuids::string_generator{}(
            request.GetPathArg("university_id")
        );
    } catch (const std::runtime_error&) {
        return MakeBadRequest(
            request,
            "invalid_university_id"
        );
    }

    if (!request.HasArg("q")) {
        return MakeBadRequest(
            request,
            "invalid_search_params"
        );
    }

    const auto params = ParsePeopleSearchParams(
        RawPeopleSearchParams{
            request.GetArg("q"),
            GetOptionalArgument(request, "limit"),
            GetOptionalArgument(request, "offset"),
        }
    );

    if (!params) {
        return MakeBadRequest(
            request,
            "invalid_search_params"
        );
    }

    const auto result =
        education_service_.SearchUniversityPeople(
            GetAuthenticatedUserId(context),
            university_id,
            *params
        );

    if (const auto* error =
            std::get_if<SearchUniversityPeopleError>(&result)) {
        userver::formats::json::ValueBuilder response;
        response["success"] = false;

        switch (*error) {
            case SearchUniversityPeopleError::kAccessDenied:
                request.SetResponseStatus(
                    HttpStatus::kForbidden
                );
                response["error"] =
                    "university_access_denied";
                break;
        }

        return response.ExtractValue();
    }

    const auto& success =
        std::get<SearchUniversityPeopleSuccess>(result);

    userver::formats::json::ValueBuilder people(
        userver::formats::json::Type::kArray
    );

    for (const auto& person : success.people) {
        people.PushBack(MakePersonJson(person));
    }

    userver::formats::json::ValueBuilder response;

    response["success"] = true;
    response["people"] = people.ExtractValue();
    response["has_more"] = success.has_more;

    if (success.next_offset) {
        response["next_offset"] =
            *success.next_offset;
    } else {
        response["next_offset"] =
            userver::formats::json::ValueBuilder{}
                .ExtractValue();
    }

    return response.ExtractValue();
}

}  // namespace RumpelQuiz