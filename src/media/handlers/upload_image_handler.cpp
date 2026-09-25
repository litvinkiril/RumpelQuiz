#include "upload_image_handler.hpp"

#include <cstddef>
#include <variant>

#include <boost/uuid/uuid_io.hpp>

#include <userver/components/component_context.hpp>
#include <userver/formats/json/serialize.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/server/http/http_response.hpp>
#include <userver/server/http/http_status.hpp>

#include "auth/middleware/jwt/auth_context.hpp"
#include "media/media_models.hpp"

namespace RumpelQuiz {
namespace {

using HttpStatus = userver::server::http::HttpStatus;

constexpr std::size_t kMaxImageBytes = 5 * 1024 * 1024;

std::string ErrorResponse(
    const userver::server::http::HttpRequest& request,
    HttpStatus status,
    std::string_view error
) {
    request.SetResponseStatus(status);

    userver::formats::json::ValueBuilder body;
    body["success"] = false;
    body["error"] = std::string{error};

    return userver::formats::json::ToString(body.ExtractValue());
}

}  // namespace

UploadImageHandler::UploadImageHandler(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context
)
    : HttpHandlerBase(config, context),
      media_service_(context.FindComponent<MediaService>()) {}

std::string UploadImageHandler::HandleRequestThrow(
    const userver::server::http::HttpRequest& request,
    userver::server::request::RequestContext& context
) const {
    auto& response = request.GetHttpResponse();
    response.SetContentType("application/json; charset=utf-8");
    response.SetHeader(std::string_view{"Cache-Control"}, "no-store");

    const auto user_id = GetAuthenticatedUserId(context);

    // Ожидаем ровно один файл в поле file.
    const auto& files = request.GetFormDataArgVector("file");

    if (files.size() != 1 || files.front().value.empty()) {
        return ErrorResponse(
            request, HttpStatus::kBadRequest, "expected_one_image"
        );
    }

    const auto& file = files.front();

    if (file.value.size() > kMaxImageBytes) {
        return ErrorResponse(
            request, HttpStatus::kPayloadTooLarge, "image_too_large"
        );
    }

    const auto result = media_service_.UploadImage(user_id, file.value);

    if (const auto* error = std::get_if<UploadImageError>(&result)) {
        switch (*error) {
            case UploadImageError::kAccessDenied:
                return ErrorResponse(
                    request, HttpStatus::kForbidden, "upload_access_denied"
                );

            case UploadImageError::kInvalidImage:
                return ErrorResponse(
                    request, HttpStatus::kBadRequest, "invalid_image"
                );

            case UploadImageError::kUnsupportedFormat:
                return ErrorResponse(
                    request,
                    HttpStatus::kUnsupportedMediaType,
                    "unsupported_image_format"
                );

            case UploadImageError::kDimensionsExceeded:
                return ErrorResponse(
                    request,
                    HttpStatus::kBadRequest,
                    "image_dimensions_exceeded"
                );

            case UploadImageError::kStorageUnavailable:
                return ErrorResponse(
                    request,
                    HttpStatus::kServiceUnavailable,
                    "image_storage_unavailable"
                );
        }
    }

    const auto& uploaded = std::get<UploadedImage>(result);

    userver::formats::json::ValueBuilder body;
    body["success"] = true;
    body["media_id"] = boost::uuids::to_string(uploaded.id);

    request.SetResponseStatus(HttpStatus::kCreated);

    return userver::formats::json::ToString(body.ExtractValue());
}

}  // namespace RumpelQuiz