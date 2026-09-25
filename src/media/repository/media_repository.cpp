#include "media_repository.hpp"

#include <stdexcept>

#include <userver/storages/postgres/io/uuid.hpp>

namespace RumpelQuiz {

void MediaRepository::CreatePending(
    userver::storages::postgres::Transaction& transaction,
    const MediaRecord& media
) const {
    transaction.Execute(
        R"(
            INSERT INTO media.images (
                id,
                owner_id,
                storage_key,
                content_type,
                size_bytes,
                width,
                height,
                status
            )
            VALUES ($1, $2, $3, $4, $5, $6, $7, 'pending')
        )",
        media.id,
        media.owner_id,
        media.storage_key,
        media.content_type,
        media.size_bytes,
        media.width,
        media.height
    );
}

void MediaRepository::MarkReady(
    userver::storages::postgres::Transaction& transaction,
    const boost::uuids::uuid& media_id
) const {
    const auto result = transaction.Execute(
        R"(
            UPDATE media.images
            SET
                status = 'ready',
                ready_at = NOW()
            WHERE id = $1
              AND status = 'pending'
            RETURNING id
        )",
        media_id
    );

    if (result.IsEmpty()) {
        throw std::runtime_error(
            "Cannot mark image ready: pending image not found"
        );
    }
}

}  // namespace RumpelQuiz