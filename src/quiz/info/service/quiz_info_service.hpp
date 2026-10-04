#pragma once

#include <optional>
#include <string_view>
#include <variant>

#include <boost/uuid/uuid.hpp>

#include <userver/components/component_base.hpp>
#include <userver/formats/json/value.hpp>
#include <userver/storages/postgres/cluster.hpp>

#include "quiz/info/repository/quiz_info_repository.hpp"

namespace RumpelQuiz {

enum class SessionResultsError {
  kNotFound,
  kNotFinished
};

using SessionResultsResult =
    std::variant<userver::formats::json::Value,
                 SessionResultsError>;

class QuizInfoService final
    : public userver::components::ComponentBase {
 public:
  static constexpr std::string_view kName = "quiz-info-service";

  QuizInfoService(
      const userver::components::ComponentConfig&,
      const userver::components::ComponentContext&);

  std::optional<userver::formats::json::Value> ListQuizSessions(
      const boost::uuids::uuid& user_id,
      const boost::uuids::uuid& quiz_id) const;

  SessionResultsResult GetSessionResults(
    const boost::uuids::uuid& user_id,
    const boost::uuids::uuid& session_id) const;

 private:
  userver::storages::postgres::ClusterPtr pg_;
  QuizInfoRepository repository_;
};

}  // namespace RumpelQuiz