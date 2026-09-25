#pragma once
#include <string>

namespace RumpelQuiz {
// Credentials belong to the SDK authenticator, not the image client.
struct S3ClientConfig {
  std::string endpoint;
  std::string bucket_name;
};
}  // namespace RumpelQuiz
