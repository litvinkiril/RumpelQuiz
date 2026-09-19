#include <userver/utest/utest.hpp>
#include <boost/uuid/string_generator.hpp>
#include "education/education_models.hpp"

namespace {
const auto fkn = boost::uuids::string_generator{}("00000000-0000-0000-0000-000000000001");
const auto fen = boost::uuids::string_generator{}("00000000-0000-0000-0000-000000000002");
const auto other = boost::uuids::string_generator{}("00000000-0000-0000-0000-000000000003");
}

UTEST(AdminAccess, UniversityAdminCanManageUnclassifiedGroups) {
    const RumpelQuiz::AdminAccess access{"university", {}};
    EXPECT_TRUE(access.CanManageUniversity());
    EXPECT_TRUE(access.CanManageFaculty(fkn));
    EXPECT_TRUE(access.CanManageGroup({}));
}

UTEST(AdminAccess, FacultyAdminCannotManageWholeUniversityOrUnassignedGroups) {
    const RumpelQuiz::AdminAccess access{"faculties", {fkn}};
    EXPECT_FALSE(access.CanManageUniversity());
    EXPECT_TRUE(access.CanManageFaculty(fkn));
    EXPECT_FALSE(access.CanManageFaculty(fen));
    EXPECT_FALSE(access.CanManageGroup({}));
    EXPECT_FALSE(access.CanManageGroup({other}));
}

UTEST(AdminAccess, EitherFacultyCanManageSharedProgramGroups) {
    const RumpelQuiz::AdminAccess first{"faculties", {fkn}};
    const RumpelQuiz::AdminAccess second{"faculties", {fen}};
    EXPECT_TRUE(first.CanManageGroup({fkn, fen}));
    EXPECT_TRUE(second.CanManageGroup({fkn, fen}));
}

UTEST(AdminAccess, EmptyAssignmentsAndUnknownScopeFailClosed) {
    const RumpelQuiz::AdminAccess empty{"faculties", {}};
    const RumpelQuiz::AdminAccess unknown{"unexpected", {fkn}};
    EXPECT_FALSE(empty.CanManageUniversity());
    EXPECT_FALSE(empty.CanManageGroup({fkn, fen}));
    EXPECT_FALSE(unknown.CanManageFaculty(fkn));
    EXPECT_FALSE(unknown.CanManageGroup({fkn}));
}
