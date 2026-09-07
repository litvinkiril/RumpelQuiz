#pragma once

#include <string>
#include <string_view>

namespace RumpelQuiz {

struct AwsSignedHeaders {
  std::string authorization;
  std::string amz_date;
  std::string content_sha256;
};

class AwsSignatureV4 {
 public:
  AwsSignatureV4(std::string key_id, std::string secret_key);

  AwsSignedHeaders SignPostboxRequest(std::string_view payload) const;

 private:
  std::string key_id_;
  std::string secret_key_;
};

}  // namespace RumpelQuiz
