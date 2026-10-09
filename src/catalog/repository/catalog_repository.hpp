#pragma once

#include <userver/storages/postgres/transaction.hpp>

#include "catalog/search_models.hpp"

namespace RumpelQuiz {

class CatalogRepository {
public:
    SearchTestsResult SearchTests(
        userver::storages::postgres::Transaction& transaction,
        const boost::uuids::uuid& user_id,
        bool is_favourite,
        const std::string& name,
        int count_spend) const;

    bool AddToFavourites(
        userver::storages::postgres::Transaction& transaction,
        const boost::uuids::uuid& user_id,
        const boost::uuids::uuid& test_id) const;

    void RemoveFromFavourites(
        userver::storages::postgres::Transaction& transaction,
        const boost::uuids::uuid& user_id,
        const boost::uuids::uuid& test_id) const;

    SearchQuizzesResult SearchQuizzes(
        userver::storages::postgres::Transaction& transaction,
        const boost::uuids::uuid& user_id,
        bool is_favourite,
        const std::string& name,
        int count_spend) const;

    bool AddQuizToFavourites(
        userver::storages::postgres::Transaction& transaction,
        const boost::uuids::uuid& user_id,
        const boost::uuids::uuid& quiz_id) const;

    void RemoveQuizFromFavourites(
        userver::storages::postgres::Transaction& transaction,
        const boost::uuids::uuid& user_id,
        const boost::uuids::uuid& quiz_id) const;
};
}  // namespace RumpelQuiz
