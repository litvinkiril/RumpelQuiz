#include "s3client.hpp"

#include <algorithm>
#include <exception>
#include <optional>
#include <string>
#include <stdexcept>
#include <utility>

#include <userver/engine/task/cancel.hpp>

namespace RumpelQuiz {
namespace {

bool IsSafeKey(std::string_view key) {
  if (key.empty() || key.size() > 1024 || key.front() == '/' ||
      key.back() == '/') {
    return false;
  }
  if (!std::all_of(key.begin(), key.end(), [](unsigned char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
               (c >= '0' && c <= '9') || c == '/' || c == '.' || c == '_' ||
               c == '-';
      })) {
    return false;
  }
  while (!key.empty()) {
    const auto slash = key.find('/');
    const auto segment = key.substr(0, slash);
    if (segment.empty() || segment == "." || segment == "..") return false;
    if (slash == std::string_view::npos) break;
    key.remove_prefix(slash + 1);
  }
  return true;
}

}  // namespace

S3Client::S3Client(std::shared_ptr<userver::s3api::Client> client, S3ClientConfig config)
    : client_(std::move(client)), config_(std::move(config)) {
  if (!client_) throw std::invalid_argument("S3 client must not be null");
  const auto valid_host = [](std::string_view host) {
    return !host.empty() && std::all_of(host.begin(), host.end(), [](unsigned char c) {
      return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
             (c >= '0' && c <= '9') || c == '-' || c == '.';
    }) && host.front() != '.' && host.back() != '.' &&
        host.find("..") == std::string_view::npos;
  };
  if (!valid_host(config_.endpoint) || !valid_host(config_.bucket_name)) {
    throw std::invalid_argument("Invalid S3 endpoint or bucket");
  }
}

SaveImageResponse S3Client::SaveImage(std::string_view contents, std::string_view key,
                         std::string_view content_type) const {
  if (!IsSafeKey(key)) return {false, "Invalid image storage key", {}};
  if (contents.empty()) return {false, "Image contents are empty", {}};
  if (content_type != "image/jpeg" && content_type != "image/png" &&
      content_type != "image/webp" && content_type != "image/gif") {
    return {false, "Unsupported image content type", {}};
  }
  if (!client_) return {false, "S3 storage is disabled", {}};

  try {
    client_->PutObject(key, std::string{contents}, std::nullopt, content_type);
  } catch (const std::exception&) {
    // Preserve cancellation and keep transport details/credentials out of
    // errors.
    userver::engine::current_task::CancellationPoint();
    return {false, "S3 image upload failed", {}};
  }
  return {true, {}, "https://" + config_.bucket_name + "." +
                        config_.endpoint + "/" + std::string{key}};
}

}  // namespace RumpelQuiz

