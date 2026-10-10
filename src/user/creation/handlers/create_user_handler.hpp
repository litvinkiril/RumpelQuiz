#pragma once

#include <string_view>
#include <userver/server/handlers/http_handler_json_base.hpp>

#include "user/creation/service/user_creation_service.hpp"

namespace RumpelQuiz {

class CreateUserHandler final
    : public userver::server::handlers::HttpHandlerJsonBase {
 public:
  static constexpr std::string_view kName = "handler-create-user";

  CreateUserHandler(const userver::components::ComponentConfig& config,
                    const userver::components::ComponentContext& context);

  userver::formats::json::Value HandleRequestJsonThrow(
      const userver::server::http::HttpRequest& request,
      const userver::formats::json::Value& request_body,
      userver::server::request::RequestContext& context) const override;

 private:
  const UserCreationService& user_creation_service_;
};

}  // namespace RumpelQuiz