#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <variant>

#include "media/media_models.hpp"

namespace RumpelQuiz {

struct ImageInfo {
    std::string content_type;
    std::string extension;
    std::int32_t width;
    std::int32_t height;
};

using ImageValidationResult =
    std::variant<ImageInfo, UploadImageError>;

ImageValidationResult ValidateImage(std::string_view contents);

}  // namespace RumpelQuiz