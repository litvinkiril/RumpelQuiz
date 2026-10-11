#include "get_faculties_handler.hpp"

#include <stdexcept>
#include <string_view>

#include <boost/uuid/string_generator.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <userver/components/component_context.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/server/http/http_status.hpp>
#include "education/education_models.hpp"

namespace RumpelQuiz {

GetFacultiesHandler::GetFacultiesHandler(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : HttpHandlerJsonBase(config, context),
      structure_service_(
          context.FindComponent<EducationStructureService>()) {}

userver::formats::json::Value
GetFacultiesHandler::HandleRequestJsonThrow(
    const userver::server::http::HttpRequest& request,
    const userver::formats::json::Value&,
    userver::server::request::RequestContext&) const {
    using userver::server::http::HttpStatus;

    request.GetHttpResponse().SetHeader(
        std::string_view{"Cache-Control"}, "no-store"
    );

    userver::formats::json::ValueBuilder response;
    response["success"] = false;

    if (!request.HasArg("university_id")) {
        request.SetResponseStatus(HttpStatus::kBadRequest);
        response["error"] = "invalid_structure_filter";
        return response.ExtractValue();
    }

    boost::uuids::uuid university_id;
    try {
        university_id = boost::uuids::string_generator{}(
            request.GetArg("university_id")
        );
    } catch (const std::runtime_error&) {
        request.SetResponseStatus(HttpStatus::kBadRequest);
        response["error"] = "invalid_university_id";
        return response.ExtractValue();
    }

    const auto faculties = structure_service_.GetFaculties(university_id);

    userver::formats::json::ValueBuilder items(
        userver::formats::json::Type::kArray
    );

    for (const auto& faculty : faculties) {
        userver::formats::json::ValueBuilder item;
        item["id"] = boost::uuids::to_string(faculty.id);
        item["name"] = faculty.name;
        items.PushBack(item.ExtractValue());
    }
    response["success"] = true;
    response["faculties"] = items.ExtractValue();

    return response.ExtractValue();
}

}  // namespace RumpelQuiz
