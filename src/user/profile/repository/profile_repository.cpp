#include "profile_repository.hpp"

#include <stdexcept>
#include <userver/storages/postgres/io/optional.hpp>
#include <userver/storages/postgres/io/row_types.hpp>

namespace RumpelQuiz {

void ProfileRepository::Create(
    userver::storages::postgres::Transaction& transaction,
    const boost::uuids::uuid& user_id,
    std::string_view first_name,
    std::string_view last_name,
    std::optional<std::string_view> middle_name,
    std::optional<std::string_view> avatar_url) const {
        const auto result = transaction.Execute(
        R"(
            INSERT INTO users.profiles (
                user_id,
                first_name,
                last_name,
                middle_name,
                avatar_url
            )
            VALUES ($1, $2, $3, $4, $5)
        )",
        user_id,
        first_name,
        last_name,
        middle_name,
        avatar_url
    );

    if (result.RowsAffected() != 1) {
        throw std::runtime_error{"Failed to create user profile"};
    }
}

std::optional<UserWithProfile>
ProfileRepository::FindUserWithProfileById(
    userver::storages::postgres::Transaction& transaction,
    const boost::uuids::uuid& user_id) const {

    const auto result = transaction.Execute(
        R"(
            SELECT
                u.email,
                u.email_verified,
                p.user_id AS profile_user_id,
                p.first_name,
                p.last_name,
                p.middle_name,
                p.avatar_url
            FROM auth.users AS u
            LEFT JOIN users.profiles AS p
                ON p.user_id = u.id
            WHERE u.id = $1
        )",
        user_id
    );

    if (result.IsEmpty()) {
        return std::nullopt;
    }

    return result.AsSingleRow<UserWithProfile>(
        userver::storages::postgres::kRowTag
    );
}


void ProfileRepository::Update(
    userver::storages::postgres::Transaction& transaction,
    const boost::uuids::uuid& user_id,
    std::string_view first_name,
    std::string_view last_name,
    std::optional<std::string_view> middle_name,
    std::optional<std::string_view> avatar_url) const {

    const auto result = transaction.Execute(
        R"(
            UPDATE users.profiles
            SET
                first_name = $2,
                last_name = $3,
                middle_name = $4,
                avatar_url = $5,
                updated_at = NOW()
            WHERE user_id = $1
        )",
        user_id,
        first_name,
        last_name,
        middle_name,
        avatar_url
    );

    if (result.RowsAffected() != 1) {
        throw std::runtime_error{"Failed to update user profile"};
    }
}


void ProfileRepository::Delete(
    userver::storages::postgres::Transaction& transaction,
    const boost::uuids::uuid& user_id) const {

    const auto result = transaction.Execute(
        R"(
            DELETE FROM users.profiles
            WHERE user_id = $1
        )",
        user_id
    );

    if (result.RowsAffected() != 1) {
        throw std::runtime_error{"Failed to delete user profile"};
    }
}

}
