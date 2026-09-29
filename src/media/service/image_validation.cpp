#include "image_validation.hpp"
#include <cstdint>
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#include <stb/stb_image.h>
#include <webp/decode.h>
namespace RumpelQuiz {
ImageValidationResult ValidateImage(std::string_view data) {
  if (data.empty() || data.size() > 5242880)
    return UploadImageError::kInvalidImage;
  const bool png =
      data.size() >= 8 &&
      data.substr(0, 8) == std::string_view("\x89PNG\r\n\x1a\n", 8);
  const bool jpeg = data.size() >= 3 &&
                    static_cast<unsigned char>(data[0]) == 255 &&
                    static_cast<unsigned char>(data[1]) == 216 &&
                    static_cast<unsigned char>(data[2]) == 255;
  const bool webp = data.size() >= 12 && data.substr(0, 4) == "RIFF" &&
                    data.substr(8, 4) == "WEBP";
  if (!png && !jpeg && !webp) return UploadImageError::kUnsupportedFormat;
  const auto* bytes = reinterpret_cast<const unsigned char*>(data.data());
  int w = 0, h = 0, channels = 0;
  if (webp) {
    if (!WebPGetInfo(bytes, data.size(), &w, &h))
      return UploadImageError::kInvalidImage;
  } else if (!stbi_info_from_memory(bytes, static_cast<int>(data.size()), &w,
                                    &h, &channels))
    return UploadImageError::kInvalidImage;
  if (w <= 0 || h <= 0 || w > 8192 || h > 8192 ||
      static_cast<std::int64_t>(w) * h > 12000000)
    return UploadImageError::kDimensionsExceeded;
  if (webp) {
    auto* decoded = WebPDecodeRGBA(bytes, data.size(), &w, &h);
    if (!decoded) return UploadImageError::kInvalidImage;
    WebPFree(decoded);
  } else {
    auto* decoded = stbi_load_from_memory(bytes, static_cast<int>(data.size()),
                                          &w, &h, &channels, 4);
    if (!decoded) return UploadImageError::kInvalidImage;
    stbi_image_free(decoded);
  }
  return ImageInfo{png    ? "image/png"
                   : jpeg ? "image/jpeg"
                          : "image/webp",
                   png    ? "png"
                   : jpeg ? "jpg"
                          : "webp",
                   w, h};
}
}  // namespace RumpelQuiz
