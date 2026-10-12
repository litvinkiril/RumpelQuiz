#include "search_groups_handler.hpp"

#include <userver/components/component_context.hpp>

#include "search_handler_utils.hpp"

namespace RumpelQuiz {

SearchGroupsHandler::SearchGroupsHandler(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : HttpHandlerJsonBase(config, context),
      search_service_(context.FindComponent<EducationSearchService>()) {}

userver::formats::json::Value SearchGroupsHandler::HandleRequestJsonThrow(
    const userver::server::http::HttpRequest& request,
    const userver::formats::json::Value&,
    userver::server::request::RequestContext&) const {
  return education_search::Search(request, search_service_, false);
}

}  // namespace RumpelQuiz
