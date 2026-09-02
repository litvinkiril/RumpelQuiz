#include "verification_repository.hpp"

namespace RumpelQuiz {
    boost::uuids::uuid VerificationRepository::UpsertCode(
        userver::storages::postgres::Transaction& transaction,
        const boost::uuids::uuid& user_id,
        std::string_view code_hash,
        userver::storages::postgres::TimePointTz expires_at
    ) const{
        const auto result = transaction.Execute(
        R"(
            INSERT INTO auth.verification_codes (
                id,
                user_id,
                code_hash,
                expires_at
            )
            VALUES (
                gen_random_uuid(),
                $1,
                $2,
                $3
            )

            ON CONFLICT (user_id)
            DO UPDATE SET
                id = gen_random_uuid(),
                code_hash = EXCLUDED.code_hash,
                expires_at = EXCLUDED.expires_at,
                created_at = NOW()

            RETURNING id
        )",
        user_id,
        code_hash,
        expires_at
    );

    return result[0]["id"].As<boost::uuids::uuid>();
    }
}
