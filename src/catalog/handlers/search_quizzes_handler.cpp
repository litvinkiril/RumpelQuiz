#include "search_quizzes_handler.hpp"

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

SearchQuizHandler::SearchQuizHandler(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : HttpHandlerJsonBase(config, context),
      catalog_service_(
          context.FindComponent<CatalogService>()) {}

userver::formats::json::Value
SearchQuizHandler::HandleRequestJsonThrow(
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

    const auto quizzes = catalog_service_.SearchQuizzes(
        GetAuthenticatedUserId(context), favourites == "true",
        request.GetArg("name"), count_spend);

    userver::formats::json::ValueBuilder response;
    response["quizzes"] = userver::formats::json::MakeArray();
    for (const auto& quiz : quizzes.quizzes_info) {
        response["quizzes"].PushBack(userver::formats::json::MakeObject(
            "quiz_id", boost::uuids::to_string(quiz.quiz_id),
            "creator_first_name", quiz.creator_first_name,
            "creator_last_name", quiz.creator_last_name,
            "title", quiz.title,
            "question_count", quiz.question_count,
            "is_favourite", quiz.is_favourite));
    }
    return response.ExtractValue();
}

}  // namespace RumpelQuiz
