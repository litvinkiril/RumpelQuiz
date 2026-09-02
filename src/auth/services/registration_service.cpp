#include "registration_service.hpp"

#include <chrono>
#include <stdexcept>

#include <userver/components/component_context.hpp>
#include <userver/logging/log.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/storages/postgres/options.hpp>

namespace RumpelQuiz {

namespace {

constexpr auto kVerificationCodeLifetime =
    std::chrono::minutes{15};

}

RegistrationService::RegistrationService(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context
)
    : ComponentBase(config, context),
      pg_(
          context
              .FindComponent<userver::components::Postgres>(
                  "postgres-db-1"
              )
              .GetCluster()
      ),
      email_service_(context.FindComponent<EmailService>()) {}

RegisterResult RegistrationService::Register(
    std::string_view email,
    std::string_view password,
    std::string_view password_confirmation
) const {
    if (password != password_confirmation) {
        return RegisterError::kPasswordsDoNotMatch;
    }

    const std::string password_hash =
        password_hasher_.HashPassword(password);

    const VerificationCode verification_code =
        verification_code_service_.Generate();

    const userver::storages::postgres::TimePointTz expires_at{
        std::chrono::system_clock::now() +
        kVerificationCodeLifetime
    };

    boost::uuids::uuid user_id;
    boost::uuids::uuid verification_id;

    {
        auto transaction = pg_->Begin(
            userver::storages::postgres::ClusterHostType::kMaster,
            userver::storages::postgres::TransactionOptions{}
        );

        const auto existing_user =
            user_repository_.FindByEmail(
                transaction,
                email
            );

        if (!existing_user.has_value()) {
            user_id = user_repository_.CreateUser(
                transaction,
                email,
                password_hash
            );
        } else {
            if (existing_user->email_verified) {
                return RegisterError::kEmailAlreadyExists;
            }

            user_id = existing_user->id;

            user_repository_.UpdateUnverifiedUser(
                transaction,
                user_id,
                password_hash
            );
        }

        verification_id =
            verification_repository_.UpsertCode(
                transaction,
                user_id,
                verification_code.hash,
                expires_at
            );

        transaction.Commit();
    }

    try {
        email_service_.SendVerificationCode(
            email,
            verification_code.plain
        );
    } catch (const std::exception&) {
        LOG_ERROR()
            << "Verification email delivery failed after "
               "registration commit";
        throw std::runtime_error(
            "Verification email delivery failed after registration commit"
        );
    }

    return RegistrationSuccess{verification_id};
}

}
