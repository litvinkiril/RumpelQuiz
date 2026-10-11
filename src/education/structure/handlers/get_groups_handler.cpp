#include "get_groups_handler.hpp"

#include <stdexcept>
#include <string>
#include <string_view>

#include <boost/uuid/string_generator.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <userver/components/component_context.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/server/http/http_status.hpp>

#include "education/education_models.hpp"

namespace RumpelQuiz {

GetGroupsHandler::GetGroupsHandler(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : HttpHandlerJsonBase(config, context),
      structure_service_(
          context.FindComponent<EducationStructureService>()) {}

userver::formats::json::Value
GetGroupsHandler::HandleRequestJsonThrow(
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
    const bool by_program = request.HasArg("programm_id");

    const int filter_count =
        by_university + by_faculty + by_program;

    if (filter_count != 1) {
        request.SetResponseStatus(HttpStatus::kBadRequest);
        response["error"] = "invalid_structure_filter";
        return response.ExtractValue();
    }

    StructureFilterKind filter_kind;
    std::string filter_name;

    if (by_university) {
        filter_kind = StructureFilterKind::kUniversity;
        filter_name = "university_id";
    } else if (by_faculty) {
        filter_kind = StructureFilterKind::kFaculty;
        filter_name = "facultet_id";
    } else {
        filter_kind = StructureFilterKind::kProgram;
        filter_name = "programm_id";
    }

    boost::uuids::uuid filter_id;

    try {
        filter_id = boost::uuids::string_generator{}(
            request.GetArg(filter_name)
        );
    } catch (const std::runtime_error&) {
        request.SetResponseStatus(HttpStatus::kBadRequest);
        response["error"] = "invalid_" + filter_name;
        return response.ExtractValue();
    }

    const auto groups = structure_service_.GetGroups(
        StructureFilter{filter_kind, filter_id}
    );

    userver::formats::json::ValueBuilder items(
        userver::formats::json::Type::kArray
    );

    for (const auto& group : groups) {
        userver::formats::json::ValueBuilder item;

        item["id"] = boost::uuids::to_string(group.id);
        item["name"] = group.name;

        items.PushBack(item.ExtractValue());
    }

    response["success"] = true;
    response["groups"] = items.ExtractValue();

    return response.ExtractValue();
}

}  // namespace RumpelQuiz
