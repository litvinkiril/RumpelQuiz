#pragma once
#include <memory>
#include <userver/components/component_base.hpp>
#include <userver/yaml_config/schema.hpp>
#include "s3client.hpp"

namespace RumpelQuiz {
class S3ClientComponent final : public userver::components::ComponentBase {
 public:
  static constexpr std::string_view kName = "s3-client";
  S3ClientComponent(const userver::components::ComponentConfig& config,
                    const userver::components::ComponentContext& context);
  const S3ClientBase& GetClient() const noexcept;
  static userver::yaml_config::Schema GetStaticConfigSchema();

 private:
  std::unique_ptr<S3ClientBase> client_ = std::make_unique<S3Client>();
};
}  // namespace RumpelQuiz
