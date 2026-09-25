#pragma once

#include <cstdint>
#include <string>
#include <variant>

#include <boost/uuid/uuid.hpp>

namespace RumpelQuiz {

struct UploadedImage {
    boost::uuids::uuid id;
};

enum class UploadImageError {
    kAccessDenied,
    kInvalidImage,
    kUnsupportedFormat,
    kDimensionsExceeded,
    kStorageUnavailable,
};

using UploadImageResult = std::variant<UploadedImage, UploadImageError>;

struct MediaRecord {
    boost::uuids::uuid id;
    boost::uuids::uuid owner_id;
    std::string storage_key;
    std::string content_type;
    std::int64_t size_bytes;
    std::int32_t width;
    std::int32_t height;
};
}  // namespace RumpelQuiz
