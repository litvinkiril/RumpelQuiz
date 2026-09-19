#include "education_repository.hpp"
#include <string>
#include <userver/storages/postgres/io/optional.hpp>
#include <userver/storages/postgres/io/array_types.hpp>
#include <userver/storages/postgres/io/uuid.hpp>

namespace RumpelQuiz {
std::vector<Membership> EducationRepository::GetAllActiveMembershipsByUserId(
    userver::storages::postgres::Transaction& transaction,
    const boost::uuids::uuid& user_id) const {
    const auto result = transaction.Execute(
        "SELECT id, university_id, role FROM education.memberships "
        "WHERE user_id = $1 AND status = 'active' ORDER BY id", user_id);
    std::vector<Membership> memberships;
    memberships.reserve(result.Size());
    for (const auto& row : result) {
        memberships.push_back(Membership{
            row["id"].As<boost::uuids::uuid>(), user_id,
            row["university_id"].As<boost::uuids::uuid>(),
            row["role"].As<std::string>(), "active"});
    }
    return memberships;
}

std::vector<University> EducationRepository::GetUniversitiesByIds(
    userver::storages::postgres::Transaction& transaction,
    const std::vector<boost::uuids::uuid>& university_ids) const {
    if (university_ids.empty()) return {};
    const auto result = transaction.Execute(
        "SELECT id, name FROM education.universities WHERE id = ANY($1::uuid[])",
        university_ids);
    std::vector<University> universities;
    universities.reserve(result.Size());
    for (const auto& row : result) {
        universities.push_back(University{
            row["id"].As<boost::uuids::uuid>(), row["name"].As<std::string>()});
    }
    return universities;
}

std::vector<StudentGroup> EducationRepository::GetGroupsStudentbyMemberships(
    userver::storages::postgres::Transaction& transaction,
    const std::vector<Membership>& memberships) const {
    std::vector<boost::uuids::uuid> studentMemberships;
    studentMemberships.reserve(memberships.size());
    for (const Membership& current : memberships) {
        if (current.role == "student") studentMemberships.push_back(current.id);
    }
    if (studentMemberships.empty()) return {};
    const auto result = transaction.Execute(
        "SELECT sg.membership_id, sg.university_id, sg.group_id, g.name "
        "FROM education.student_groups AS sg "
        "JOIN education.study_groups AS g "
        "ON g.id = sg.group_id AND g.university_id = sg.university_id "
        "WHERE sg.membership_id = ANY($1::uuid[])", studentMemberships);
    std::vector<StudentGroup> groups;
    groups.reserve(result.Size());
    for (const auto& row : result) {
        groups.push_back(StudentGroup{
            row["membership_id"].As<boost::uuids::uuid>(),
            row["university_id"].As<boost::uuids::uuid>(),
            row["group_id"].As<boost::uuids::uuid>(),
            row["name"].As<std::string>()});
    }
    return groups;
}

std::vector<Position> EducationRepository::GetPositionsByUserId(
    userver::storages::postgres::Transaction& transaction,
    const boost::uuids::uuid& user_id) const {

    const auto result = transaction.Execute(
        R"(
            SELECT
                u.id AS university_id,
                u.name AS university_name,
                m.role,
                g.id AS group_id,
                g.name AS group_name
            FROM education.memberships AS m
            JOIN education.universities AS u
                ON u.id = m.university_id
            LEFT JOIN education.student_groups AS sg
                ON sg.membership_id = m.id
                AND sg.university_id = m.university_id
                AND m.role = 'student'
            LEFT JOIN education.study_groups AS g
                ON g.id = sg.group_id
                AND g.university_id = sg.university_id
            WHERE m.user_id = $1
                AND m.status = 'active'
            ORDER BY u.name, m.role, m.id
        )",
        user_id
    );

    std::vector<Position> positions;
    positions.reserve(result.Size());

    for (const auto& row : result) {
        positions.push_back(Position{
            row["university_id"].As<boost::uuids::uuid>(),
            row["university_name"].As<std::string>(),
            row["role"].As<std::string>(),
            row["group_id"].As<std::optional<boost::uuids::uuid>>(),
            row["group_name"].As<std::optional<std::string>>()
        });
    }

    return positions;
}

bool EducationRepository::HasActiveAdminMembership(
    userver::storages::postgres::Transaction& transaction,
    const boost::uuids::uuid& user_id,
    const boost::uuids::uuid& university_id) const {
    const auto result = transaction.Execute(
        R"(
            SELECT EXISTS (
                SELECT 1
                FROM education.memberships
                WHERE user_id = $1
                    AND university_id = $2
                    AND role = 'admin'
                    AND status = 'active'
            )
        )",
        user_id,
        university_id
    );

    return result.AsSingleRow<bool>();
}

std::vector<UniversityAdmin>
EducationRepository::GetActiveAdminsByUniversityId(
    userver::storages::postgres::Transaction& transaction,
    const boost::uuids::uuid& university_id) const {
    const auto result = transaction.Execute(
        R"(
            SELECT
                m.id AS membership_id,
                p.first_name,
                p.last_name,
                p.middle_name,
                p.avatar_url,
                u.email
            FROM education.memberships AS m
            JOIN auth.users AS u
                ON u.id = m.user_id
            LEFT JOIN users.profiles AS p
                ON p.user_id = m.user_id
            WHERE m.university_id = $1
                AND m.role = 'admin'
                AND m.status = 'active'
            ORDER BY
                p.last_name NULLS LAST,
                p.first_name NULLS LAST,
                p.middle_name NULLS LAST,
                m.id
        )",
        university_id
    );

    std::vector<UniversityAdmin> admins;
    admins.reserve(result.Size());

    for (const auto& row : result) {
        admins.push_back(UniversityAdmin{
            row["membership_id"].As<boost::uuids::uuid>(),
            row["first_name"].As<std::optional<std::string>>(),
            row["last_name"].As<std::optional<std::string>>(),
            row["middle_name"].As<std::optional<std::string>>(),
            row["avatar_url"].As<std::optional<std::string>>(),
            row["email"].As<std::string>()
        });
    }

    return admins;
}
}  // namespace RumpelQuiz
