#pragma once

#include <optional>

#include <boost/uuid/uuid.hpp>
#include <userver/storages/postgres/transaction.hpp>

#include "game/game_models.hpp"

namespace RumpelQuiz {

class GameSessionRepository {
 public:
    GameSessionResult Create(
        userver::storages::postgres::Transaction& tx,
        const boost::uuids::uuid& user_id,
        const boost::uuids::uuid& quiz_id,
        const boost::uuids::uuid& session_id
    ) const;

    NextGameQuestionResult NextQuestion(
        userver::storages::postgres::Transaction& tx,
        const boost::uuids::uuid& user_id,
        const boost::uuids::uuid& session_id,
        const std::optional<boost::uuids::uuid>& expected_question_id
    ) const;

    GameSessionResult Close(
        userver::storages::postgres::Transaction& tx,
        const boost::uuids::uuid& user_id,
        const boost::uuids::uuid& session_id
    ) const;

 private:
    std::optional<GameSession> LockOwnedSession(
        userver::storages::postgres::Transaction& tx,
        const boost::uuids::uuid& user_id,
        const boost::uuids::uuid& session_id
    ) const;
};

}  // namespace RumpelQuiz