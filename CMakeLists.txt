cmake_minimum_required(VERSION 3.12...3.31)
project(RumpelQuiz CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(DownloadUserver)

find_package(
    userver
    COMPONENTS core #
               postgresql
    QUIET
)
if(NOT userver_FOUND)
    # Tries TRY_DIR first, falls back to downloading userver from GitHub using CPM.
    download_userver(TRY_DIR third_party/userver GIT_TAG v3.1)
endif()

userver_setup_environment()

find_package(OpenSSL REQUIRED)
find_library(CRYPT_LIBRARY crypt REQUIRED)

# Embed the frontend into the service: one origin, no extra server or npm build.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    frontend/index.html frontend/styles.css frontend/app.js)
file(READ frontend/index.html FRONTEND_HTML)
file(READ frontend/styles.css FRONTEND_CSS)
file(READ frontend/app.js FRONTEND_JS)
configure_file(src/frontend/assets.hpp.in generated/frontend_assets.hpp @ONLY)


# Common sources


add_library(
    ${PROJECT_NAME}_objs OBJECT
    src/frontend/frontend_handler.cpp
    src/user/profile/handlers/get_profile_handler.cpp
    src/user/profile/service/profile_service.cpp
    src/user/profile/repository/profile_repository.cpp
    src/education/repository/education_repository.cpp
    src/education/service/education_service.cpp
    src/education/handlers/get_university_admins_handler.cpp
    src/demo/greeting/greeting.cpp
    src/demo/handlers/hello/hello_handler.cpp
    src/demo/handlers/hello_postgres/hello_postgres_handler.cpp
    src/auth/handlers/current_user/current_user_handler.cpp
    src/auth/handlers/email_verification/email_verification_handler.cpp
    src/auth/handlers/login/login_handler.cpp
    src/auth/handlers/registration/registration_handler.cpp
    src/auth/handlers/resend_code/resend_code_handler.cpp
    src/auth/handlers/forgot_password/email_check/email_check.cpp
    src/auth/handlers/forgot_password/verify_code/verify_code.cpp
    src/auth/handlers/forgot_password/update_password/update_password.cpp
    src/auth/services/forgot_password/forgot_password_service.cpp
    src/auth/repositories/password_reset_codes/password_reset_repository.cpp
    src/auth/repositories/password_reset_tokens/password_reset_token_repository.cpp
    src/education/handlers/search_university_people_handler.cpp
    src/education/handlers/search_university_people_params_parser.cpp
    src/auth/middleware/jwt/jwt_auth_checker.cpp
    src/auth/repositories/user/user_repository.cpp
    src/auth/repositories/verification_code/verification_repository.cpp
    src/auth/services/email_verification/email_verification_service.cpp
    src/auth/services/jwt/jwt_service.cpp
    src/auth/repositories/session/session_repository.cpp
    src/auth/services/refresh_token/refresh_token_service.cpp
    src/auth/services/auth_session/auth_session_service.cpp
    src/auth/handlers/session/session_handlers.cpp
    src/auth/services/login/login_service.cpp
    src/auth/services/password/password_hasher.cpp
    src/auth/services/registration/registration_service.cpp
    src/auth/services/resend_code/resend_code_service.cpp
    src/auth/services/verification_code/verification_code_service.cpp
    src/email/postbox/postbox_client.cpp
    src/email/service/email_service.cpp
    src/email/signing/aws_signature_v4.cpp
)
target_include_directories(
    ${PROJECT_NAME}_objs
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src"
    PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated"
)
target_link_libraries(
    ${PROJECT_NAME}_objs
    PUBLIC userver::core #
           userver::postgresql
           OpenSSL::Crypto
           ${CRYPT_LIBRARY}
)


# The Service
add_executable(${PROJECT_NAME} src/main.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE ${PROJECT_NAME}_objs)

# Each public project header must compile without include-order dependencies.
file(GLOB_RECURSE PROJECT_HEADERS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp")
set(HEADER_CHECK_SOURCES)
foreach(HEADER IN LISTS PROJECT_HEADERS)
    file(RELATIVE_PATH HEADER_RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}/src" "${HEADER}")
    string(MAKE_C_IDENTIFIER "${HEADER_RELATIVE}" HEADER_ID)
    set(HEADER_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/header-check/${HEADER_ID}.cpp")
    file(WRITE "${HEADER_SOURCE}" "#include \"${HEADER_RELATIVE}\"\n")
    list(APPEND HEADER_CHECK_SOURCES "${HEADER_SOURCE}")
endforeach()
add_library(${PROJECT_NAME}_headers OBJECT EXCLUDE_FROM_ALL ${HEADER_CHECK_SOURCES})
target_link_libraries(${PROJECT_NAME}_headers PRIVATE ${PROJECT_NAME}_objs)

# Unit Tests
add_executable(
    ${PROJECT_NAME}_unittest
    tests/unit/demo/greeting_test.cpp
    tests/unit/auth/jwt_service_test.cpp
    tests/unit/auth/refresh_token_service_test.cpp
    tests/unit/auth/crypto_test.cpp
    tests/unit/education/admin_access_test.cpp
)
target_link_libraries(${PROJECT_NAME}_unittest PRIVATE ${PROJECT_NAME}_objs userver::utest)
add_dependencies(${PROJECT_NAME}_unittest ${PROJECT_NAME}_headers)
add_google_tests(${PROJECT_NAME}_unittest)

# Benchmarks
add_executable(
    ${PROJECT_NAME}_benchmark
    benchmarks/demo/greeting_benchmark.cpp
)
target_link_libraries(${PROJECT_NAME}_benchmark PRIVATE ${PROJECT_NAME}_objs userver::ubench)
add_google_benchmark_tests(${PROJECT_NAME}_benchmark)

# Functional testing
userver_testsuite_add_simple(PYTEST_ARGS -k "not console_codes")
userver_testsuite_add_simple(
    TEST_SUFFIX console
    PYTEST_ARGS -k console_codes
    TEST_ENV RUMPELQUIZ_TEST_CONSOLE=1
)
set_tests_properties(testsuite-RumpelQuiz testsuite-RumpelQuiz-console
    PROPERTIES RESOURCE_LOCK rumpelquiz-testsuite)

# Install
include(GNUInstallDirs)

if(DEFINED ENV{PREFIX})
    message(STATUS "Set install prefix: $ENV{PREFIX}")
    file(TO_CMAKE_PATH "$ENV{PREFIX}" PREFIX_PATH)
    set(CMAKE_INSTALL_PREFIX "${PREFIX_PATH}")
endif()

file(GLOB CONFIGS_FILES ${CMAKE_CURRENT_SOURCE_DIR}/configs/*.yaml ${CMAKE_CURRENT_SOURCE_DIR}/configs/*.json)

install(
    TARGETS ${PROJECT_NAME}
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT ${PROJECT_NAME}
)
install(
    FILES ${CONFIGS_FILES}
    DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/${PROJECT_NAME}
    COMPONENT ${PROJECT_NAME}
)
