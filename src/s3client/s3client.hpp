#pragma once
#include <memory>
#include <userver/s3api/clients/s3api.hpp>
#include "s3client_base.hpp"
#include "s3client_config.hpp"

namespace RumpelQuiz {
class S3Client final : public S3ClientBase {
 public:
  // Disabled storage, useful when uploads are not configured.
  S3Client() = default;
  S3Client(std::shared_ptr<userver::s3api::Client> client, S3ClientConfig config);
  SaveImageResponse SaveImage(std::string_view contents, std::string_view key,
                             std::string_view content_type) const override;

 private:
  std::shared_ptr<userver::s3api::Client> client_;
  S3ClientConfig config_;
};
}  // namespace RumpelQuiz
