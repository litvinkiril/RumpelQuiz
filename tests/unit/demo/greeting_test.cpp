#include "demo/greeting/greeting.hpp"

#include <userver/utest/utest.hpp>

using RumpelQuiz::UserType;

UTEST(SayHelloTo, Basic) {
    EXPECT_EQ(RumpelQuiz::SayHelloTo("Developer", UserType::kFirstTime), "Hello, Developer!\n");
    EXPECT_EQ(RumpelQuiz::SayHelloTo({}, UserType::kFirstTime), "Hello, unknown user!\n");

    EXPECT_EQ(RumpelQuiz::SayHelloTo("Developer", UserType::kKnown), "Hi again, Developer!\n");
    EXPECT_EQ(RumpelQuiz::SayHelloTo({}, UserType::kKnown), "Hi again, unknown user!\n");
}
