#include <userver/utest/utest.hpp>
#include "auth/services/password/password_hasher.hpp"
#include "auth/services/verification_code/verification_code_service.hpp"
#include "email/signing/aws_signature_v4.hpp"

UTEST(PasswordHasher, SaltedHashesVerifyOnlyCorrectPassword) {
  const RumpelQuiz::PasswordHasher hasher;
  const auto first = hasher.HashPassword("пароль-with-unicode");
  const auto second = hasher.HashPassword("пароль-with-unicode");
  EXPECT_NE(first, second);
  EXPECT_TRUE(first.starts_with("$2"));
  EXPECT_TRUE(hasher.VerifyPassword("пароль-with-unicode", first));
  EXPECT_FALSE(hasher.VerifyPassword("wrong", first));
  EXPECT_FALSE(hasher.VerifyPassword("password", "invalid-hash"));
  EXPECT_FALSE(hasher.VerifyPassword("password", ""));
}

UTEST(VerificationCodeService, ProducesSixDigitsAndVerifiableHash) {
  const RumpelQuiz::VerificationCodeService generator;
  const RumpelQuiz::PasswordHasher hasher;
  const auto code = generator.Generate();
  EXPECT_EQ(code.plain.size(), 6);
  EXPECT_EQ(code.plain.find_first_not_of("0123456789"), std::string::npos);
  EXPECT_NE(code.hash, code.plain);
  EXPECT_TRUE(hasher.VerifyPassword(code.plain, code.hash));
  const auto wrong = code.plain == "000000" ? "111111" : "000000";
  EXPECT_FALSE(hasher.VerifyPassword(wrong, code.hash));
}

UTEST(AwsSignatureV4, HashesPayloadAndSignsExpectedScope) {
  const RumpelQuiz::AwsSignatureV4 signer{"test-key", "test-secret"};
  const auto result = signer.SignPostboxRequest("");
  EXPECT_EQ(result.content_sha256,
            "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
  EXPECT_EQ(result.amz_date.size(), 16);
  EXPECT_EQ(result.amz_date.back(), 'Z');
  EXPECT_NE(result.authorization.find("Credential=test-key/"),
            std::string::npos);
  EXPECT_NE(result.authorization.find("/ru-central1/ses/aws4_request"),
            std::string::npos);
  EXPECT_EQ(result.authorization.size() -
                result.authorization.find("Signature=") - 10,
            64);
  EXPECT_EQ(result.authorization.find("test-secret"), std::string::npos);
}
