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
        "SELECT id, university_id, role, admin_scope FROM education.memberships "
        "WHERE user_id = $1 AND status = 'active' ORDER BY id", user_id);
    std::vector<Membership> memberships;
    memberships.reserve(result.Size());
    for (const auto& row : result) {
        memberships.push_back(Membership{
            row["id"].As<boost::uuids::uuid>(), user_id,
            row["university_id"].As<boost::uuids::uuid>(),
            row["role"].As<std::string>(), "active",
            row["admin_scope"].As<std::optional<std::string>>()});
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
                g.name AS group_name,
                m.admin_scope
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
            row["group_name"].As<std::optional<std::string>>(),
            row["admin_scope"].As<std::optional<std::string>>()
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

bool EducationRepository::AddMembership(
    userver::storages::postgres::Transaction& transaction,
    const boost::uuids::uuid& user_id,
    const boost::uuids::uuid& university_id,
    std::string_view role,
    std::optional<boost::uuids::uuid> facultet_id
) const {
    std::optional<std::string> admin_scope;
    if (role == "admin") {
        admin_scope = facultet_id ? "faculties" : "university";
    }

    const auto result = transaction.Execute(
        R"(
            INSERT INTO education.memberships (
                user_id,
                university_id,
                role,
                status,
                admin_scope
            )
            VALUES ($1, $2, $3, 'active', $4)
            ON CONFLICT (user_id, university_id, role) DO NOTHING
            RETURNING id
        )",
        user_id,
        university_id,
        role,
        admin_scope
    );

    if (result.IsEmpty()) {
        return false;
    }

    if (facultet_id) {
        const auto membership_id =
            result[0]["id"].As<boost::uuids::uuid>();

        transaction.Execute(
            R"(
                INSERT INTO education.admin_faculties (
                    membership_id,
                    faculty_id,
                    university_id
                )
                VALUES ($1, $2, $3)
            )",
            membership_id,
            *facultet_id,
            university_id
        );
    }

    return true;
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
std::optional<AdminAccess> EducationRepository::GetAdminAccess(
    userver::storages::postgres::Transaction& transaction,
    const boost::uuids::uuid& user_id,
    const boost::uuids::uuid& university_id) const {
    const auto result = transaction.Execute(R"(
        SELECT m.admin_scope,
            ARRAY(SELECT af.faculty_id FROM education.admin_faculties AS af
                WHERE af.membership_id = m.id AND af.university_id = m.university_id
                ORDER BY af.faculty_id) AS faculty_ids
        FROM education.memberships AS m
        WHERE m.user_id = $1 AND m.university_id = $2
            AND m.role = 'admin' AND m.status = 'active'
    )", user_id, university_id);
    if (result.IsEmpty()) return std::nullopt;
    return AdminAccess{
        result[0]["admin_scope"].As<std::string>(),
        result[0]["faculty_ids"].As<std::vector<boost::uuids::uuid>>()};
}

bool EducationRepository::FacultyExists(
    userver::storages::postgres::Transaction& transaction,
    const boost::uuids::uuid& university_id,
    const boost::uuids::uuid& faculty_id) const {
    return transaction.Execute(
        "SELECT EXISTS (SELECT 1 FROM education.faculties "
        "WHERE university_id = $1 AND id = $2)", university_id, faculty_id).AsSingleRow<bool>();
}

std::optional<std::vector<boost::uuids::uuid>> EducationRepository::GetGroupFacultyIds(
    userver::storages::postgres::Transaction& transaction,
    const boost::uuids::uuid& university_id,
    const boost::uuids::uuid& group_id) const {
    const auto result = transaction.Execute(R"(
        SELECT ARRAY(SELECT fp.faculty_id FROM education.faculty_programs AS fp
            WHERE fp.program_id = g.program_id AND fp.university_id = g.university_id
            ORDER BY fp.faculty_id) AS faculty_ids
        FROM education.study_groups AS g
        WHERE g.university_id = $1 AND g.id = $2
    )", university_id, group_id);
    if (result.IsEmpty()) return std::nullopt;
    return result[0]["faculty_ids"].As<std::vector<boost::uuids::uuid>>();
}

