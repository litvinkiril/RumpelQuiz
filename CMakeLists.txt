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

# Common sources
include_directories(src)

add_library(
    ${PROJECT_NAME}_objs OBJECT
    src/demo/greeting/greeting.cpp
    src/demo/handlers/hello/hello_handler.cpp
    src/demo/handlers/hello_postgres/hello_postgres_handler.cpp
    src/auth/handlers/current_user/current_user_handler.cpp
    src/auth/handlers/email_verification/email_verification_handler.cpp
    src/auth/handlers/login/login_handler.cpp
    src/auth/handlers/registration/registration_handler.cpp
    src/auth/handlers/resend_code/resend_code_handler.cpp
    src/auth/middleware/jwt/jwt_auth_checker.cpp
    src/auth/repositories/user/user_repository.cpp
    src/auth/repositories/verification_code/verification_repository.cpp
    src/auth/services/email_verification/email_verification_service.cpp
    src/auth/services/jwt/jwt_service.cpp
    src/auth/services/login/login_service.cpp
    src/auth/services/password/password_hasher.cpp
    src/auth/services/registration/registration_service.cpp
    src/auth/services/resend_code/resend_code_service.cpp
    src/auth/services/verification_code/verification_code_service.cpp
    src/email/postbox/postbox_client.cpp
    src/email/service/email_service.cpp
    src/email/signing/aws_signature_v4.cpp
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

# Unit Tests
add_executable(
    ${PROJECT_NAME}_unittest
    tests/unit/demo/greeting_test.cpp
    tests/unit/auth/jwt_service_test.cpp
)
target_link_libraries(${PROJECT_NAME}_unittest PRIVATE ${PROJECT_NAME}_objs userver::utest)
add_google_tests(${PROJECT_NAME}_unittest)

# Benchmarks
add_executable(
    ${PROJECT_NAME}_benchmark
    benchmarks/demo/greeting_benchmark.cpp
)
target_link_libraries(${PROJECT_NAME}_benchmark PRIVATE ${PROJECT_NAME}_objs userver::ubench)
add_google_benchmark_tests(${PROJECT_NAME}_benchmark)

# Functional testing
userver_testsuite_add_simple()

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
