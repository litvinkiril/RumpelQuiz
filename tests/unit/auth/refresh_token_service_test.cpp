#include "auth/services/refresh_token/refresh_token_service.hpp"
#include <userver/utest/utest.hpp>

UTEST(RefreshTokenService, GeneratesDistinctTokensAndVerifiesHash) {
  const RumpelQuiz::RefreshTokenService service;
  const auto token = service.Generate();
  const auto other = service.Generate();
  EXPECT_EQ(token.size(), 64);
  EXPECT_NE(token, other);
  const auto hash = service.Hash(token);
  EXPECT_NE(hash, token);
  EXPECT_TRUE(service.Verify(token, hash));
  EXPECT_FALSE(service.Verify(other, hash));
  EXPECT_FALSE(service.Verify("", hash));
  EXPECT_FALSE(service.Verify(token, "invalid"));
  EXPECT_EQ(service.Hash("abc"),
            "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}
