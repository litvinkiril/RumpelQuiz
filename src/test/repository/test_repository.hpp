#pragma once
#include <optional>
#include <userver/formats/json/value.hpp>
#include <userver/storages/postgres/transaction.hpp>
#include "test/test_models.hpp"
namespace RumpelQuiz {
struct StoredTest {
  boost::uuids::uuid university_id;
  std::int64_t revision;
  std::string status;
};
class TestRepository {
 public:
  std::optional<StoredTest> FindOwnedForUpdate(
      userver::storages::postgres::Transaction&, const boost::uuids::uuid&,
      const boost::uuids::uuid&) const;
  bool CheckImages(userver::storages::postgres::Transaction&,
                   const boost::uuids::uuid&,
                   const std::vector<boost::uuids::uuid>&) const;
  std::int64_t Save(userver::storages::postgres::Transaction&,
                    const boost::uuids::uuid&, const boost::uuids::uuid&,
                    const SaveTestRequest&, bool create) const;
  void ReplaceQuestions(userver::storages::postgres::Transaction&,
                        const boost::uuids::uuid&,
                        const SaveTestRequest&) const;
  userver::formats::json::Value Read(userver::storages::postgres::Transaction&,
                                     const boost::uuids::uuid&,
                                     const boost::uuids::uuid&) const;
  userver::formats::json::Value List(userver::storages::postgres::Transaction&,
                                     const boost::uuids::uuid&) const;
};
}  // namespace RumpelQuiz
