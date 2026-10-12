#include "education_search_service.hpp"

#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/storages/postgres/options.hpp>

namespace RumpelQuiz {
namespace {

std::string SearchPattern(const std::string& name) {
  std::string pattern = "%";
  for (const char ch : name) {
    if (ch == '!' || ch == '%' || ch == '_') {
      pattern += '!';
    }
    pattern += ch;
  }
  pattern += '%';
  return pattern;
}

}  // namespace

EducationSearchService::EducationSearchService(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      pg_(context.FindComponent<userver::components::Postgres>("postgres-db-1")
              .GetCluster()) {}

std::vector<EducationSearchItem> EducationSearchService::SearchPrograms(
    const boost::uuids::uuid& university_id, const std::string& name) const {
  auto transaction =
      pg_->Begin(userver::storages::postgres::ClusterHostType::kMaster,
                 userver::storages::postgres::TransactionOptions{});
  auto items = repository_.SearchPrograms(transaction, university_id,
                                          SearchPattern(name));
  transaction.Commit();
  return items;
}

std::vector<EducationSearchItem> EducationSearchService::SearchGroups(
    const boost::uuids::uuid& university_id, const std::string& name) const {
  auto transaction =
      pg_->Begin(userver::storages::postgres::ClusterHostType::kMaster,
                 userver::storages::postgres::TransactionOptions{});
  auto items =
      repository_.SearchGroups(transaction, university_id, SearchPattern(name));
  transaction.Commit();
  return items;
}

}  // namespace RumpelQuiz
