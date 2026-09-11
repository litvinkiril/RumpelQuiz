#include "refresh_token_service.hpp"

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <array>
#include <stdexcept>

namespace RumpelQuiz {
namespace {
std::string Hex(const unsigned char* bytes, std::size_t size) {
  constexpr char digits[] = "0123456789abcdef";
  std::string result(size * 2, '0');
  for (std::size_t i = 0; i < size; ++i) {
    result[2 * i] = digits[bytes[i] >> 4];
    result[2 * i + 1] = digits[bytes[i] & 15];
  }
  return result;
}
}  // namespace

std::string RefreshTokenService::Generate() const {
  std::array<unsigned char, 32> bytes{};
  if (RAND_bytes(bytes.data(), bytes.size()) != 1)
    throw std::runtime_error("Failed to generate refresh token");
  return Hex(bytes.data(), bytes.size());
}

std::string RefreshTokenService::Hash(std::string_view token) const {
  std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
  unsigned int size = 0;
  if (EVP_Digest(token.data(), token.size(), digest.data(), &size, EVP_sha256(),
                 nullptr) != 1)
    throw std::runtime_error("Failed to hash refresh token");
  return Hex(digest.data(), size);
}

bool RefreshTokenService::Verify(std::string_view token,
                                 std::string_view token_hash) const {
  if (token.size() != 64) return false;
  const auto actual = Hash(token);
  return actual.size() == token_hash.size() &&
         CRYPTO_memcmp(actual.data(), token_hash.data(), actual.size()) == 0;
}
}  // namespace RumpelQuiz
