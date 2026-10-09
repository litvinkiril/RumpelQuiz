#include "catalog_service.hpp"

#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/storages/postgres/options.hpp>

namespace RumpelQuiz {

CatalogService::CatalogService(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      pg_(context.FindComponent<userver::components::Postgres>(
              "postgres-db-1")
              .GetCluster()) {}

SearchTestsResult CatalogService::SearchTests(
    const boost::uuids::uuid& user_id,
    bool is_favourite,
    const std::string& name,
    int count_spend) const {
    auto transaction = pg_->Begin(
        userver::storages::postgres::ClusterHostType::kMaster,
        userver::storages::postgres::TransactionOptions{});

    auto result = catalog_repository_.SearchTests(
        transaction, user_id, is_favourite, name, count_spend);
    transaction.Commit();
    return result;
}

bool CatalogService::AddToFavourites(
    const boost::uuids::uuid& user_id,
    const boost::uuids::uuid& test_id) const {
    auto transaction = pg_->Begin(
        userver::storages::postgres::ClusterHostType::kMaster,
        userver::storages::postgres::TransactionOptions{});
    const bool added = catalog_repository_.AddToFavourites(
        transaction, user_id, test_id);
    transaction.Commit();
    return added;
}

void CatalogService::RemoveFromFavourites(
    const boost::uuids::uuid& user_id,
    const boost::uuids::uuid& test_id) const {
    auto transaction = pg_->Begin(
        userver::storages::postgres::ClusterHostType::kMaster,
        userver::storages::postgres::TransactionOptions{});
    catalog_repository_.RemoveFromFavourites(transaction, user_id, test_id);
    transaction.Commit();
}

SearchQuizzesResult CatalogService::SearchQuizzes(
    const boost::uuids::uuid& user_id,
    bool is_favourite,
    const std::string& name,
    int count_spend) const {
    auto transaction = pg_->Begin(
        userver::storages::postgres::ClusterHostType::kMaster,
        userver::storages::postgres::TransactionOptions{});

    auto result = catalog_repository_.SearchQuizzes(
        transaction, user_id, is_favourite, name, count_spend);
    transaction.Commit();
    return result;
}

bool CatalogService::AddQuizToFavourites(
    const boost::uuids::uuid& user_id,
    const boost::uuids::uuid& quiz_id) const {
    auto transaction = pg_->Begin(
        userver::storages::postgres::ClusterHostType::kMaster,
        userver::storages::postgres::TransactionOptions{});
    const bool added = catalog_repository_.AddQuizToFavourites(
        transaction, user_id, quiz_id);
    transaction.Commit();
    return added;
}

void CatalogService::RemoveQuizFromFavourites(
    const boost::uuids::uuid& user_id,
    const boost::uuids::uuid& quiz_id) const {
    auto transaction = pg_->Begin(
        userver::storages::postgres::ClusterHostType::kMaster,
        userver::storages::postgres::TransactionOptions{});
    catalog_repository_.RemoveQuizFromFavourites(transaction, user_id, quiz_id);
    transaction.Commit();
}

}  // namespace RumpelQuiz
