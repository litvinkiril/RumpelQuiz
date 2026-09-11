#include "jwt_service.hpp"

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <array>
#include <boost/uuid/string_generator.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <chrono>
#include <cstdint>
#include <exception>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <userver/components/component_config.hpp>
#include <userver/formats/json/serialize.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/yaml_config/merge_schemas.hpp>
#include <userver/yaml_config/schema.hpp>
#include <utility>
#include <vector>

namespace RumpelQuiz {
namespace {

constexpr std::string_view kJwtHeader = R"({"alg":"HS256","typ":"JWT"})";
constexpr auto kAllowedClockSkew = std::chrono::seconds{60};

std::string EncodeBase64Url(std::string_view value) {
  if (value.empty()) {
    return {};
  }

  std::string encoded(4 * ((value.size() + 2) / 3), '\0');
  const auto encoded_size =
      EVP_EncodeBlock(reinterpret_cast<unsigned char*>(encoded.data()),
                      reinterpret_cast<const unsigned char*>(value.data()),
                      static_cast<int>(value.size()));
  if (encoded_size < 0) {
    throw JwtError("Failed to encode JWT data");
  }

  encoded.resize(static_cast<std::size_t>(encoded_size));
  for (char& character : encoded) {
    if (character == '+') {
      character = '-';
    } else if (character == '/') {
      character = '_';
    }
  }
  while (!encoded.empty() && encoded.back() == '=') {
    encoded.pop_back();
  }
  return encoded;
}

std::string DecodeBase64Url(std::string_view value) {
  if (value.empty() || value.size() % 4 == 1) {
    throw JwtError("Invalid JWT base64url value");
  }

  std::string padded{value};
  for (char& character : padded) {
    const bool is_valid = (character >= 'A' && character <= 'Z') ||
                          (character >= 'a' && character <= 'z') ||
                          (character >= '0' && character <= '9') ||
                          character == '-' || character == '_';
    if (!is_valid) {
      throw JwtError("Invalid JWT base64url value");
    }
    if (character == '-') {
      character = '+';
    } else if (character == '_') {
      character = '/';
    }
  }
  while (padded.size() % 4 != 0) {
    padded.push_back('=');
  }

  std::string decoded(3 * padded.size() / 4, '\0');
  const auto decoded_size =
      EVP_DecodeBlock(reinterpret_cast<unsigned char*>(decoded.data()),
                      reinterpret_cast<const unsigned char*>(padded.data()),
                      static_cast<int>(padded.size()));
  if (decoded_size < 0) {
    throw JwtError("Invalid JWT base64url value");
  }

  std::size_t padding = 0;
  if (!padded.empty() && padded.back() == '=') {
    ++padding;
  }
  if (padded.size() > 1 && padded[padded.size() - 2] == '=') {
    ++padding;
  }
  decoded.resize(static_cast<std::size_t>(decoded_size) - padding);
  return decoded;
}

std::array<unsigned char, EVP_MAX_MD_SIZE> Sign(std::string_view value,
                                                std::string_view secret,
                                                unsigned int& signature_size) {
  std::array<unsigned char, EVP_MAX_MD_SIZE> signature{};
  if (secret.size() >
      static_cast<std::size_t>(std::numeric_limits<int>::max())) {
    throw JwtError("JWT secret is too long");
  }

  if (!HMAC(EVP_sha256(), secret.data(), static_cast<int>(secret.size()),
            reinterpret_cast<const unsigned char*>(value.data()), value.size(),
            signature.data(), &signature_size)) {
    throw JwtError("Failed to sign JWT");
  }
  return signature;
}

std::int64_t UnixTimeNow() {
  return std::chrono::duration_cast<std::chrono::seconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

}  // namespace

JwtService::JwtService(std::string secret,
                       std::chrono::seconds access_token_lifetime)
    : secret_(std::move(secret)),
      access_token_lifetime_(access_token_lifetime) {
  if (secret_.size() < 32) {
    throw std::invalid_argument(
        "JWT secret must contain at least 32 characters");
  }
  if (access_token_lifetime_ <= std::chrono::seconds::zero()) {
    throw std::invalid_argument("JWT access token lifetime must be positive");
  }
}

std::string JwtService::GenerateAccessToken(
    const boost::uuids::uuid& user_id,
    const boost::uuids::uuid& session_id) const {
  const auto issued_at = UnixTimeNow();

  userver::formats::json::ValueBuilder payload;
  payload["sub"] = boost::uuids::to_string(user_id);
  payload["user_id"] = boost::uuids::to_string(user_id);
  payload["session_id"] = boost::uuids::to_string(session_id);
  payload["iat"] = issued_at;
  payload["exp"] = issued_at + access_token_lifetime_.count();

  const std::string encoded_header = EncodeBase64Url(kJwtHeader);
  const std::string encoded_payload =
      EncodeBase64Url(userver::formats::json::ToString(payload.ExtractValue()));
  const std::string signing_input = encoded_header + "." + encoded_payload;

  unsigned int signature_size = 0;
  const auto signature = Sign(signing_input, secret_, signature_size);
  const std::string signature_bytes(
      reinterpret_cast<const char*>(signature.data()), signature_size);

  return signing_input + "." + EncodeBase64Url(signature_bytes);
}

AccessTokenClaims JwtService::VerifyAccessToken(std::string_view token) const {
  try {
    const auto first_dot = token.find('.');
    const auto second_dot = token.find('.', first_dot + 1);
    if (first_dot == std::string_view::npos ||
        second_dot == std::string_view::npos ||
        token.find('.', second_dot + 1) != std::string_view::npos) {
      throw JwtError("JWT must contain three segments");
    }

    const auto encoded_header = token.substr(0, first_dot);
    const auto encoded_payload =
        token.substr(first_dot + 1, second_dot - first_dot - 1);
    const auto encoded_signature = token.substr(second_dot + 1);

    const auto header =
        userver::formats::json::FromString(DecodeBase64Url(encoded_header));
    if (header["alg"].As<std::string>() != "HS256" ||
        header["typ"].As<std::string>() != "JWT") {
      throw JwtError("Unsupported JWT header");
    }

    const std::string signing_input{token.substr(0, second_dot)};
    unsigned int expected_size = 0;
    const auto expected = Sign(signing_input, secret_, expected_size);
    const auto actual = DecodeBase64Url(encoded_signature);
    if (actual.size() != expected_size ||
        CRYPTO_memcmp(actual.data(), expected.data(), expected_size) != 0) {
      throw JwtError("Invalid JWT signature");
    }

    const auto payload =
        userver::formats::json::FromString(DecodeBase64Url(encoded_payload));
    const auto subject = payload["user_id"].As<std::string>();
    const auto session = payload["session_id"].As<std::string>();
    if (payload["sub"].As<std::string>() != subject)
      throw JwtError("Inconsistent JWT subject");
    const auto issued_at = payload["iat"].As<std::int64_t>();
    const auto expires_at = payload["exp"].As<std::int64_t>();
    const auto now = UnixTimeNow();
    if (expires_at <= now) {
      throw JwtError("JWT has expired");
    }
    if (issued_at > now + kAllowedClockSkew.count() ||
        expires_at <= issued_at) {
      throw JwtError("Invalid JWT timestamps");
    }

    return {boost::uuids::string_generator{}(subject),
            boost::uuids::string_generator{}(session)};
  } catch (const JwtError&) {
    throw;
  } catch (const std::exception&) {
    throw JwtError("Malformed JWT");
  }
}

JwtServiceComponent::JwtServiceComponent(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      service_(config["secret"].As<std::string>(),
               std::chrono::seconds{
                   config["access-token-ttl-seconds"].As<std::int64_t>()}) {}

const JwtService& JwtServiceComponent::GetService() const noexcept {
  return service_;
}

userver::yaml_config::Schema JwtServiceComponent::GetStaticConfigSchema() {
  return userver::yaml_config::MergeSchemas<userver::components::ComponentBase>(
      R"(
type: object
description: JWT access token service configuration.
additionalProperties: false
properties:
    secret:
        type: string
        description: HS256 signing secret, at least 32 characters.
    access-token-ttl-seconds:
        type: integer
        minimum: 1
        description: Access token lifetime in seconds.
required:
  - secret
  - access-token-ttl-seconds
)");
}

}  // namespace RumpelQuiz
