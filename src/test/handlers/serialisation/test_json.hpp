#pragma once
#include <string>
#include <userver/formats/json/value.hpp>
#include "test/test_models.hpp"

namespace RumpelQuiz {
ParseSaveTestResult ParseSaveTestRequest(
    const userver::formats::json::Value& body);
std::string SerializeTestStatus(TestStatus status);
}  // namespace RumpelQuiz
