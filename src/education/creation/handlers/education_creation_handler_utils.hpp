#pragma once

#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>

#include <boost/uuid/uuid_io.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/server/http/http_request.hpp>
#include <userver/server/http/http_response.hpp>
#include <userver/server/http/http_status.hpp>

#include "education/creation/service/education_creation_service.hpp"

namespace RumpelQuiz::EducationCreationHttp {

inline userver::formats::json::Value Error(
    const userver::server::http::HttpRequest& request,
    userver::server::http::HttpStatus status, std::string_view error) {
  request.SetResponseStatus(status);
  request.GetHttpResponse().SetHeader(std::string_view{"Cache-Control"},
                                      "no-store");
  userver::formats::json::ValueBuilder response;
  response["success"] = false;
  response["error"] = std::string{error};
  return response.ExtractValue();
}

inline userver::formats::json::Value MakeResponse(
    const userver::server::http::HttpRequest& request,
    const EducationCreationResult& result, std::string_view id_field,
    std::string_view duplicate_error) {
  using userver::server::http::HttpStatus;

  if (const auto* success = std::get_if<EducationCreationSuccess>(&result)) {
    request.SetResponseStatus(HttpStatus::kCreated);
    request.GetHttpResponse().SetHeader(std::string_view{"Cache-Control"},
                                        "no-store");
    userver::formats::json::ValueBuilder response;
    response["success"] = true;
    response[std::string{id_field}] = boost::uuids::to_string(success->id);
    return response.ExtractValue();
  }

  switch (std::get<EducationCreationError>(result)) {
    case EducationCreationError::kInvalidRequest:
      return Error(request, HttpStatus::kBadRequest, "invalid_request");
    case EducationCreationError::kAccessDenied:
      return Error(request, HttpStatus::kForbidden, "university_access_denied");
    case EducationCreationError::kUniversityNotFound:
      return Error(request, HttpStatus::kNotFound, "university_not_found");
    case EducationCreationError::kInvalidFaculty:
      return Error(request, HttpStatus::kBadRequest, "invalid_faculty");
    case EducationCreationError::kInvalidProgram:
      return Error(request, HttpStatus::kBadRequest, "invalid_program");
    case EducationCreationError::kNameAlreadyExists:
      return Error(request, HttpStatus::kConflict, duplicate_error);
  }
  throw std::logic_error{"Unexpected EducationCreationError"};
}

}  // namespace RumpelQuiz::EducationCreationHttp
