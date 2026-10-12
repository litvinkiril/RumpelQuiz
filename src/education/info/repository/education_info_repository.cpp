#include "education_info_repository.hpp"

#include <string>
#include <utility>

#include <userver/storages/postgres/io/optional.hpp>
#include <userver/storages/postgres/io/uuid.hpp>

namespace RumpelQuiz {

std::optional<GroupMembers> EducationInfoRepository::GetGroupMembers(
    userver::storages::postgres::Transaction& transaction,
    const boost::uuids::uuid& group_id) const {
    const auto result = transaction.Execute(
        R"(
            WITH assignments AS (
                SELECT membership_id, university_id, group_id, role
                FROM education.teacher_groups
                WHERE group_id = $1
                UNION ALL
                SELECT membership_id, university_id, group_id, role
                FROM education.student_groups
                WHERE group_id = $1
            ), members AS (
                SELECT assignment.group_id, assignment.university_id,
                    membership.user_id, membership.role
                FROM assignments AS assignment
                JOIN education.memberships AS membership
                  ON membership.id = assignment.membership_id
                 AND membership.university_id = assignment.university_id
                 AND membership.role = assignment.role
                WHERE membership.status = 'active'
            )
            SELECT member.user_id, member.role,
                profile.first_name, profile.last_name,
                profile.middle_name, profile.avatar_url
            FROM education.study_groups AS study_group
            LEFT JOIN members AS member
              ON member.group_id = study_group.id
             AND member.university_id = study_group.university_id
            LEFT JOIN users.profiles AS profile
              ON profile.user_id = member.user_id
            WHERE study_group.id = $1
            ORDER BY member.role,
                profile.last_name NULLS LAST,
                profile.first_name NULLS LAST,
                profile.middle_name NULLS LAST,
                member.user_id
        )",
        group_id
    );

    if (result.IsEmpty()) {
        return std::nullopt;
    }

    GroupMembers members;
    for (const auto& row : result) {
        const auto user_id =
            row["user_id"].As<std::optional<boost::uuids::uuid>>();
        if (!user_id) {
            continue;
        }

        GroupMember member{
            *user_id,
            row["first_name"].As<std::optional<std::string>>(),
            row["last_name"].As<std::optional<std::string>>(),
            row["middle_name"].As<std::optional<std::string>>(),
            row["avatar_url"].As<std::optional<std::string>>(),
        };
        auto& people = row["role"].As<std::string>() == "teacher"
            ? members.teachers : members.students;
        people.push_back(std::move(member));
    }

    return members;
}

}  // namespace RumpelQuiz
