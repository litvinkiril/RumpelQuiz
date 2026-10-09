#include "remove_quiz_favourite_handler.hpp"

#include <stdexcept>
#include <boost/uuid/string_generator.hpp>
#include <userver/components/component_context.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/server/http/http_status.hpp>

#include "auth/middleware/jwt/auth_context.hpp"

namespace RumpelQuiz {

RemoveQuizFavouriteHandler::RemoveQuizFavouriteHandler(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : HttpHandlerJsonBase(config, context),
      catalog_service_(context.FindComponent<CatalogService>()) {}

userver::formats::json::Value RemoveQuizFavouriteHandler::HandleRequestJsonThrow(
    const userver::server::http::HttpRequest& request,
    const userver::formats::json::Value&,
    userver::server::request::RequestContext& context) const {
    using userver::server::http::HttpStatus;
    request.GetHttpResponse().SetHeader(
        std::string_view{"Cache-Control"}, "no-store");

    boost::uuids::uuid quiz_id;
    try {
        quiz_id = boost::uuids::string_generator{}(request.GetPathArg("quiz_id"));
        if (quiz_id.is_nil()) throw std::runtime_error("nil quiz id");
    } catch (const std::runtime_error&) {
        request.SetResponseStatus(HttpStatus::kBadRequest);
        return userver::formats::json::MakeObject(
            "success", false, "error", "invalid_quiz_id");
    }

    catalog_service_.RemoveQuizFromFavourites(GetAuthenticatedUserId(context), quiz_id);
    return userver::formats::json::MakeObject("success", true);
}

}  // namespace RumpelQuiz
