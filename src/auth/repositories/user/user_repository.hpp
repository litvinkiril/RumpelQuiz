#pragma once

#include <optional>
#include <string_view>

#include <boost/uuid/uuid.hpp>

#include <userver/storages/postgres/transaction.hpp>

#include "auth/models/user/user.hpp"

namespace RumpelQuiz {

class UserRepository {
 public:
  std::optional<UserData> FindByEmail(
      userver::storages::postgres::Transaction& transaction,
      std::string_view email) const;

  std::optional<UserData> FindById(
      userver::storages::postgres::Transaction& transaction,
      const boost::uuids::uuid& user_id) const;

  boost::uuids::uuid CreateUser(
      userver::storages::postgres::Transaction& transaction,
      std::string_view email, std::string_view password_hash) const;

  void UpdateUnverifiedUser(
      userver::storages::postgres::Transaction& transaction,
      const boost::uuids::uuid& user_id, std::string_view password_hash) const;

  void VerifyUser(userver::storages::postgres::Transaction& transaction,
                  const boost::uuids::uuid& user_id) const;
};

}  // namespace RumpelQuiz
