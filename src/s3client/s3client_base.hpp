#pragma once
#include <string_view>
#include "save_image_response.hpp"

namespace RumpelQuiz {
struct LoadImageResponse {
  bool success = false;
  bool found = false;
  std::string contents;
};

class S3ClientBase {
 public:
  virtual ~S3ClientBase() = default;
  // Raw bytes (not base64), generated storage key, canonical MIME type.
  // The service validates image contents and permissions before calling.
  virtual SaveImageResponse SaveImage(std::string_view contents,
                                     std::string_view key,
                                     std::string_view content_type) const = 0;
  virtual LoadImageResponse LoadImage(std::string_view key) const = 0;
  virtual std::string DownloadUrl(std::string_view) const { return {}; }
};
}  // namespace RumpelQuiz
