#include "profile_service.hpp"

#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/storages/postgres/options.hpp>

namespace RumpelQuiz {
namespace {

std::vector<boost::uuids::uuid> get_university_ids(const std::vector<Membership>& memberships) {
    std::vector<boost::uuids::uuid> university_ids;
    university_ids.reserve(memberships.size());
    for (const Membership& current : memberships) {
        university_ids.push_back(current.university_id);
    }
    return university_ids;
}

std::map<boost::uuids::uuid, std::string> getUniversityPairIdName(const std::vector<University>& universities) {
    std::map<boost::uuids::uuid, std::string> universityPairIdName;
    for (const auto& current : universities ) {
        universityPairIdName[current.id] = current.name;
    }
    return universityPairIdName;
}

std::map<boost::uuids::uuid, std::string> get_pair_membership_id_group_name(
    const std::vector<StudentGroup>& groups) {
        std::map<boost::uuids::uuid, std::string> pair_membership_id_group_name;
        for (const auto& current : groups ) {
            pair_membership_id_group_name[current.membership_id] = current.name;
        }
        return pair_membership_id_group_name;
}

}  // namespace

ProfileService::ProfileService(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
    pg_(context.FindComponent<userver::components::Postgres>("postgres-db-1")
              .GetCluster()) {}


GetProfileResult ProfileService::GetProfile(const boost::uuids::uuid& user_id) const {
    auto transaction =
        pg_->Begin(userver::storages::postgres::ClusterHostType::kMaster,
                   userver::storages::postgres::TransactionOptions{});

    const auto user = user_repository_.FindById(transaction, user_id);
    if (!user) return GetProfileError::kUserNotFound;
    if (!user->email_verified) return GetProfileError::kEmailNotVerified;
      
    const auto user_profile = profile_repository_.FindById(transaction, user_id);

    if (!user_profile) {
        return GetProfileError::kUserProfileNotFound;  //такого быть не может так как все создает администратор
    }

    const auto memberships = education_repository_.GetAllActiveMembershipsByUserId(transaction, user_id);

    std::vector<boost::uuids::uuid> university_ids = get_university_ids(memberships);

    const auto universities = education_repository_.GetUniversitiesByIds(transaction, university_ids);

    std::map<boost::uuids::uuid, std::string> universityPairIdName =
            getUniversityPairIdName(universities);

    const auto studentgroups = 
        education_repository_.GetGroupsStudentbyMemberships(transaction, memberships);

    std::map<boost::uuids::uuid, std::string> pair_membership_id_group_name = 
            get_pair_membership_id_group_name(studentgroups);

    std::vector<Position> university_position;
    university_position.reserve(memberships.size());
    for (const auto& membership : memberships) {
        const auto university = universityPairIdName.find(membership.university_id);
        if (university == universityPairIdName.end()) {
            return GetProfileError::kUniversitiesNotFound;
        }
        std::string university_name = university->second;
        std::string role = membership.role;
        std::optional<std::string> group_name = std::nullopt;
        if (role == "student") {
            const auto group = pair_membership_id_group_name.find(membership.id);
            if (group != pair_membership_id_group_name.end()) {
                group_name = group->second;
            }
        }
        university_position.push_back(Position{
            university_name,
            role,
            group_name
        });
    }
    transaction.Commit();
    return FullProfile {
        user->email,
        user_profile->first_name,
        user_profile->last_name,
        user_profile->middle_name,
        user_profile->avatar_url,
        std::move(university_position)
    };
}
}
