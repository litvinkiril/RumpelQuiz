#include <userver/clients/dns/component.hpp>
#include <userver/clients/http/component_list.hpp>
#include <userver/components/component.hpp>
#include <userver/components/component_list.hpp>
#include <userver/components/minimal_server_component_list.hpp>
#include <userver/congestion_control/component.hpp>
#include <userver/server/handlers/auth/auth_checker_factory.hpp>
#include <userver/server/handlers/ping.hpp>
#include <userver/server/handlers/tests_control.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/testsuite/testsuite_support.hpp>
#include <userver/utils/daemon_run.hpp>

#include "auth/handlers/current_user/current_user_handler.hpp"
#include "auth/handlers/email_verification/email_verification_handler.hpp"
#include "auth/handlers/forgot_password/email_check/email_check.hpp"
#include "auth/handlers/forgot_password/update_password/update_password.hpp"
#include "auth/handlers/forgot_password/verify_code/verify_code.hpp"
#include "auth/handlers/login/login_handler.hpp"
#include "auth/handlers/session/session_handlers.hpp"
#include "auth/services/auth_session/auth_session_service.hpp"
#include "auth/handlers/registration/registration_handler.hpp"
#include "auth/handlers/resend_code/resend_code_handler.hpp"
#include "auth/middleware/jwt/jwt_auth_checker.hpp"
#include "auth/services/email_verification/email_verification_service.hpp"
#include "auth/services/forgot_password/forgot_password_service.hpp"
#include "auth/services/jwt/jwt_service.hpp"
#include "auth/services/login/login_service.hpp"
#include "auth/services/registration/registration_service.hpp"
#include "auth/services/resend_code/resend_code_service.hpp"
#include "demo/handlers/hello/hello_handler.hpp"
#include "demo/handlers/hello_postgres/hello_postgres_handler.hpp"
#include "email/postbox/postbox_client.hpp"
#include "email/service/email_service.hpp"
#include "s3client/s3client_component.hpp"
#include "frontend/frontend_handler.hpp"
#include "user/profile/handlers/get_profile_handler.hpp"
#include "user/profile/service/profile_service.hpp"
#include "education/handlers/get_university_admins_handler.hpp"
#include "education/service/education_service.hpp"
#include "education/handlers/search_university_people_handler.hpp"
#include "game/handlers/create_game_session_handler.hpp"
#include "game/handlers/game_play_handler.hpp"
#include "game/handlers/next_game_question_handler.hpp"
#include "game/handlers/close_game_session_handler.hpp"
#include "quiz/authoring/handlers/save_quiz_handler.hpp"
#include "quiz/authoring/handlers/get_quiz_handler.hpp"
#include "media/handlers/upload_image_handler.hpp"
#include "game/service/game_session_service.hpp"

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
          .Append<RumpelQuiz::FrontendHandler>()
          .Append<RumpelQuiz::FrontendHandler>("handler-frontend-css")
          .Append<RumpelQuiz::FrontendHandler>("handler-frontend-js")
          .Append<RumpelQuiz::FrontendHandler>("handler-frontend-quiz-js")
          .Append<RumpelQuiz::FrontendHandler>("handler-frontend-game-js")
          .Append<RumpelQuiz::FrontendHandler>("handler-frontend-qr-js")
          .Append<RumpelQuiz::Hello>()
          .Append<userver::components::Postgres>("postgres-db-1")
          .Append<RumpelQuiz::HelloPostgres>()
          .Append<RumpelQuiz::PostboxClientComponent>()
          .Append<RumpelQuiz::S3ClientComponent>()
          .Append<RumpelQuiz::QuizService>()
          .Append<RumpelQuiz::MediaService>()
          .Append<RumpelQuiz::UploadImageHandler>()
          .Append<RumpelQuiz::SaveQuizHandler>("handler-create-quiz")
          .Append<RumpelQuiz::SaveQuizHandler>("handler-update-quiz")
          .Append<RumpelQuiz::GetQuizHandler>()
          .Append<RumpelQuiz::GetQuizHandler>("handler-list-quizzes")
          .Append<RumpelQuiz::EmailService>()
          .Append<RumpelQuiz::RegistrationService>()
          .Append<RumpelQuiz::RegisterHandler>()
          .Append<RumpelQuiz::EmailVerificationService>()
          .Append<RumpelQuiz::VerifyEmailHandler>()
          .Append<RumpelQuiz::JwtServiceComponent>()
          .Append<RumpelQuiz::AuthSessionService>()
          .Append<RumpelQuiz::RefreshHandler>()
          .Append<RumpelQuiz::LogoutHandler>()
          .Append<RumpelQuiz::CurrentUserHandler>()
          .Append<RumpelQuiz::ProfileService>()
          .Append<RumpelQuiz::GetProfileHandler>()
          .Append<RumpelQuiz::EducationService>()
          .Append<RumpelQuiz::GetUniversityAdminsHandler>()
          .Append<RumpelQuiz::LoginHandler>()
          .Append<RumpelQuiz::LoginService>()
          .Append<RumpelQuiz::ResendCodeService>()
          .Append<RumpelQuiz::ResendCodeHandler>()
          .Append<RumpelQuiz::SearchUniversityPeopleHandler>()
          .Append<RumpelQuiz::ForgotPasswordEmailHandler>()
          .Append<RumpelQuiz::ForgotPasswordVerifyHandler>()
          .Append<RumpelQuiz::ForgotPasswordUpdateHandler>()
          .Append<RumpelQuiz::ForgotPasswordService>()
          .Append<RumpelQuiz::CreateGameSessionHandler>()
          .Append<RumpelQuiz::NextGameQuestionHandler>()
          .Append<RumpelQuiz::CloseGameSessionHandler>()
          .Append<RumpelQuiz::GameSessionService>()
          .Append<RumpelQuiz::GamePlayHandler>()
          .Append<RumpelQuiz::GamePlayHandler>("handler-game-join")
          .Append<RumpelQuiz::GamePlayHandler>("handler-game-answer");

  return userver::utils::DaemonMain(argc, argv, component_list);
}
