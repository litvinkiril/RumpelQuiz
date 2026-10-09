#include "catalog_repository.hpp"

#include <stdexcept>
#include <userver/storages/postgres/io/uuid.hpp>

namespace RumpelQuiz {

SearchTestsResult CatalogRepository::SearchTests(
    userver::storages::postgres::Transaction& transaction,
    const boost::uuids::uuid& user_id,
    bool is_favourite,
    const std::string& name,
    int count_spend) const {

    if (count_spend < 0) {
        throw std::invalid_argument("Search offset must be non-negative");
    }

    const auto rows = transaction.Execute(
        R"(
            WITH accessible_tests AS (
                SELECT
                    t.id,
                    t.author_id,
                    t.name,
                    t.created_at,
                    EXISTS (
                        SELECT 1
                        FROM test.favourites AS f
                        WHERE f.user_id = $1
                          AND f.test_id = t.id
                    ) AS is_favourite
                FROM test.tests AS t
                WHERE EXISTS (
                    SELECT 1
                    FROM education.memberships AS m
                    WHERE m.user_id = $1
                      AND m.university_id = t.university_id
                      AND m.status = 'active'
                      AND m.role IN ('teacher', 'admin')
                )
                AND (
                    $3::text = ''
                    OR strpos(
                        lower(translate(
                            t.name,
                            'АБВГДЕЁЖЗИЙКЛМНОПРСТУФХЦЧШЩЪЫЬЭЮЯ',
                            'абвгдеёжзийклмнопрстуфхцчшщъыьэюя'
                        )),
                        lower(translate(
                            $3::text,
                            'АБВГДЕЁЖЗИЙКЛМНОПРСТУФХЦЧШЩЪЫЬЭЮЯ',
                            'абвгдеёжзийклмнопрстуфхцчшщъыьэюя'
                        ))
                    ) > 0
                )
            ),
            selected_tests AS (
                SELECT *
                FROM accessible_tests
                WHERE NOT $2::boolean OR is_favourite
                ORDER BY created_at DESC, id
                LIMIT 10 OFFSET $4
            )
            SELECT
                t.id AS test_id,
                COALESCE(p.first_name, '') AS creator_first_name,
                COALESCE(p.last_name, '') AS creator_last_name,
                t.name AS title,
                (
                    SELECT COUNT(*)::integer
                    FROM test.questions AS q
                    WHERE q.test_id = t.id
                ) AS question_count,
                t.is_favourite
            FROM selected_tests AS t
            LEFT JOIN users.profiles AS p
                ON p.user_id = t.author_id
            ORDER BY t.created_at DESC, t.id
        )",
        user_id,
        is_favourite,
        name,
        count_spend
    );

    SearchTestsResult result;
    result.tests_info.reserve(rows.Size());

    for (const auto& row : rows) {
        result.tests_info.push_back(TestInfo{
            row["test_id"].As<boost::uuids::uuid>(),
            row["creator_first_name"].As<std::string>(),
            row["creator_last_name"].As<std::string>(),
            row["title"].As<std::string>(),
            row["question_count"].As<int>(),
            row["is_favourite"].As<bool>()
        });
    }

    return result;
}

bool CatalogRepository::AddToFavourites(
    userver::storages::postgres::Transaction& transaction,
    const boost::uuids::uuid& user_id,
    const boost::uuids::uuid& test_id) const {
    const auto accessible = transaction.Execute(
        R"(
            SELECT 1 FROM test.tests AS t
            WHERE t.id = $2 AND EXISTS (
                SELECT 1 FROM education.memberships AS m
                WHERE m.user_id = $1
                  AND m.university_id = t.university_id
                  AND m.status = 'active'
                  AND m.role IN ('teacher', 'admin')
            )
        )",
        user_id, test_id);
    if (accessible.IsEmpty()) return false;

    transaction.Execute(
        R"(
            INSERT INTO test.favourites (user_id, test_id) VALUES ($1, $2)
            ON CONFLICT (user_id, test_id) DO NOTHING
        )",
        user_id, test_id);
    return true;
}

void CatalogRepository::RemoveFromFavourites(
    userver::storages::postgres::Transaction& transaction,
    const boost::uuids::uuid& user_id,
    const boost::uuids::uuid& test_id) const {
    transaction.Execute(
        "DELETE FROM test.favourites WHERE user_id = $1 AND test_id = $2",
        user_id, test_id);
}

