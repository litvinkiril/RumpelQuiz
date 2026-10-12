#pragma once

#include <string>
#include <string_view>
#include <vector>

#include <userver/components/component_base.hpp>
#include <userver/storages/postgres/cluster.hpp>

#include "education/search/repository/education_search_repository.hpp"

namespace RumpelQuiz {

class EducationSearchService final : public userver::components::ComponentBase {
 public:
  static constexpr std::string_view kName = "education-search-service";

  EducationSearchService(const userver::components::ComponentConfig& config,
                         const userver::components::ComponentContext& context);

  std::vector<EducationSearchItem> SearchPrograms(
      const boost::uuids::uuid& university_id, const std::string& name) const;

  std::vector<EducationSearchItem> SearchGroups(
      const boost::uuids::uuid& university_id, const std::string& name) const;

 private:
  userver::storages::postgres::ClusterPtr pg_;
  EducationSearchRepository repository_;
};

}  // namespace RumpelQuiz
