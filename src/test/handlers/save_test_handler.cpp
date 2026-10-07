#include "save_test_handler.hpp"

#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <boost/uuid/string_generator.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <userver/components/component_context.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/server/http/http_response.hpp>
#include <userver/server/http/http_status.hpp>

#include "auth/middleware/jwt/auth_context.hpp"
#include "test/handlers/serialisation/test_json.hpp"
#include "test/test_models.hpp"

namespace RumpelQuiz {
namespace {

using HttpStatus = userver::server::http::HttpStatus;
using JsonValue = userver::formats::json::Value;
using JsonBuilder = userver::formats::json::ValueBuilder;

JsonValue MakeErrorResponse(const userver::server::http::HttpRequest& request,
                            HttpStatus status, std::string_view code,
                            const std::vector<TestFieldError>& details = {}) {
  request.SetResponseStatus(status);

  JsonBuilder body;
  body["success"] = false;
  body["error"] = std::string{code};

  JsonBuilder errors(userver::formats::json::Type::kArray);

  for (const auto& detail : details) {
    JsonBuilder item;
    item["field"] = detail.field;
    item["message"] = detail.message;
    errors.PushBack(item.ExtractValue());
  }

  body["details"] = errors.ExtractValue();
  return body.ExtractValue();
}

JsonValue MakeServiceErrorResponse(
    const userver::server::http::HttpRequest& request,
    const SaveTestFailure& failure) {
  switch (failure.code) {
    case SaveTestError::kAccessDenied:
      return MakeErrorResponse(request, HttpStatus::kForbidden,
                               "test_access_denied");

    case SaveTestError::kNotFound:
      return MakeErrorResponse(request, HttpStatus::kNotFound,
                               "test_not_found");

    case SaveTestError::kRevisionConflict:
      return MakeErrorResponse(request, HttpStatus::kConflict,
                               "test_revision_conflict");

    case SaveTestError::kPublished:
      return MakeErrorResponse(request, HttpStatus::kConflict,
                               "test_published");

    case SaveTestError::kValidationFailed:
      return MakeErrorResponse(request, HttpStatus::kBadRequest,
                               "test_validation_failed", failure.details);
  }

  throw std::logic_error("Unexpected SaveTestError");
}

}  // namespace

SaveTestHandler::SaveTestHandler(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : HttpHandlerJsonBase(config, context),
      test_service_(context.FindComponent<TestService>()) {}

JsonValue SaveTestHandler::HandleRequestJsonThrow(
    const userver::server::http::HttpRequest& request,
    const JsonValue& request_body,
    userver::server::request::RequestContext& context) const {
  request.GetHttpResponse().SetHeader(std::string_view{"Cache-Control"},
                                      "no-store");

  const auto& user_id = GetAuthenticatedUserId(context);

  std::optional<boost::uuids::uuid> test_id;

  if (request.HasPathArg("test_id")) {
    try {
      test_id = boost::uuids::string_generator{}(request.GetPathArg("test_id"));
    } catch (const std::runtime_error&) {
      return MakeErrorResponse(request, HttpStatus::kBadRequest,
                               "invalid_test_id");
    }

    if (test_id->is_nil()) {
      return MakeErrorResponse(request, HttpStatus::kBadRequest,
                               "invalid_test_id");
    }
  }

  const auto parsed = ParseSaveTestRequest(request_body);

  if (const auto* error = std::get_if<TestParseError>(&parsed)) {
    return MakeErrorResponse(request, HttpStatus::kBadRequest,
                             "invalid_request", error->details);
  }

  const auto& input = std::get<SaveTestRequest>(parsed);

  if (test_id && !input.revision.has_value()) {
    return MakeErrorResponse(request, HttpStatus::kBadRequest,
                             "revision_required",
                             {{"revision", "Передайте текущую версию теста."}});
  }

  if (!test_id && input.revision.has_value()) {
    return MakeErrorResponse(
        request, HttpStatus::kBadRequest, "revision_not_allowed",
        {{"revision", "При создании теста revision передавать не нужно."}});
  }

  const auto result = test_service_.SaveTest(user_id, test_id, input);

  if (const auto* failure = std::get_if<SaveTestFailure>(&result)) {
    return MakeServiceErrorResponse(request, *failure);
  }

  const auto& saved = std::get<SaveTestSuccess>(result);

  request.SetResponseStatus(test_id ? HttpStatus::kOk : HttpStatus::kCreated);

  JsonBuilder body;
  body["success"] = true;
  body["test_id"] = boost::uuids::to_string(saved.test_id);
  body["status"] = SerializeTestStatus(saved.status);
  body["revision"] = saved.revision;

  return body.ExtractValue();
}

}  // namespace RumpelQuiz