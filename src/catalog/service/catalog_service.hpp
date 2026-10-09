#pragma once

#include <string_view>

#include <boost/uuid/uuid.hpp>

#include <userver/components/component_base.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include "catalog/repository/catalog_repository.hpp"
#include "catalog/search_models.hpp"

namespace RumpelQuiz {

class CatalogService final
    : public userver::components::ComponentBase {
public:
    static constexpr std::string_view kName = "catalog-service";

    CatalogService(
        const userver::components::ComponentConfig& config,
        const userver::components::ComponentContext& context);

    SearchTestsResult SearchTests(
        const boost::uuids::uuid& user_id,
        bool is_favourite,
        const std::string& name,
        int count_spend) const;

    bool AddToFavourites(
        const boost::uuids::uuid& user_id,
        const boost::uuids::uuid& test_id) const;

    void RemoveFromFavourites(
        const boost::uuids::uuid& user_id,
        const boost::uuids::uuid& test_id) const;

    SearchQuizzesResult SearchQuizzes(
        const boost::uuids::uuid& user_id,
        bool is_favourite,
        const std::string& name,
        int count_spend) const;

    bool AddQuizToFavourites(
        const boost::uuids::uuid& user_id,
        const boost::uuids::uuid& quiz_id) const;

    void RemoveQuizFromFavourites(
        const boost::uuids::uuid& user_id,
        const boost::uuids::uuid& quiz_id) const;

private:
    userver::storages::postgres::ClusterPtr pg_;
    CatalogRepository catalog_repository_;
};

}  // namespace RumpelQuiz
