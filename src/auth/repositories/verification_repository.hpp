#pragma once

#include <string_view>

#include <boost/uuid/uuid.hpp>

#include <userver/storages/postgres/transaction.hpp>
#include <userver/storages/postgres/io/chrono.hpp>

namespace RumpelQuiz {

class VerificationRepository {
public:
    boost::uuids::uuid UpsertCode(
        userver::storages::postgres::Transaction& transaction,
        const boost::uuids::uuid& user_id,
        std::string_view code_hash,
        userver::storages::postgres::TimePointTz expires_at
    ) const;
};

}