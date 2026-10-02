#include "image_validation.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_ONLY_GIF
#define STBI_NO_STDIO
#include <stb/stb_image.h>

#include <webp/decode.h>

namespace RumpelQuiz {
namespace {

constexpr std::size_t kMaxImageBytes = 5 * 1024 * 1024;
constexpr int kMaxDimension = 4096;
constexpr std::int64_t kMaxPixels = 16'000'000;

bool ValidDimensions(int width, int height) {
    return width > 0 && height > 0 &&
           width <= kMaxDimension &&
           height <= kMaxDimension &&
           static_cast<std::int64_t>(width) * height <= kMaxPixels;
}

enum class Format {
    kJpeg,
    kPng,
    kGif,
    kWebp,
    kUnknown,
};

Format DetectFormat(std::string_view data) {
    if (data.starts_with(std::string_view{"__codex_directive_quoted_backslash__xff__codex_directive_quoted_backslash__xd8__codex_directive_quoted_backslash__xff", 3})) {
        return Format::kJpeg;
    }

    if (data.starts_with(std::string_view{"__codex_directive_quoted_backslash__x89PNG__codex_directive_quoted_backslash__r__codex_directive_quoted_backslash__n__codex_directive_quoted_backslash__x1a__codex_directive_quoted_backslash__n", 8})) {
        return Format::kPng;
    }

    if (data.starts_with("GIF87a") || data.starts_with("GIF89a")) {
        return Format::kGif;
    }

    if (data.size() >= 12 &&
        data.substr(0, 4) == "RIFF" &&
        data.substr(8, 4) == "WEBP") {
        return Format::kWebp;
    }

    return Format::kUnknown;
}

}  // namespace

ImageValidationResult ValidateImage(std::string_view contents) {
    if (contents.empty() || contents.size() > kMaxImageBytes) {
        // HTTP handler already returns 413 for oversized files.
        // The current error enum has no separate size error.
        return UploadImageError::kInvalidImage;
    }

    const auto format = DetectFormat(contents);
    if (format == Format::kUnknown) {
        return UploadImageError::kUnsupportedFormat;
    }

    const auto* bytes =
        reinterpret_cast<const unsigned char*>(contents.data());

    int width = 0;
    int height = 0;

    if (format == Format::kWebp) {
        WebPBitstreamFeatures features{};

        if (WebPGetFeatures(bytes, contents.size(), &features) !=
            VP8_STATUS_OK) {
            return UploadImageError::kInvalidImage;
        }

        // Animated WebP needs a separate frame-by-frame decoder.
        if (features.has_animation) {
            return UploadImageError::kUnsupportedFormat;
        }

        if (!ValidDimensions(features.width, features.height)) {
            return UploadImageError::kDimensionsExceeded;
        }

        std::unique_ptr<std::uint8_t, decltype(&WebPFree)> decoded(
            WebPDecodeRGBA(bytes, contents.size(), &width, &height),
            &WebPFree
        );

        if (!decoded ||
            width != features.width ||
            height != features.height) {
            return UploadImageError::kInvalidImage;
        }

        return ImageInfo{"image/webp", "webp", width, height};
    }

    // Safe conversion: contents is limited to 5 MiB above.
    const auto size = static_cast<int>(contents.size());
    int channels = 0;

    if (!stbi_info_from_memory(
            bytes, size, &width, &height, &channels)) {
        return UploadImageError::kInvalidImage;
    }

    if (!ValidDimensions(width, height)) {
        return UploadImageError::kDimensionsExceeded;
    }

    const int expected_width = width;
    const int expected_height = height;

    std::unique_ptr<stbi_uc, decltype(&stbi_image_free)> decoded(
        stbi_load_from_memory(
            bytes, size, &width, &height, &channels, 4
        ),
        &stbi_image_free
    );

    if (!decoded ||
        width != expected_width ||
        height != expected_height) {
        return UploadImageError::kInvalidImage;
    }

    switch (format) {
        case Format::kJpeg:
            return ImageInfo{"image/jpeg", "jpg", width, height};

        case Format::kPng:
            return ImageInfo{"image/png", "png", width, height};

        case Format::kGif:
            return ImageInfo{"image/gif", "gif", width, height};

        default:
            return UploadImageError::kUnsupportedFormat;
    }
}

}  // namespace RumpelQuiz
