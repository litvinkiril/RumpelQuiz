#pragma once

#include <boost/uuid/uuid.hpp>
#include <optional>
#include <string_view>
#include <userver/storages/postgres/io/chrono.hpp>
#include <userver/storages/postgres/transaction.hpp>

#include "auth/models/verification_code/verification_code.hpp"

namespace RumpelQuiz {

class PasswordResetCodeRepository {
 public:
  boost::uuids::uuid UpsertCode(
      userver::storages::postgres::Transaction& transaction,
      const boost::uuids::uuid& user_id, std::string_view code_hash,
      userver::storages::postgres::TimePointTz expires_at) const;

  std::optional<CodeData> FindCode(
      userver::storages::postgres::Transaction& transaction,
      const boost::uuids::uuid& verification_id) const;

  void DeleteCode(userver::storages::postgres::Transaction& transaction,
                  const boost::uuids::uuid& verification_id) const;
};

}  // namespace RumpelQuiz