SearchQuizzesResult CatalogRepository::SearchQuizzes(
    userver::storages::postgres::Transaction& transaction,
    const boost::uuids::uuid& user_id,
    bool is_favourite,
    const std::string& name,
    int count_spend) const {

    if (count_spend < 0) {
        throw std::invalid_argument("Search offset must be non-negative");
    }

    const auto rows = transaction.Execute(
        R"(
            WITH accessible_quizzes AS (
                SELECT
                    t.id,
                    t.author_id,
                    t.name,
                    t.created_at,
                    EXISTS (
                        SELECT 1
                        FROM quiz.favourites AS f
                        WHERE f.user_id = $1
                          AND f.quiz_id = t.id
                    ) AS is_favourite
                FROM quiz.quizzes AS t
                WHERE EXISTS (
                    SELECT 1
                    FROM education.memberships AS m
                    WHERE m.user_id = $1
                      AND m.university_id = t.university_id
                      AND m.status = 'active'
                      AND m.role IN ('teacher', 'admin')
                )
                AND (
                    $3::text = ''
                    OR strpos(
                        lower(translate(
                            t.name,
                            'АБВГДЕЁЖЗИЙКЛМНОПРСТУФХЦЧШЩЪЫЬЭЮЯ',
                            'абвгдеёжзийклмнопрстуфхцчшщъыьэюя'
                        )),
                        lower(translate(
                            $3::text,
                            'АБВГДЕЁЖЗИЙКЛМНОПРСТУФХЦЧШЩЪЫЬЭЮЯ',
                            'абвгдеёжзийклмнопрстуфхцчшщъыьэюя'
                        ))
                    ) > 0
                )
            ),
            selected_quizzes AS (
                SELECT *
                FROM accessible_quizzes
                WHERE NOT $2::boolean OR is_favourite
                ORDER BY created_at DESC, id
                LIMIT 10 OFFSET $4
            )
            SELECT
                t.id AS quiz_id,
                COALESCE(p.first_name, '') AS creator_first_name,
                COALESCE(p.last_name, '') AS creator_last_name,
                t.name AS title,
                (
                    SELECT COUNT(*)::integer
                    FROM quiz.questions AS q
                    WHERE q.quiz_id = t.id
                ) AS question_count,
                t.is_favourite
            FROM selected_quizzes AS t
            LEFT JOIN users.profiles AS p
                ON p.user_id = t.author_id
            ORDER BY t.created_at DESC, t.id
        )",
        user_id,
        is_favourite,
        name,
        count_spend
    );

    SearchQuizzesResult result;
    result.quizzes_info.reserve(rows.Size());

    for (const auto& row : rows) {
        result.quizzes_info.push_back(QuizInfo{
            row["quiz_id"].As<boost::uuids::uuid>(),
            row["creator_first_name"].As<std::string>(),
            row["creator_last_name"].As<std::string>(),
            row["title"].As<std::string>(),
            row["question_count"].As<int>(),
            row["is_favourite"].As<bool>()
        });
    }

    return result;
}

bool CatalogRepository::AddQuizToFavourites(
    userver::storages::postgres::Transaction& transaction,
    const boost::uuids::uuid& user_id,
    const boost::uuids::uuid& quiz_id) const {
    const auto accessible = transaction.Execute(
        R"(
            SELECT 1 FROM quiz.quizzes AS t
            WHERE t.id = $2 AND EXISTS (
                SELECT 1 FROM education.memberships AS m
                WHERE m.user_id = $1
                  AND m.university_id = t.university_id
                  AND m.status = 'active'
                  AND m.role IN ('teacher', 'admin')
            )
        )",
        user_id, quiz_id);
    if (accessible.IsEmpty()) return false;

    transaction.Execute(
        R"(
            INSERT INTO quiz.favourites (user_id, quiz_id) VALUES ($1, $2)
            ON CONFLICT (user_id, quiz_id) DO NOTHING
        )",
        user_id, quiz_id);
    return true;
}

void CatalogRepository::RemoveQuizFromFavourites(
    userver::storages::postgres::Transaction& transaction,
    const boost::uuids::uuid& user_id,
    const boost::uuids::uuid& quiz_id) const {
    transaction.Execute(
        "DELETE FROM quiz.favourites WHERE user_id = $1 AND quiz_id = $2",
        user_id, quiz_id);
}

}  // namespace RumpelQuiz
