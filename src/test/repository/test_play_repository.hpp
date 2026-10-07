#pragma once
#include <boost/uuid/uuid.hpp>
#include <stdexcept>
#include <string>
#include <userver/formats/json/value.hpp>
#include <userver/storages/postgres/transaction.hpp>
#include <vector>
namespace RumpelQuiz {
struct TestPlayError : std::runtime_error {
  using std::runtime_error::runtime_error;
};
class TestPlayRepository {
 public:
  userver::formats::json::Value Available(
      userver::storages::postgres::Transaction&,
      const boost::uuids::uuid&) const;
  void Start(userver::storages::postgres::Transaction&,
             const boost::uuids::uuid&, const boost::uuids::uuid&) const;
  userver::formats::json::Value Read(userver::storages::postgres::Transaction&,
                                     const boost::uuids::uuid&,
                                     const boost::uuids::uuid&) const;
  bool Submit(userver::storages::postgres::Transaction&,
              const boost::uuids::uuid&, const boost::uuids::uuid&,
              const boost::uuids::uuid&, std::vector<boost::uuids::uuid>) const;
  userver::formats::json::Value Results(
      userver::storages::postgres::Transaction&, const boost::uuids::uuid&,
      const boost::uuids::uuid&) const;
};
}  // namespace RumpelQuiz
