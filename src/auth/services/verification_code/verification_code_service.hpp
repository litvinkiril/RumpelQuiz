#pragma once

#include <string>

namespace RumpelQuiz {

struct VerificationCode {
    std::string plain;
    std::string hash;
};

class VerificationCodeService {
public:
    VerificationCode Generate() const;
};

}