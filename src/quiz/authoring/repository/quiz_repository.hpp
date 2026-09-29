#pragma once
#include <optional>
#include <userver/formats/json/value.hpp>
#include <userver/storages/postgres/transaction.hpp>
#include "quiz/quiz_models.hpp"
namespace RumpelQuiz {
struct StoredQuiz {
  boost::uuids::uuid university_id;
  std::int64_t revision;
};
class QuizRepository {
 public:
  std::optional<StoredQuiz> FindOwnedForUpdate(
      userver::storages::postgres::Transaction&, const boost::uuids::uuid&,
      const boost::uuids::uuid&) const;
  bool CheckImages(userver::storages::postgres::Transaction&,
                   const boost::uuids::uuid&,
                   const std::vector<boost::uuids::uuid>&) const;
  std::int64_t Save(userver::storages::postgres::Transaction&,
                    const boost::uuids::uuid&, const boost::uuids::uuid&,
                    const SaveQuizRequest&, bool create) const;
  void ReplaceQuestions(userver::storages::postgres::Transaction&,
                        const boost::uuids::uuid&,
                        const SaveQuizRequest&) const;
  userver::formats::json::Value Read(userver::storages::postgres::Transaction&,
                                     const boost::uuids::uuid&,
                                     const boost::uuids::uuid&) const;
  userver::formats::json::Value List(userver::storages::postgres::Transaction&,
                                     const boost::uuids::uuid&) const;
};
}  // namespace RumpelQuiz
