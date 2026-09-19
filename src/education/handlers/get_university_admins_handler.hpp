#pragma once

#include <string_view>

#include <userver/server/handlers/http_handler_json_base.hpp>

#include "education/service/education_service.hpp"

namespace RumpelQuiz {

class GetUniversityAdminsHandler final
    : public userver::server::handlers::HttpHandlerJsonBase {
public:
    static constexpr std::string_view kName =
        "handler-get-university-admins";

    GetUniversityAdminsHandler(
        const userver::components::ComponentConfig& config,
        const userver::components::ComponentContext& context);

private:
    userver::formats::json::Value HandleRequestJsonThrow(
        const userver::server::http::HttpRequest& request,
        const userver::formats::json::Value& request_body,
        userver::server::request::RequestContext& context
    ) const override;

    EducationService& education_service_;
};

}  // namespace RumpelQuiz