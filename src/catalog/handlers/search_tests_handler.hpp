#pragma once

#include <string_view>

#include <userver/server/handlers/http_handler_json_base.hpp>

#include "catalog/search_models.hpp"
#include "catalog/service/catalog_service.hpp"

namespace RumpelQuiz {

class SearchTestHandler final
    : public userver::server::handlers::HttpHandlerJsonBase {
public:
    static constexpr std::string_view kName =
        "handler-search-tests";

    SearchTestHandler(
        const userver::components::ComponentConfig& config,
        const userver::components::ComponentContext& context);

private:
    userver::formats::json::Value HandleRequestJsonThrow(
        const userver::server::http::HttpRequest& request,
        const userver::formats::json::Value& request_body,
        userver::server::request::RequestContext& context
    ) const override;

    CatalogService& catalog_service_;
};

}  // namespace RumpelQuiz