#pragma once

#include <stdexcept>
#include <string>
#include <string_view>

#include <boost/uuid/string_generator.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <userver/formats/json/value_builder.hpp>
#include <userver/server/http/http_request.hpp>
#include <userver/server/http/http_status.hpp>

#include "education/search/service/education_search_service.hpp"

namespace RumpelQuiz::education_search {

inline userver::formats::json::Value Search(
    const userver::server::http::HttpRequest& request,
    const EducationSearchService& service, bool programs) {
  using userver::server::http::HttpStatus;
  request.GetHttpResponse().SetHeader(std::string_view{"Cache-Control"},
                                      "no-store");
  userver::formats::json::ValueBuilder response;
  response["success"] = false;

  boost::uuids::uuid university_id;
  try {
    university_id =
        boost::uuids::string_generator{}(request.GetArg("university_id"));
  } catch (const std::runtime_error&) {
    request.SetResponseStatus(HttpStatus::kBadRequest);
    response["error"] = "invalid_university_id";
    return response.ExtractValue();
  }

  const auto& raw_name = request.GetArg("name");
  const auto first = raw_name.find_first_not_of(" \t\r\n\f\v");
  if (first == std::string::npos) {
    request.SetResponseStatus(HttpStatus::kBadRequest);
    response["error"] = "invalid_name";
    return response.ExtractValue();
  }
  const auto last = raw_name.find_last_not_of(" \t\r\n\f\v");
  const auto name = raw_name.substr(first, last - first + 1);
  const auto matches = programs ? service.SearchPrograms(university_id, name)
                                : service.SearchGroups(university_id, name);

  userver::formats::json::ValueBuilder items(
      userver::formats::json::Type::kArray);
  if (matches.size() > 30) {
    response["error"] = "too_many_results";
    response["message"] = "Введите более точное название";
  } else {
    response["success"] = true;
    for (const auto& match : matches) {
      userver::formats::json::ValueBuilder item;
      item["id"] = boost::uuids::to_string(match.id);
      item["name"] = match.name;
      userver::formats::json::ValueBuilder faculties(
          userver::formats::json::Type::kArray);
      for (const auto& faculty : match.facultet_names) {
        faculties.PushBack(faculty);
      }
      item["facultet_names"] = faculties.ExtractValue();
      if (!programs) {
        if (match.program_name) {
          item["program_name"] = *match.program_name;
        } else {
          item["program_name"] = nullptr;
        }
      }
      items.PushBack(item.ExtractValue());
    }
  }
  response[programs ? "programs" : "groups"] = items.ExtractValue();
  return response.ExtractValue();
}

}  // namespace RumpelQuiz::education_search
