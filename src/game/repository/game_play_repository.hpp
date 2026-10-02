#pragma once
#include <boost/uuid/uuid.hpp>
#include <stdexcept>
#include <string>
#include <userver/formats/json/value.hpp>
#include <userver/storages/postgres/transaction.hpp>
#include <vector>
namespace RumpelQuiz {
struct GamePlayError : std::runtime_error {
  using std::runtime_error::runtime_error;
};
class GamePlayRepository {
 public:
  userver::formats::json::Value Read(userver::storages::postgres::Transaction&,
                                     const boost::uuids::uuid& user,
                                     const boost::uuids::uuid& session) const;
  boost::uuids::uuid Join(userver::storages::postgres::Transaction&,
                          const boost::uuids::uuid& user,
                          const std::string& code) const;
  void Submit(userver::storages::postgres::Transaction&,
              const boost::uuids::uuid& user, const boost::uuids::uuid& session,
              const boost::uuids::uuid& question,
              const std::vector<boost::uuids::uuid>& choices) const;
};
}  // namespace RumpelQuiz
