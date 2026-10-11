#include "get_programs_handler.hpp"

#include <stdexcept>
#include <string_view>

#include <boost/uuid/string_generator.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <userver/components/component_context.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/server/http/http_status.hpp>

#include "education/education_models.hpp"

namespace RumpelQuiz {

GetProgramsHandler::GetProgramsHandler(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : HttpHandlerJsonBase(config, context),
      structure_service_(
          context.FindComponent<EducationStructureService>()) {}

userver::formats::json::Value
GetProgramsHandler::HandleRequestJsonThrow(
    const userver::server::http::HttpRequest& request,
    const userver::formats::json::Value&,
    userver::server::request::RequestContext&) const {
    using userver::server::http::HttpStatus;

    request.GetHttpResponse().SetHeader(
        std::string_view{"Cache-Control"}, "no-store"
    );

    userver::formats::json::ValueBuilder response;
    response["success"] = false;

    const bool by_university = request.HasArg("university_id");
    const bool by_faculty = request.HasArg("facultet_id");

    // Оба фильтра переданы или оба отсутствуют.
    if (!(by_university ^ by_faculty)) {
        request.SetResponseStatus(HttpStatus::kBadRequest);
        response["error"] = "invalid_structure_filter";
        return response.ExtractValue();
    }

    const auto filter_kind = by_university
        ? StructureFilterKind::kUniversity
        : StructureFilterKind::kFaculty;

    const auto filter_name = by_university
        ? "university_id"
        : "facultet_id";

    boost::uuids::uuid filter_id;

    try {
        filter_id = boost::uuids::string_generator{}(
            request.GetArg(filter_name)
        );
    } catch (const std::runtime_error&) {
        request.SetResponseStatus(HttpStatus::kBadRequest);
        response["error"] = by_university
            ? "invalid_university_id"
            : "invalid_facultet_id";
        return response.ExtractValue();
    }

    const auto programs = structure_service_.GetPrograms(
        StructureFilter{filter_kind, filter_id}
    );

    userver::formats::json::ValueBuilder items(
        userver::formats::json::Type::kArray
    );

    for (const auto& program : programs) {
        userver::formats::json::ValueBuilder item;

        item["id"] = boost::uuids::to_string(program.id);
        item["name"] = program.name;

        items.PushBack(item.ExtractValue());
    }

    response["success"] = true;
    response["programs"] = items.ExtractValue();

    return response.ExtractValue();
}

}  // namespace RumpelQuiz
