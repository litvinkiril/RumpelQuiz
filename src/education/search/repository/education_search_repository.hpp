#pragma once

#include <string>
#include <vector>

#include <userver/storages/postgres/transaction.hpp>

#include "education/search/search_models.hpp"

namespace RumpelQuiz {

class EducationSearchRepository {
 public:
  std::vector<EducationSearchItem> SearchPrograms(
      userver::storages::postgres::Transaction& transaction,
      const boost::uuids::uuid& university_id,
      const std::string& pattern) const;

  std::vector<EducationSearchItem> SearchGroups(
      userver::storages::postgres::Transaction& transaction,
      const boost::uuids::uuid& university_id,
      const std::string& pattern) const;
};

}  // namespace RumpelQuiz
