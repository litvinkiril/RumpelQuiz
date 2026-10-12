#include "create_program_handler.hpp"

#include <stdexcept>
#include <string>

#include <boost/uuid/string_generator.hpp>
#include <userver/components/component_context.hpp>
#include <userver/formats/json/exception.hpp>

#include "auth/middleware/jwt/auth_context.hpp"
#include "education_creation_handler_utils.hpp"

namespace RumpelQuiz {

CreateProgramHandler::CreateProgramHandler(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : HttpHandlerJsonBase(config, context),
      creation_service_(context.FindComponent<EducationCreationService>()) {}

userver::formats::json::Value CreateProgramHandler::HandleRequestJsonThrow(
    const userver::server::http::HttpRequest& request,
    const userver::formats::json::Value& request_body,
    userver::server::request::RequestContext& context) const {
  using userver::server::http::HttpStatus;

  CreateProgramRequest data{};
  try {
    data.university_id = boost::uuids::string_generator{}(
        request_body["university_id"].As<std::string>());
    data.name = request_body["name"].As<std::string>();
    const auto faculties = request_body["faculty_ids"];
    if (!faculties.IsArray()) {
      return EducationCreationHttp::Error(request, HttpStatus::kBadRequest,
                                          "invalid_request");
    }
    data.faculty_ids.reserve(faculties.GetSize());
    for (const auto& faculty : faculties) {
      data.faculty_ids.push_back(
          boost::uuids::string_generator{}(faculty.As<std::string>()));
    }
  } catch (const userver::formats::json::Exception&) {
    return EducationCreationHttp::Error(request, HttpStatus::kBadRequest,
                                        "invalid_request");
  } catch (const std::runtime_error&) {
    return EducationCreationHttp::Error(request, HttpStatus::kBadRequest,
                                        "invalid_request");
  }

  const auto& user_id = GetAuthenticatedUserId(context);
  const auto result = creation_service_.CreateProgram(user_id, data);

  return EducationCreationHttp::MakeResponse(
      request, result, "program_id", "program_already_exists");
}

}  // namespace RumpelQuiz
