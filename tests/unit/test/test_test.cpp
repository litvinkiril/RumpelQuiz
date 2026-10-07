#include <boost/uuid/random_generator.hpp>
#include <limits>
#include <userver/formats/json.hpp>
#include <userver/utest/utest.hpp>
#include "test/handlers/serialisation/test_json.hpp"
#include "test/service/test_validation.hpp"
using namespace RumpelQuiz;
UTEST(TestAuthoring, PublicationAndCommonLimits) {
  SaveTestRequest in;
  in.university_id = boost::uuids::random_generator{}();
  TestValidation v;
  EXPECT_TRUE(v.CheckCommon(in).empty());
  EXPECT_FALSE(v.CheckReady(in).empty());
  in.name = "Тест";
  in.questions.push_back(
      {"Q",
       QuestionType::kSingle,
       std::nullopt,
       {{"A", true, std::nullopt}, {"B", false, std::nullopt}}});
  for (auto status : {TestStatus::kPublic, TestStatus::kPrivate}) {
    in.status = status;
    EXPECT_TRUE(v.CheckCommon(in).empty());
    EXPECT_TRUE(v.CheckReady(in).empty());
  }
  in.questions[0].answers[1].is_correct = true;
  EXPECT_FALSE(v.CheckReady(in).empty());
  in.questions[0].type = QuestionType::kMulty;
  EXPECT_TRUE(v.CheckReady(in).empty());
  in.time_to_complete = 0;
  EXPECT_FALSE(v.CheckCommon(in).empty());
  in.time_to_complete = 60;
  in.revision = std::numeric_limits<std::int64_t>::max();
  EXPECT_FALSE(v.CheckCommon(in).empty());
  in.name = "\xC2\xA0\xE2\x80\x83";
  EXPECT_FALSE(v.CheckReady(in).empty());
}
UTEST(TestAuthoring, ParserRequiresWholeTestTimeAndRejectsQuestionTime) {
  const std::string base =
      R"({"university_id":"00000000-0000-4000-8000-000000000001","status":"private",)";
  for (
      const auto& [tail, field] :
      std::vector<std::pair<std::string, std::string>>{
          {R"("questions":[]})", "time_to_complete"},
          {R"("time_to_complete":"60"})", "time_to_complete"},
          {R"("time_to_complete":60,"questions":[{"time_seconds":null}]})",
           "questions[0].time_seconds"},
          {R"("time_to_complete":60,"questions":[{"answers":[{"is_correct":1}]}]})",
           "questions[0].answers[0].is_correct"}}) {
    auto r =
        ParseSaveTestRequest(userver::formats::json::FromString(base + tail));
    ASSERT_TRUE(std::holds_alternative<TestParseError>(r));
    EXPECT_EQ(std::get<TestParseError>(r).details[0].field, field);
  }
  auto r = ParseSaveTestRequest(
      userver::formats::json::FromString(base + R"("time_to_complete":60})"));
  ASSERT_TRUE(std::holds_alternative<SaveTestRequest>(r));
  EXPECT_EQ(std::get<SaveTestRequest>(r).status, TestStatus::kPrivate);
  EXPECT_EQ(SerializeTestStatus(TestStatus::kPublic), "public");
}
