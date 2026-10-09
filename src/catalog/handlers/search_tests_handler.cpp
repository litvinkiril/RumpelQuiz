#include "search_tests_handler.hpp"

#include <algorithm>
#include <charconv>
#include <string>
#include <string_view>
#include <system_error>

#include <boost/uuid/uuid_io.hpp>

#include <userver/components/component_context.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/server/http/http_status.hpp>

#include "auth/middleware/jwt/auth_context.hpp"

namespace RumpelQuiz {

SearchTestHandler::SearchTestHandler(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : HttpHandlerJsonBase(config, context),
      catalog_service_(
          context.FindComponent<CatalogService>()) {}

userver::formats::json::Value
SearchTestHandler::HandleRequestJsonThrow(
    const userver::server::http::HttpRequest& request,
    const userver::formats::json::Value&,
    userver::server::request::RequestContext& context) const {
    request.GetHttpResponse().SetHeader(
        std::string_view{"Cache-Control"}, "no-store");

    const auto bad_request = [&request] {
        request.SetResponseStatus(
            userver::server::http::HttpStatus::kBadRequest);
        return userver::formats::json::MakeObject(
            "success", false, "error", "invalid_search_params");
    };

    if (!request.HasArg("favourites") || !request.HasArg("name") ||
        !request.HasArg("count_spend")) {
        return bad_request();
    }

    const auto& favourites = request.GetArg("favourites");
    if (favourites != "true" && favourites != "false") {
        return bad_request();
    }

    const auto& offset = request.GetArg("count_spend");
    if (offset.empty() || !std::all_of(offset.begin(), offset.end(),
            [](char ch) { return ch >= '0' && ch <= '9'; })) {
        return bad_request();
    }
    int count_spend = 0;
    const auto parsed = std::from_chars(
        offset.data(), offset.data() + offset.size(), count_spend);
    if (parsed.ec != std::errc{} ||
        parsed.ptr != offset.data() + offset.size()) {
        return bad_request();
    }

    const auto tests = catalog_service_.SearchTests(
        GetAuthenticatedUserId(context), favourites == "true",
        request.GetArg("name"), count_spend);

    userver::formats::json::ValueBuilder response;
    response["tests"] = userver::formats::json::MakeArray();
    for (const auto& test : tests.tests_info) {
        response["tests"].PushBack(userver::formats::json::MakeObject(
            "test_id", boost::uuids::to_string(test.test_id),
            "creator_first_name", test.creator_first_name,
            "creator_last_name", test.creator_last_name,
            "title", test.title,
            "question_count", test.question_count,
            "is_favourite", test.is_favourite));
    }
    return response.ExtractValue();
}

}  // namespace RumpelQuiz
