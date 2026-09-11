#pragma once

#include <boost/uuid/uuid.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/io/chrono.hpp>
#include <userver/storages/postgres/transaction.hpp>

namespace RumpelQuiz {

struct SessionData {
  boost::uuids::uuid id;
  boost::uuids::uuid user_id;

  std::string refresh_token_hash;

  userver::storages::postgres::TimePointTz expires_at;
};

class SessionRepository {
 public:
  boost::uuids::uuid Create(
      userver::storages::postgres::Transaction& transaction,
      const boost::uuids::uuid& user_id, std::string_view refresh_token_hash,
      userver::storages::postgres::TimePointTz expires_at) const;

  // Locks the row until the caller commits or rolls back the transaction.
  std::optional<SessionData> FindById(
      userver::storages::postgres::Transaction& transaction,
      const boost::uuids::uuid& session_id) const;

  void UpdateRefreshToken(userver::storages::postgres::Transaction& transaction,
                          const boost::uuids::uuid& session_id,
                          std::string_view refresh_token_hash) const;

  void Delete(userver::storages::postgres::Transaction& transaction,
              const boost::uuids::uuid& session_id) const;

  void DeleteAllByUserId(userver::storages::postgres::Transaction& transaction,
                         const boost::uuids::uuid& user_id) const;
};

}  // namespace RumpelQuiz
