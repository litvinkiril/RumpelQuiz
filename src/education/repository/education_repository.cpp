#include "education_repository.hpp"
#include <string>
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
}  // namespace RumpelQuiz
