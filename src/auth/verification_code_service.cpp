#include "verification_code_service.hpp"

#include <cstdint>
#include <cstdio>
#include <limits>
#include <stdexcept>
#include <string>

#include <openssl/rand.h>

#include "auth/password_hasher.hpp"

namespace RumpelQuiz {

VerificationCode VerificationCodeService::Generate() const {
    constexpr std::uint32_t kCodeRange = 1'000'000;
    constexpr std::uint32_t kAcceptedLimit =
        std::numeric_limits<std::uint32_t>::max() -
        (std::numeric_limits<std::uint32_t>::max() % kCodeRange);

    std::uint32_t random_value = 0;

    do {
        if (RAND_bytes(
                reinterpret_cast<unsigned char*>(&random_value),
                sizeof(random_value)
            ) != 1) {
            throw std::runtime_error(
                "Failed to generate verification code"
            );
        }
    } while (random_value >= kAcceptedLimit);

    const auto value = random_value % kCodeRange;

    char code_buffer[7];

    std::snprintf(
        code_buffer,
        sizeof(code_buffer),
        "%06u",
        value
    );

    const std::string plain_code{code_buffer};

    const PasswordHasher hasher;

    return VerificationCode{
        plain_code,
        hasher.HashPassword(plain_code)
    };
}

}
