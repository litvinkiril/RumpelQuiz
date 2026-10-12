#include "get_group_members_handler.hpp"

#include <stdexcept>
#include <string_view>
#include <vector>

#include <boost/uuid/string_generator.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <userver/components/component_context.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/server/http/http_status.hpp>

namespace RumpelQuiz {
namespace {

userver::formats::json::Value SerializeMembers(
    const std::vector<GroupMember>& members) {
    userver::formats::json::ValueBuilder items(
        userver::formats::json::Type::kArray);
    for (const auto& member : members) {
        userver::formats::json::ValueBuilder item;
        item["id"] = boost::uuids::to_string(member.id);
        item["first_name"] = member.first_name
            ? userver::formats::json::ValueBuilder{*member.first_name}
            : userver::formats::json::ValueBuilder{};
        item["last_name"] = member.last_name
            ? userver::formats::json::ValueBuilder{*member.last_name}
            : userver::formats::json::ValueBuilder{};
        item["middle_name"] = member.middle_name
            ? userver::formats::json::ValueBuilder{*member.middle_name}
            : userver::formats::json::ValueBuilder{};
        item["avatar_url"] = member.avatar_url
            ? userver::formats::json::ValueBuilder{*member.avatar_url}
            : userver::formats::json::ValueBuilder{};
        items.PushBack(item.ExtractValue());
    }
    return items.ExtractValue();
}

}  // namespace

GetGroupMembersHandler::GetGroupMembersHandler(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : HttpHandlerJsonBase(config, context),
      info_service_(context.FindComponent<EducationInfoService>()) {}

userver::formats::json::Value
GetGroupMembersHandler::HandleRequestJsonThrow(
    const userver::server::http::HttpRequest& request,
    const userver::formats::json::Value&,
    userver::server::request::RequestContext&) const {
    using userver::server::http::HttpStatus;

    request.GetHttpResponse().SetHeader(
        std::string_view{"Cache-Control"}, "no-store");
    userver::formats::json::ValueBuilder response;
    response["success"] = false;

    boost::uuids::uuid group_id;
    try {
        group_id = boost::uuids::string_generator{}(
            request.GetPathArg("group_id"));
    } catch (const std::runtime_error&) {
        request.SetResponseStatus(HttpStatus::kBadRequest);
        response["error"] = "invalid_group_id";
        return response.ExtractValue();
    }

    const auto members = info_service_.GetGroupMembers(group_id);
    if (!members) {
        request.SetResponseStatus(HttpStatus::kNotFound);
        response["error"] = "group_not_found";
        return response.ExtractValue();
    }

    response["success"] = true;
    response["teachers"] = SerializeMembers(members->teachers);
    response["students"] = SerializeMembers(members->students);
    return response.ExtractValue();
}

}  // namespace RumpelQuiz
