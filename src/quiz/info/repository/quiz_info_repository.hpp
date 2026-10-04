#pragma once

#include <boost/uuid/uuid.hpp>
#include <optional>
#include <string>

#include <userver/formats/json/value.hpp>
#include <userver/storages/postgres/transaction.hpp>

namespace RumpelQuiz {

struct SessionResultsAccess {
  std::string status;
  bool can_view;
};    

class QuizInfoRepository {
 public:
  bool CanViewQuiz(
      userver::storages::postgres::Transaction&,
      const boost::uuids::uuid& user_id,
      const boost::uuids::uuid& quiz_id) const;

  userver::formats::json::Value ListQuizSessions(
      userver::storages::postgres::Transaction&,
      const boost::uuids::uuid& quiz_id) const;

  std::optional<SessionResultsAccess> GetSessionResultsAccess(
    userver::storages::postgres::Transaction&,
    const boost::uuids::uuid& user_id,
    const boost::uuids::uuid& session_id) const;

  userver::formats::json::Value GetSessionResults(
    userver::storages::postgres::Transaction&,
    const boost::uuids::uuid& session_id) const;
};

}  // namespace RumpelQuiz
