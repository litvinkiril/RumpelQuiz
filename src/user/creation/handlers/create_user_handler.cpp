#include "create_user_handler.hpp"

#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>

#include <boost/uuid/string_generator.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <userver/components/component_context.hpp>
#include <userver/formats/json/exception.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/server/http/http_status.hpp>

#include "auth/middleware/jwt/auth_context.hpp"

namespace RumpelQuiz {

CreateUserHandler::CreateUserHandler(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : HttpHandlerJsonBase(config, context),
      user_creation_service_(context.FindComponent<UserCreationService>()) {}

userver::formats::json::Value CreateUserHandler::HandleRequestJsonThrow(
    const userver::server::http::HttpRequest& request,
    const userver::formats::json::Value& request_body,
    userver::server::request::RequestContext& context) const {
  using userver::server::http::HttpStatus;

  const auto make_error = [&](HttpStatus status, std::string_view error) {
    request.SetResponseStatus(status);

    userver::formats::json::ValueBuilder response;
    response["success"] = false;
    response["error"] = std::string{error};
    return response.ExtractValue();
  };

  const auto read_optional_string =
      [&](std::string_view key) -> std::optional<std::string> {
    if (!request_body.HasMember(key) || request_body[key].IsNull()) {
      return std::nullopt;
    }

    return request_body[key].As<std::string>();
  };

  CreateUserRequest data{};

  try {
    data.email = request_body["email"].As<std::string>();
    data.password = request_body["password"].As<std::string>();
    data.first_name = request_body["first_name"].As<std::string>();
    data.last_name = request_body["last_name"].As<std::string>();
    data.middle_name = read_optional_string("middle_name");
    data.description = read_optional_string("description");
    data.role = request_body["role"].As<std::string>();

    data.university_id = boost::uuids::string_generator{}(
        request_body["university"].As<std::string>());

    if (const auto faculty = read_optional_string("facultet_id")) {
      data.facultet_id = boost::uuids::string_generator{}(*faculty);
    }
  } catch (const userver::formats::json::Exception&) {
    return make_error(HttpStatus::kBadRequest, "invalid_request");
  } catch (const std::runtime_error&) {
    // Некорректный формат UUID.
    return make_error(HttpStatus::kBadRequest, "invalid_request");
  }

  const auto& actor_user_id = GetAuthenticatedUserId(context);
  const auto result = user_creation_service_.CreateUser(actor_user_id, data);

  if (const auto* success = std::get_if<CreateUserSuccess>(&result)) {
    request.SetResponseStatus(HttpStatus::kCreated);

    userver::formats::json::ValueBuilder response;
    response["success"] = true;
    response["user_id"] = boost::uuids::to_string(success->user_id);
    return response.ExtractValue();
  }

  switch (std::get<CreateUserError>(result)) {
    case CreateUserError::kInvalidRequest:
      return make_error(HttpStatus::kBadRequest, "invalid_request");

    case CreateUserError::kAccessDenied:
      return make_error(HttpStatus::kForbidden, "university_access_denied");

    case CreateUserError::kEmailAlreadyExists:
      return make_error(HttpStatus::kConflict, "email_already_exists");

    case CreateUserError::kUniversityNotFound:
      return make_error(HttpStatus::kNotFound, "university_not_found");

    case CreateUserError::kInvalidFaculty:
      return make_error(HttpStatus::kBadRequest, "invalid_faculty");
  }

  throw std::logic_error{"Unexpected CreateUserError"};
}

}  // namespace RumpelQuiz