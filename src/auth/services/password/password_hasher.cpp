#include "password_hasher.hpp"

#include <crypt.h>
#include <openssl/rand.h>
#include <array>
#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

namespace RumpelQuiz {

std::string PasswordHasher::HashPassword(std::string_view password) const {
  constexpr unsigned long kBcryptCost = 12;
  std::array<unsigned char, 16> random_bytes{};
  std::array<char, CRYPT_GENSALT_OUTPUT_SIZE> salt{};

  if (RAND_bytes(random_bytes.data(), static_cast<int>(random_bytes.size())) !=
      1) {
    throw std::runtime_error("Failed to generate password salt");
  }

  if (!crypt_gensalt_rn("$2b$", kBcryptCost,
                        reinterpret_cast<const char*>(random_bytes.data()),
                        static_cast<int>(random_bytes.size()), salt.data(),
                        static_cast<int>(salt.size()))) {
    throw std::runtime_error("Failed to generate bcrypt salt");
  }

  const std::string password_string{password};
  void* crypt_data = nullptr;
  int crypt_data_size = 0;

  const char* hash = crypt_ra(password_string.c_str(), salt.data(), &crypt_data,
                              &crypt_data_size);

  if (!hash) {
    std::free(crypt_data);
    throw std::runtime_error("Failed to hash password");
  }

  const std::string result{hash};
  std::free(crypt_data);
  return result;
}

bool PasswordHasher::VerifyPassword(std::string_view password,
                                    std::string_view password_hash) const {
  const std::string password_string{password};
  const std::string hash_string{password_hash};
  void* crypt_data = nullptr;
  int crypt_data_size = 0;

  const char* calculated_hash =
      crypt_ra(password_string.c_str(), hash_string.c_str(), &crypt_data,
               &crypt_data_size);

  if (!calculated_hash) {
    std::free(crypt_data);
    return false;
  }

  const bool matches = hash_string == calculated_hash;
  std::free(crypt_data);
  return matches;
}

}  // namespace RumpelQuiz
