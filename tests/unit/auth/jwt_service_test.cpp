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

  const auto token = service.GenerateAccessToken(user_id);

  EXPECT_EQ(service.VerifyAccessToken(token), user_id);
}

UTEST(JwtService, RejectsTokenWithModifiedSignature) {
  const RumpelQuiz::JwtService service{std::string{kSecret}};
  const auto user_id =
      boost::uuids::string_generator{}("5cd5ba59-6d31-47f3-ac76-f8aa0b4d6f30");
  auto token = service.GenerateAccessToken(user_id);
  const auto signature_start = token.rfind('.') + 1;
  token[signature_start] = token[signature_start] == 'a' ? 'b' : 'a';

  EXPECT_THROW(service.VerifyAccessToken(token), RumpelQuiz::JwtError);
}

UTEST(JwtService, RejectsMalformedToken) {
  const RumpelQuiz::JwtService service{std::string{kSecret}};

  EXPECT_THROW(service.VerifyAccessToken("not-a-jwt"), RumpelQuiz::JwtError);
}
