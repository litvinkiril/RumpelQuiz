#include "add_test_favourite_handler.hpp"

#include <stdexcept>
#include <boost/uuid/string_generator.hpp>
#include <userver/components/component_context.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/server/http/http_status.hpp>

#include "auth/middleware/jwt/auth_context.hpp"

namespace RumpelQuiz {

AddTestFavouriteHandler::AddTestFavouriteHandler(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : HttpHandlerJsonBase(config, context),
      catalog_service_(context.FindComponent<CatalogService>()) {}

userver::formats::json::Value AddTestFavouriteHandler::HandleRequestJsonThrow(
    const userver::server::http::HttpRequest& request,
    const userver::formats::json::Value&,
    userver::server::request::RequestContext& context) const {
    using userver::server::http::HttpStatus;
    request.GetHttpResponse().SetHeader(
        std::string_view{"Cache-Control"}, "no-store");

    boost::uuids::uuid test_id;
    try {
        test_id = boost::uuids::string_generator{}(request.GetPathArg("test_id"));
        if (test_id.is_nil()) throw std::runtime_error("nil test id");
    } catch (const std::runtime_error&) {
        request.SetResponseStatus(HttpStatus::kBadRequest);
        return userver::formats::json::MakeObject(
            "success", false, "error", "invalid_test_id");
    }

    if (!catalog_service_.AddToFavourites(GetAuthenticatedUserId(context), test_id)) {
        request.SetResponseStatus(HttpStatus::kNotFound);
        return userver::formats::json::MakeObject(
            "success", false, "error", "test_not_found");
    }
    return userver::formats::json::MakeObject("success", true);
}

}  // namespace RumpelQuiz
