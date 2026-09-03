#include <userver/clients/dns/component.hpp>
#include <userver/clients/http/component_list.hpp>
#include <userver/components/component.hpp>
#include <userver/components/component_list.hpp>
#include <userver/components/minimal_server_component_list.hpp>
#include <userver/congestion_control/component.hpp>
#include <userver/server/handlers/auth/auth_checker_factory.hpp>
#include <userver/server/handlers/ping.hpp>
#include <userver/server/handlers/tests_control.hpp>
#include <userver/testsuite/testsuite_support.hpp>

#include <userver/storages/postgres/component.hpp>

#include <userver/utils/daemon_run.hpp>

#include <hello.hpp>
#include <hello_postgres.hpp>
#include "auth/handlers/current_user.hpp"
#include "auth/handlers/register.hpp"
#include "auth/handlers/verify_email.hpp"
#include "auth/handlers/login.hpp"
#include "auth/middleware/jwt_auth_checker.hpp"
#include "auth/services/email_verification_service.hpp"
#include "auth/services/jwt_service.hpp"
#include "auth/services/registration_service.hpp"
#include "auth/services/login_service.hpp"
#include "email/email_service.hpp"
#include "email/postbox_client.hpp"

int main(int argc, char* argv[]) {
  userver::server::handlers::auth::RegisterAuthCheckerFactory<
      RumpelQuiz::JwtAuthCheckerFactory>();

  auto component_list =
      userver::components::MinimalServerComponentList()
          .Append<userver::server::handlers::Ping>()
          .Append<userver::components::TestsuiteSupport>()
          .AppendComponentList(userver::clients::http::ComponentList())
          .Append<userver::clients::dns::Component>()
          .Append<userver::server::handlers::TestsControl>()
          .Append<userver::congestion_control::Component>()
          .Append<RumpelQuiz::Hello>()
          .Append<userver::components::Postgres>("postgres-db-1")
          .Append<RumpelQuiz::HelloPostgres>()
          .Append<RumpelQuiz::PostboxClientComponent>()
          .Append<RumpelQuiz::EmailService>()
          .Append<RumpelQuiz::RegistrationService>()
          .Append<RumpelQuiz::RegisterHandler>()
          .Append<RumpelQuiz::EmailVerificationService>()
          .Append<RumpelQuiz::VerifyEmailHandler>()
          .Append<RumpelQuiz::JwtServiceComponent>()
          .Append<RumpelQuiz::CurrentUserHandler>()
          .Append<RumpelQuiz::LoginHandler>()
          .Append<RumpelQuiz::LoginService>()
          ;

  return userver::utils::DaemonMain(argc, argv, component_list);
}
