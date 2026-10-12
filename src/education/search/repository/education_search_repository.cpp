#include "education_search_repository.hpp"

#include <userver/storages/postgres/io/array_types.hpp>
#include <userver/storages/postgres/io/optional.hpp>
#include <userver/storages/postgres/io/uuid.hpp>

namespace RumpelQuiz {

std::vector<EducationSearchItem> EducationSearchRepository::SearchPrograms(
    userver::storages::postgres::Transaction& transaction,
    const boost::uuids::uuid& university_id, const std::string& pattern) const {
  const auto result = transaction.Execute(
      R"(
            WITH matches AS (
                SELECT id, university_id, name
                FROM education.programs
                WHERE university_id = $1 AND name ILIKE $2 ESCAPE '!'
                ORDER BY name, id
                LIMIT 31
            )
            SELECT p.id, p.name,
                   COALESCE(array_agg(f.name ORDER BY f.name, f.id)
                       FILTER (WHERE f.id IS NOT NULL), ARRAY[]::text[])
                       AS facultet_names
            FROM matches AS p
            LEFT JOIN education.faculty_programs AS fp
                ON fp.program_id = p.id AND fp.university_id = p.university_id
            LEFT JOIN education.faculties AS f
                ON f.id = fp.faculty_id AND f.university_id = p.university_id
            GROUP BY p.id, p.name
            ORDER BY p.name, p.id
        )",
      university_id, pattern);
  std::vector<EducationSearchItem> items;
  items.reserve(result.Size());
  for (const auto& row : result) {
    items.push_back({
        row["id"].As<boost::uuids::uuid>(),
        row["name"].As<std::string>(),
        row["facultet_names"].As<std::vector<std::string>>(),
        std::nullopt,
    });
  }
  return items;
}

std::vector<EducationSearchItem> EducationSearchRepository::SearchGroups(
    userver::storages::postgres::Transaction& transaction,
    const boost::uuids::uuid& university_id, const std::string& pattern) const {
  const auto result = transaction.Execute(
      R"(
            WITH matches AS (
                SELECT id, university_id, name, program_id
                FROM education.study_groups
                WHERE university_id = $1 AND name ILIKE $2 ESCAPE '!'
                ORDER BY name, id
                LIMIT 31
            )
            SELECT g.id, g.name, p.name AS program_name,
                   COALESCE(array_agg(f.name ORDER BY f.name, f.id)
                       FILTER (WHERE f.id IS NOT NULL), ARRAY[]::text[])
                       AS facultet_names
            FROM matches AS g
            LEFT JOIN education.programs AS p
                ON p.id = g.program_id AND p.university_id = g.university_id
            LEFT JOIN education.faculty_programs AS fp
                ON fp.program_id = p.id AND fp.university_id = g.university_id
            LEFT JOIN education.faculties AS f
                ON f.id = fp.faculty_id AND f.university_id = g.university_id
            GROUP BY g.id, g.name, p.name
            ORDER BY g.name, g.id
        )",
      university_id, pattern);
  std::vector<EducationSearchItem> items;
  items.reserve(result.Size());
  for (const auto& row : result) {
    items.push_back({
        row["id"].As<boost::uuids::uuid>(),
        row["name"].As<std::string>(),
        row["facultet_names"].As<std::vector<std::string>>(),
        row["program_name"].As<std::optional<std::string>>(),
    });
  }
  return items;
}

}  // namespace RumpelQuiz
