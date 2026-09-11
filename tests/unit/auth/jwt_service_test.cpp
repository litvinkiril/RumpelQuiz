#include "auth/services/jwt/jwt_service.hpp"

#include <chrono>
#include <string>

#include <boost/uuid/string_generator.hpp>

#include <userver/utest/utest.hpp>

namespace {

constexpr std::string_view kSecret =
    "unit-test-jwt-secret-that-is-at-least-32-characters";

}  // namespace

UTEST(JwtService, GeneratesAndVerifiesAccessToken) {
  const RumpelQuiz::JwtService service{std::string{kSecret}};
  const auto user_id =
      boost::uuids::string_generator{}("5cd5ba59-6d31-47f3-ac76-f8aa0b4d6f30");
  const auto session_id =
      boost::uuids::string_generator{}("17b5377d-ecad-4a59-94da-380a19565971");

  const auto token = service.GenerateAccessToken(user_id, session_id);

  EXPECT_EQ(service.VerifyAccessToken(token).user_id, user_id);
  EXPECT_EQ(service.VerifyAccessToken(token).session_id, session_id);
}

UTEST(JwtService, RejectsTokenWithModifiedSignature) {
  const RumpelQuiz::JwtService service{std::string{kSecret}};
  const auto user_id =
      boost::uuids::string_generator{}("5cd5ba59-6d31-47f3-ac76-f8aa0b4d6f30");
  auto token = service.GenerateAccessToken(user_id, user_id);
  const auto signature_start = token.rfind('.') + 1;
  token[signature_start] = token[signature_start] == 'a' ? 'b' : 'a';

  EXPECT_THROW(service.VerifyAccessToken(token), RumpelQuiz::JwtError);
}

UTEST(JwtService, RejectsMalformedToken) {
  const RumpelQuiz::JwtService service{std::string{kSecret}};

  EXPECT_THROW(service.VerifyAccessToken("not-a-jwt"), RumpelQuiz::JwtError);
}

UTEST(JwtService, RejectsInvalidConfiguration) {
  EXPECT_THROW(RumpelQuiz::JwtService("short"), std::invalid_argument);
  EXPECT_THROW(
      (RumpelQuiz::JwtService{std::string{kSecret}, std::chrono::seconds{0}}),
      std::invalid_argument);
  EXPECT_THROW(
      (RumpelQuiz::JwtService{std::string{kSecret}, std::chrono::seconds{-1}}),
      std::invalid_argument);
}

UTEST(JwtService, RejectsAnotherSigningKeyAndMalformedSegments) {
  const RumpelQuiz::JwtService service{std::string{kSecret}};
  const RumpelQuiz::JwtService other{
      "another-unit-test-secret-with-at-least-32-characters"};
  const auto id =
      boost::uuids::string_generator{}("5cd5ba59-6d31-47f3-ac76-f8aa0b4d6f30");
  EXPECT_THROW(other.VerifyAccessToken(service.GenerateAccessToken(id, id)),
               RumpelQuiz::JwtError);
  for (const auto token : {"", ".", "..", "a.b.c.d", "!.e30.!", "a=.b.c"}) {
    EXPECT_THROW(service.VerifyAccessToken(token), RumpelQuiz::JwtError);
  }
}
