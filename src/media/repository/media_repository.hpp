#pragma once

#include <boost/uuid/uuid.hpp>

#include <userver/storages/postgres/transaction.hpp>

#include "media/media_models.hpp"

namespace RumpelQuiz {

class MediaRepository {
public:
    void CreatePending(
        userver::storages::postgres::Transaction& transaction,
        const MediaRecord& media
    ) const;

    void MarkReady(
        userver::storages::postgres::Transaction& transaction,
        const boost::uuids::uuid& media_id
    ) const;
};

}  // namespace RumpelQuiz