bool EducationRepository::HasActiveMembership(
    userver::storages::postgres::Transaction& transaction,
    const boost::uuids::uuid& user_id,
    const boost::uuids::uuid& university_id) const {
    return transaction.Execute(
        R"(
            SELECT EXISTS (
                SELECT 1
                FROM education.memberships
                WHERE user_id = $1
                  AND university_id = $2
                  AND status = 'active'
            )
        )",
        user_id,
        university_id
    ).AsSingleRow<bool>();
}

std::vector<UniversityPerson>
EducationRepository::SearchPeopleByUniversityId(
    userver::storages::postgres::Transaction& transaction,
    const boost::uuids::uuid& university_id,
    const PeopleSearchParams& params) const {
    const auto result = transaction.Execute(
        R"(
            WITH matching_people AS (
                SELECT
                    u.id AS user_id,
                    u.email,
                    p.first_name,
                    p.last_name,
                    p.middle_name,
                    p.avatar_url
                FROM auth.users AS u
                LEFT JOIN users.profiles AS p
                    ON p.user_id = u.id
                WHERE EXISTS (
                    SELECT 1
                    FROM education.memberships AS membership
                    WHERE membership.user_id = u.id
                      AND membership.university_id = $1
                      AND membership.status = 'active'
                )
                AND NOT EXISTS (
                    -- Explicit Russian case mapping also works with a C locale database.
                    SELECT 1
                    FROM unnest($2::text[]) AS query_word(word)
                    WHERE strpos(
                        lower(translate(COALESCE(p.last_name, ''), 'АБВГДЕЁЖЗИЙКЛМНОПРСТУФХЦЧШЩЪЫЬЭЮЯ', 'абвгдеёжзийклмнопрстуфхцчшщъыьэюя')),
                        lower(translate(query_word.word, 'АБВГДЕЁЖЗИЙКЛМНОПРСТУФХЦЧШЩЪЫЬЭЮЯ', 'абвгдеёжзийклмнопрстуфхцчшщъыьэюя'))
                    ) = 0
                    AND strpos(
                        lower(translate(COALESCE(p.first_name, ''), 'АБВГДЕЁЖЗИЙКЛМНОПРСТУФХЦЧШЩЪЫЬЭЮЯ', 'абвгдеёжзийклмнопрстуфхцчшщъыьэюя')),
                        lower(translate(query_word.word, 'АБВГДЕЁЖЗИЙКЛМНОПРСТУФХЦЧШЩЪЫЬЭЮЯ', 'абвгдеёжзийклмнопрстуфхцчшщъыьэюя'))
                    ) = 0
                    AND strpos(
                        lower(translate(COALESCE(p.middle_name, ''), 'АБВГДЕЁЖЗИЙКЛМНОПРСТУФХЦЧШЩЪЫЬЭЮЯ', 'абвгдеёжзийклмнопрстуфхцчшщъыьэюя')),
                        lower(translate(query_word.word, 'АБВГДЕЁЖЗИЙКЛМНОПРСТУФХЦЧШЩЪЫЬЭЮЯ', 'абвгдеёжзийклмнопрстуфхцчшщъыьэюя'))
                    ) = 0
                )
            ),
            selected_people AS (
                SELECT *
                FROM matching_people
                ORDER BY
                    last_name NULLS LAST,
                    first_name NULLS LAST,
                    middle_name NULLS LAST,
                    user_id
                LIMIT $3
                OFFSET $4
            )
            SELECT
                person.user_id,
                person.email,
                person.first_name,
                person.last_name,
                person.middle_name,
                person.avatar_url,

                ARRAY(
                    SELECT DISTINCT membership.role
                    FROM education.memberships AS membership
                    WHERE membership.user_id = person.user_id
                      AND membership.university_id = $1
                      AND membership.status = 'active'
                    ORDER BY membership.role
                ) AS roles,

                student.membership_id AS student_membership_id,
                student.group_id,
                student.group_name,

                ARRAY(
                    SELECT faculty.id
                    FROM education.faculty_programs AS faculty_program
                    JOIN education.faculties AS faculty
                      ON faculty.id = faculty_program.faculty_id
                     AND faculty.university_id =
                         faculty_program.university_id
                    WHERE faculty_program.program_id = student.program_id
                      AND faculty_program.university_id = $1
                    ORDER BY faculty.name, faculty.id
                ) AS faculty_ids,

                ARRAY(
                    SELECT faculty.name
                    FROM education.faculty_programs AS faculty_program
                    JOIN education.faculties AS faculty
                      ON faculty.id = faculty_program.faculty_id
                     AND faculty.university_id =
                         faculty_program.university_id
                    WHERE faculty_program.program_id = student.program_id
                      AND faculty_program.university_id = $1
                    ORDER BY faculty.name, faculty.id
                ) AS faculty_names

            FROM selected_people AS person

            LEFT JOIN LATERAL (
                SELECT
                    membership.id AS membership_id,
                    study_group.id AS group_id,
                    study_group.name AS group_name,
                    study_group.program_id
                FROM education.memberships AS membership
                LEFT JOIN education.student_groups AS student_group
                  ON student_group.membership_id = membership.id
                 AND student_group.university_id =
                     membership.university_id
                LEFT JOIN education.study_groups AS study_group
                  ON study_group.id = student_group.group_id
                 AND study_group.university_id =
                     student_group.university_id
                WHERE membership.user_id = person.user_id
                  AND membership.university_id = $1
                  AND membership.role = 'student'
                  AND membership.status = 'active'
                LIMIT 1
            ) AS student ON TRUE

            ORDER BY
                person.last_name NULLS LAST,
                person.first_name NULLS LAST,
                person.middle_name NULLS LAST,
                person.user_id
        )",
        university_id,
        params.words,
        params.limit + 1,
        params.offset
    );

    std::vector<UniversityPerson> people;
    people.reserve(result.Size());

    for (const auto& row : result) {
        std::optional<StudentDetails> student_details;

        const auto student_membership_id =
            row["student_membership_id"]
                .As<std::optional<boost::uuids::uuid>>();

        if (student_membership_id) {
            StudentDetails details;

            const auto group_id =
                row["group_id"].As<std::optional<boost::uuids::uuid>>();
            const auto group_name =
                row["group_name"].As<std::optional<std::string>>();

            if (group_id && group_name) {
                details.group = PersonGroup{*group_id, *group_name};
            }

            const auto faculty_ids =
                row["faculty_ids"]
                    .As<std::vector<boost::uuids::uuid>>();
            const auto faculty_names =
                row["faculty_names"].As<std::vector<std::string>>();

            details.faculties.reserve(faculty_ids.size());

            for (std::size_t index = 0;
                 index < faculty_ids.size() &&
                 index < faculty_names.size();
                 ++index) {
                details.faculties.push_back(PersonFaculty{
                    faculty_ids[index],
                    faculty_names[index],
                });
            }

            student_details = std::move(details);
        }

        people.push_back(UniversityPerson{
            row["user_id"].As<boost::uuids::uuid>(),
            row["first_name"].As<std::optional<std::string>>(),
            row["last_name"].As<std::optional<std::string>>(),
            row["middle_name"].As<std::optional<std::string>>(),
            row["avatar_url"].As<std::optional<std::string>>(),
            row["email"].As<std::string>(),
            row["roles"].As<std::vector<std::string>>(),
            std::move(student_details),
        });
    }

    return people;
}

std::vector<std::string> EducationRepository::GetActiveRoles(
    userver::storages::postgres::Transaction& transaction,
    const boost::uuids::uuid& user_id,
    const boost::uuids::uuid& university_id
) const {
    const auto result = transaction.Execute(
        R"(
            SELECT role
            FROM education.memberships
            WHERE user_id = $1
              AND university_id = $2
              AND status = 'active'
            ORDER BY role
        )",
        user_id,
        university_id
    );

    return result.AsContainer<std::vector<std::string>>();
}
}  // namespace RumpelQuiz
