#include <boost/uuid/random_generator.hpp>
#include <userver/formats/json.hpp>
#include <userver/utest/utest.hpp>
#include "media/service/image_validation.hpp"
#include "quiz/authoring/handlers/serialization/quiz_json.hpp"
#include "quiz/authoring/service/quiz_validation.hpp"
using namespace RumpelQuiz;
UTEST(Quiz, DraftAndReady) {
  SaveQuizRequest in;
  in.university_id = boost::uuids::random_generator{}();
  QuizValidation v;
  EXPECT_TRUE(v.CheckCommon(in).empty());
  EXPECT_FALSE(v.CheckReady(in).empty());
  in.name = "Quiz";
  in.questions.push_back(
      {"Q",
       QuestionType::kSingle,
       std::nullopt,
       std::nullopt,
       {{"A", true, std::nullopt}, {"B", false, std::nullopt}}});
  EXPECT_TRUE(v.CheckReady(in).empty());
  in.questions[0].answers[1].is_correct = true;
  EXPECT_FALSE(v.CheckReady(in).empty());
  in.questions[0].type = QuestionType::kMulty;
  EXPECT_TRUE(v.CheckReady(in).empty());
  in.questions[0].time_seconds = 0;
  EXPECT_FALSE(v.CheckCommon(in).empty());
  in.name = "\xC2\xA0";
  EXPECT_FALSE(v.CheckReady(in).empty());
}
UTEST(Quiz, ParserRejectsWrongTypes) {
  auto result = ParseSaveQuizRequest(userver::formats::json::FromString(
      R"({"university_id":"00000000-0000-4000-8000-000000000001","status":"draft","questions":false})"));
  ASSERT_TRUE(std::holds_alternative<QuizParseError>(result));
  EXPECT_EQ(std::get<QuizParseError>(result).details[0].field, "questions");
  result = ParseSaveQuizRequest(userver::formats::json::FromString(
      R"({"university_id":"00000000-0000-4000-8000-000000000001","status":"draft"})"));
  ASSERT_TRUE(std::holds_alternative<SaveQuizRequest>(result));
  EXPECT_TRUE(std::get<SaveQuizRequest>(result).questions.empty());
}
UTEST(Media, RejectsFakeAndOversizedImages) {
  EXPECT_EQ(std::get<UploadImageError>(ValidateImage("fake.png")),
            UploadImageError::kUnsupportedFormat);
  EXPECT_EQ(
      std::get<UploadImageError>(ValidateImage(std::string(5242881, 'x'))),
      UploadImageError::kInvalidImage);
  EXPECT_EQ(std::get<UploadImageError>(
                ValidateImage(std::string_view("\x89PNG\r\n\x1a\n", 8))),
            UploadImageError::kInvalidImage);
  EXPECT_EQ(std::get<UploadImageError>(
                ValidateImage(std::string_view("\xff\xd8\xff", 3))),
            UploadImageError::kInvalidImage);
}
