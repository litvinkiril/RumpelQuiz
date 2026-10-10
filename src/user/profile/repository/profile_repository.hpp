#pragma once

#include <boost/uuid/uuid.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <userver/storages/postgres/io/uuid.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/io/chrono.hpp>
#include <userver/storages/postgres/transaction.hpp>
#include "user/profile/profile_models.hpp"

namespace RumpelQuiz {

class ProfileRepository {
 public:
  void Create(
      userver::storages::postgres::Transaction& transaction,
      const boost::uuids::uuid& user_id,
      std::string_view first_name,
      std::string_view last_name,
      std::optional<std::string_view> middle_name,
      std::optional<std::string_view> avatar_url,
      std::optional<std::string_view> description) const;

  std::optional<UserWithProfile> FindUserWithProfileById(
      userver::storages::postgres::Transaction& transaction,
      const boost::uuids::uuid& user_id) const;

  void Update(
    userver::storages::postgres::Transaction& transaction,
    const boost::uuids::uuid& user_id,
    std::string_view first_name,
    std::string_view last_name,
    std::optional<std::string_view> middle_name,
    std::optional<std::string_view> avatar_url,
    std::optional<std::string_view> description) const;
  
  void Delete(userver::storages::postgres::Transaction& transaction,
              const boost::uuids::uuid& user_id) const;
};

}