#pragma once

#include <boost/uuid/uuid.hpp>
#include <string>
#include <userver/storages/postgres/io/chrono.hpp>

namespace RumpelQuiz {

struct CodeData {
  boost::uuids::uuid id;
  boost::uuids::uuid user_id;
  std::string code_hash;
  userver::storages::postgres::TimePointTz expires_at;
  userver::storages::postgres::TimePointTz created_at;
};
}  // namespace RumpelQuiz
