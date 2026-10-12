#include "create_group_handler.hpp"

#include <stdexcept>
#include <string>

#include <boost/uuid/string_generator.hpp>
#include <userver/components/component_context.hpp>
#include <userver/formats/json/exception.hpp>

#include "auth/middleware/jwt/auth_context.hpp"
#include "education_creation_handler_utils.hpp"

namespace RumpelQuiz {

CreateGroupHandler::CreateGroupHandler(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : HttpHandlerJsonBase(config, context),
      creation_service_(context.FindComponent<EducationCreationService>()) {}

userver::formats::json::Value CreateGroupHandler::HandleRequestJsonThrow(
    const userver::server::http::HttpRequest& request,
    const userver::formats::json::Value& request_body,
    userver::server::request::RequestContext& context) const {
  using userver::server::http::HttpStatus;

  CreateGroupRequest data{};
  try {
    data.university_id = boost::uuids::string_generator{}(
        request_body["university_id"].As<std::string>());
    data.program_id = boost::uuids::string_generator{}(
        request_body["program_id"].As<std::string>());
    data.name = request_body["name"].As<std::string>();
  } catch (const userver::formats::json::Exception&) {
    return EducationCreationHttp::Error(request, HttpStatus::kBadRequest,
                                        "invalid_request");
  } catch (const std::runtime_error&) {
    return EducationCreationHttp::Error(request, HttpStatus::kBadRequest,
                                        "invalid_request");
  }

  const auto& user_id = GetAuthenticatedUserId(context);
  const auto result = creation_service_.CreateGroup(user_id, data);

  return EducationCreationHttp::MakeResponse(
      request, result, "group_id", "group_already_exists");
}

}  // namespace RumpelQuiz
