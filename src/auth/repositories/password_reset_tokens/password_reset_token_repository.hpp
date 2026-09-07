#pragma once

#include <boost/uuid/uuid.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/io/chrono.hpp>
#include <userver/storages/postgres/transaction.hpp>

namespace RumpelQuiz {

struct ResetTokenData {
  boost::uuids::uuid id;
  boost::uuids::uuid user_id;
  std::string token_hash;
  userver::storages::postgres::TimePointTz expires_at;
  userver::storages::postgres::TimePointTz created_at;
};

class ResetTokenRepository {
 public:
  boost::uuids::uuid Create(
      userver::storages::postgres::Transaction& transaction,
      const boost::uuids::uuid& user_id, std::string_view token_hash,
      userver::storages::postgres::TimePointTz expires_at) const;

  std::optional<ResetTokenData> FindById(
      userver::storages::postgres::Transaction& transaction,
      const boost::uuids::uuid& token_id) const;

  void Delete(userver::storages::postgres::Transaction& transaction,
              const boost::uuids::uuid& token_id) const;
};

}  // namespace RumpelQuiz
