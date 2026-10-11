#pragma once
#include <boost/uuid/uuid.hpp>
#include <optional>
#include <string>
#include <variant>
#include <vector>
#include <algorithm>

namespace RumpelQuiz {

enum class StructureFilterKind {
    kUniversity,
    kFaculty,
    kProgram,
};

struct StructureFilter {
    StructureFilterKind kind;
    boost::uuids::uuid id;
};


struct University {
    boost::uuids::uuid id;
    std::string name;
};
struct StudyGroup {
    boost::uuids::uuid id;
    boost::uuids::uuid university_id;
    std::string name;
    std::optional<boost::uuids::uuid> program_id;
};
struct Faculty {
    boost::uuids::uuid id;
    boost::uuids::uuid university_id;
    std::string name;
};
struct Program {
    boost::uuids::uuid id;
    boost::uuids::uuid university_id;
    std::string name;
};
// Returned only for an active admin membership in the requested university.
struct AdminAccess {
    std::string scope;
    std::vector<boost::uuids::uuid> faculty_ids;

    bool CanManageUniversity() const { return scope == "university"; }
    bool CanManageFaculty(const boost::uuids::uuid& faculty_id) const {
        return CanManageUniversity() || (scope == "faculties" &&
            std::find(faculty_ids.begin(), faculty_ids.end(), faculty_id) != faculty_ids.end());
    }
    bool CanManageGroup(const std::vector<boost::uuids::uuid>& group_faculties) const {
        return CanManageUniversity() || std::any_of(
            group_faculties.begin(), group_faculties.end(),
            [this](const auto& id) { return CanManageFaculty(id); });
    }
};
struct Membership {
    boost::uuids::uuid id;
    boost::uuids::uuid user_id;
    boost::uuids::uuid university_id;
    std::string role;
    std::string status;
    std::optional<std::string> admin_scope;
};
struct StudentGroup {
    boost::uuids::uuid membership_id;
    boost::uuids::uuid university_id;
    boost::uuids::uuid group_id;
    std::string name;
};

struct Position {
    boost::uuids::uuid university_id;
    std::string university_name;
    std::string role;
    std::optional<boost::uuids::uuid> group_id;
    std::optional<std::string> group_name;
    std::optional<std::string> admin_scope;
};

struct UniversityAdmin {
    boost::uuids::uuid membership_id;
    std::optional<std::string> first_name;
    std::optional<std::string> last_name;
    std::optional<std::string> middle_name;
    std::optional<std::string> avatar_url;
    std::string email;
};

enum class GetUniversityAdminsError {
    kAccessDenied
};

using GetUniversityAdminsResult = std::variant<
    std::vector<UniversityAdmin>,
    GetUniversityAdminsError
>;


struct PeopleSearchParams {
    std::vector<std::string> words;
    int limit;
    int offset;
};

struct PersonGroup {
    boost::uuids::uuid id;
    std::string name;
};

struct PersonFaculty {
    boost::uuids::uuid id;
    std::string name;
};

struct StudentDetails {
    std::optional<PersonGroup> group;
    std::vector<PersonFaculty> faculties;
};

struct UniversityPerson {
    boost::uuids::uuid user_id;
    std::optional<std::string> first_name;
    std::optional<std::string> last_name;
    std::optional<std::string> middle_name;
    std::optional<std::string> avatar_url;
    std::string email;
    std::vector<std::string> roles;
    std::optional<StudentDetails> student_details;
};

struct SearchUniversityPeopleSuccess {
    std::vector<UniversityPerson> people;
    bool has_more;
    std::optional<int> next_offset;
};

enum class SearchUniversityPeopleError {
    kAccessDenied,
};

using SearchUniversityPeopleResult = std::variant<
    SearchUniversityPeopleSuccess,
    SearchUniversityPeopleError
>;


}  // namespace RumpelQuiz
