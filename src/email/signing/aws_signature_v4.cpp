#include "aws_signature_v4.hpp"

#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <array>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace RumpelQuiz {

namespace {

constexpr std::string_view kHost = "postbox.cloud.yandex.net";

constexpr std::string_view kPath = "/v2/email/outbound-emails";

constexpr std::string_view kRegion = "ru-central1";

constexpr std::string_view kService = "ses";

constexpr std::string_view kAlgorithm = "AWS4-HMAC-SHA256";

std::string ToHex(const unsigned char* data, std::size_t size) {
  std::ostringstream stream;

  stream << std::hex << std::setfill('0');

  for (std::size_t i = 0; i < size; ++i) {
    stream << std::setw(2) << static_cast<int>(data[i]);
  }

  return stream.str();
}

std::string Sha256Hex(std::string_view data) {
  std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
  unsigned int digest_length = 0;

  EVP_MD_CTX* context = EVP_MD_CTX_new();

  if (!context) {
    throw std::runtime_error("Failed to create EVP context");
  }

  if (EVP_DigestInit_ex(context, EVP_sha256(), nullptr) != 1) {
    EVP_MD_CTX_free(context);

    throw std::runtime_error("Failed to initialize SHA256");
  }

  if (EVP_DigestUpdate(context, data.data(), data.size()) != 1) {
    EVP_MD_CTX_free(context);

    throw std::runtime_error("Failed to update SHA256");
  }

  if (EVP_DigestFinal_ex(context, digest.data(), &digest_length) != 1) {
    EVP_MD_CTX_free(context);

    throw std::runtime_error("Failed to finalize SHA256");
  }

  EVP_MD_CTX_free(context);

  return ToHex(digest.data(), digest_length);
}

std::vector<unsigned char> HmacSha256(const std::vector<unsigned char>& key,
                                      std::string_view data) {
  std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
  unsigned int digest_length = 0;

  const auto* result =
      HMAC(EVP_sha256(), key.data(), static_cast<int>(key.size()),
           reinterpret_cast<const unsigned char*>(data.data()), data.size(),
           digest.data(), &digest_length);

  if (!result) {
    throw std::runtime_error("Failed to calculate HMAC-SHA256");
  }

  return std::vector<unsigned char>(digest.begin(),
                                    digest.begin() + digest_length);
}

std::vector<unsigned char> HmacSha256(std::string_view key,
                                      std::string_view data) {
  const std::vector<unsigned char> key_bytes(key.begin(), key.end());

  return HmacSha256(key_bytes, data);
}

struct AwsTime {
  std::string full_date;
  std::string short_date;
};

AwsTime GetAwsTime() {
  const auto now = std::chrono::system_clock::now();

  const std::time_t time = std::chrono::system_clock::to_time_t(now);

  std::tm utc_time{};

#ifdef _WIN32
  gmtime_s(&utc_time, &time);
#else
  gmtime_r(&time, &utc_time);
#endif

  std::ostringstream full_date;
  full_date << std::put_time(&utc_time, "%Y%m%dT%H%M%SZ");

  std::ostringstream short_date;
  short_date << std::put_time(&utc_time, "%Y%m%d");

  return AwsTime{full_date.str(), short_date.str()};
}

}  // namespace

AwsSignatureV4::AwsSignatureV4(std::string key_id, std::string secret_key)
    : key_id_(std::move(key_id)), secret_key_(std::move(secret_key)) {}

AwsSignedHeaders AwsSignatureV4::SignPostboxRequest(
    std::string_view payload) const {
  const auto time = GetAwsTime();

  const std::string payload_hash = Sha256Hex(payload);

  const std::string canonical_headers =
      "content-type:application/json\n"
      "host:" +
      std::string{kHost} +
      "\n"
      "x-amz-content-sha256:" +
      payload_hash +
      "\n"
      "x-amz-date:" +
      time.full_date + "\n";

  const std::string signed_headers =
      "content-type;"
      "host;"
      "x-amz-content-sha256;"
      "x-amz-date";

  const std::string canonical_request = "POST\n" + std::string{kPath} +
                                        "\n"
                                        "\n" +
                                        canonical_headers + "\n" +
                                        signed_headers + "\n" + payload_hash;

  const std::string canonical_request_hash = Sha256Hex(canonical_request);

  const std::string credential_scope = time.short_date + "/" +
                                       std::string{kRegion} + "/" +
                                       std::string{kService} + "/aws4_request";

  const std::string string_to_sign = std::string{kAlgorithm} + "\n" +
                                     time.full_date + "\n" + credential_scope +
                                     "\n" + canonical_request_hash;

  const std::string aws_secret = "AWS4" + secret_key_;

  const auto date_key = HmacSha256(aws_secret, time.short_date);

  const auto region_key = HmacSha256(date_key, kRegion);

  const auto service_key = HmacSha256(region_key, kService);

  const auto signing_key = HmacSha256(service_key, "aws4_request");

  const auto signature_binary = HmacSha256(signing_key, string_to_sign);

  const std::string signature =
      ToHex(signature_binary.data(), signature_binary.size());

  const std::string authorization =
      std::string{kAlgorithm} + " Credential=" + key_id_ + "/" +
      credential_scope + ", SignedHeaders=" + signed_headers +
      ", Signature=" + signature;

  return AwsSignedHeaders{authorization, time.full_date, payload_hash};
}

}  // namespace RumpelQuiz
