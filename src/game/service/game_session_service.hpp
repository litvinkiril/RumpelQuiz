#pragma once

#include <optional>
#include <string_view>
#include <userver/formats/json/value.hpp>
#include <vector>

#include <boost/uuid/uuid.hpp>

#include <userver/components/component_base.hpp>
#include <userver/storages/postgres/cluster.hpp>

#include "game/game_models.hpp"
#include "game/service/game_events.hpp"
#include "game/repository/game_play_repository.hpp"
#include "game/repository/game_session_repository.hpp"
#include "s3client/s3client_base.hpp"

namespace RumpelQuiz {

class GameSessionService final : public userver::components::ComponentBase {
 public:
  static constexpr std::string_view kName = "game-session-service";

  GameSessionService(const userver::components::ComponentConfig& config,
                     const userver::components::ComponentContext& context);

  GameSessionResult Create(const boost::uuids::uuid& user_id,
                           const boost::uuids::uuid& quiz_id,
                           const boost::uuids::uuid& session_id,
                           const std::string& name) const;

  NextGameQuestionServiceResult NextQuestion(
      const boost::uuids::uuid& user_id, const boost::uuids::uuid& session_id,
      const std::optional<boost::uuids::uuid>& expected_question_id) const;

  GameSessionResult Close(const boost::uuids::uuid& user_id,
                          const boost::uuids::uuid& session_id) const;

 private:
  userver::storages::postgres::ClusterPtr pg_;
  GameSessionRepository repository_;
  GamePlayRepository play_repository_;
  const S3ClientBase& s3_;
  GameEvents& events_;

 public:
  userver::formats::json::Value Read(const boost::uuids::uuid& user,
                                     const boost::uuids::uuid& session) const;
  boost::uuids::uuid Join(const boost::uuids::uuid& user,
                          const std::string& code) const;
  void Submit(const boost::uuids::uuid& user, const boost::uuids::uuid& session,
              const boost::uuids::uuid& question,
              const std::vector<boost::uuids::uuid>& choices) const;
};

}  // namespace RumpelQuiz
