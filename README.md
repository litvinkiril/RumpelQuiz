# RumpelQuiz

Template of a C++ service that uses [userver framework](https://github.com/userver-framework/userver).


## Local development PostgreSQL

The persistent development database is separate from the temporary PostgreSQL
instance created by testsuite. Docker Compose stores its data in the named
volume `rumpelquiz-postgres-data` and initializes it from the existing files in
`postgresql/schemas` and `postgresql/data` on the first start.

Port `5433` is used on the Windows host because the standard port `5432` is
already occupied. Start the database from PowerShell in the project directory:

```console
docker compose up -d --wait postgres
docker compose ps
```

Open the Dev Container and start the backend there:

```console
make build-debug
make start-debug
```

`make start-debug` starts the binary directly with `configs/config_vars.yaml`.
The PostgreSQL connection can be overridden without changing C++ code:

```console
DB_CONNECTION='postgresql://rumpelquiz:rumpelquiz_dev@host.docker.internal:5433/rumpelquiz' make start-debug
```

The default development connection is:

| Setting | Value |
| --- | --- |
| Host from Windows / pgAdmin | `localhost` |
| Host from the Dev Container | `host.docker.internal` |
| Port | `5433` |
| Database | `rumpelquiz` |
| Username | `rumpelquiz` |
| Password | `rumpelquiz_dev` |

Connection checks:

```console
docker compose exec postgres pg_isready -U rumpelquiz -d rumpelquiz
docker compose exec postgres psql -U rumpelquiz -d rumpelquiz -c "SELECT current_database(), current_user;"
curl http://localhost:8080/ping
curl http://localhost:8080/hello
curl http://localhost:8080/hello-postgres
curl 'http://localhost:8080/hello-postgres?name=DevCheck'
```

Stop PostgreSQL without deleting development data:

```console
docker compose down
```

To intentionally reset the development database, run `docker compose down -v`
and start it again. This deletes the development volume; it does not affect the
separate testsuite database.


## Download and Build

To create your own userver-based service follow the following steps:

1. Press the "Use this template button" at the top right of this GitHub page
2. Clone the service `git clone your-service-repo && cd your-service-repo && git submodule update --init`
3. Give a proper name to your service and replace all the occurrences of "RumpelQuiz" string with that name
4. Feel free to tweak, adjust or fully rewrite the source code of your service.


## Makefile

`PRESET` is either `debug`, `release`, or if you've added custom presets in `CMakeUserPresets.json`, it
can also be `debug-custom`, `release-custom`.

* `make cmake-PRESET` - run cmake configure, update cmake options and source file lists
* `make build-PRESET` - build the service
* `make test-PRESET` - build the service and run all tests
* `make start-PRESET` - build the service, start it in testsuite environment and leave it running
* `make install-PRESET` - build the service and install it in directory set in environment `PREFIX`
* `make` or `make all` - build and run all tests in `debug` and `release` modes
* `make format` - reformat all C++ and Python sources
* `make dist-clean` - clean build files and cmake cache
* `make docker-COMMAND` - run `make COMMAND` in docker environment
* `make docker-clean-data` - stop docker containers


## License

The original template is distributed under the [Apache-2.0 License](https://github.com/userver-framework/userver/blob/v3.1/LICENSE)
and [CLA](https://github.com/userver-framework/userver/blob/v3.1/CONTRIBUTING.md). Services based on the template may change
the license and CLA.

## Authentication

All requests below use JSON. Successful login and email verification create a
session and return `access_token`, `refresh_token`, `session_id`,
`token_type: Bearer`, and `success: true`. Send the access token as
`Authorization: Bearer <access_token>` to `GET /v1/auth/me`.

| POST endpoint | Request fields |
| --- | --- |
| /v1/auth/register | email, password, password_confirmation |
| /v1/auth/verify-email | verification_id, code |
| /v1/auth/resend-code | verification_id |
| /v1/auth/login | email, password |
| /v1/auth/refresh | session_id, refresh_token |
| /v1/auth/logout | empty JSON object; Authorization: Bearer access token |
| /v1/auth/forgot-password/email-check | email |
| /v1/auth/forgot-password/verify-code | verification_id, code |
| /v1/auth/forgot-password/update-password | reset_token, password, password_confirmation |

Access JWTs use HS256 and contain `user_id`, `session_id`, `exp`, `iat`, and
`sub` (equal to `user_id`). The default access lifetime is 900 seconds; the
absolute session lifetime is 15,552,000 seconds (180 days). Configure these with
`jwt-access-token-ttl-seconds` and `session-lifetime-seconds` in config vars.

Refresh requires no access JWT. It returns the same response fields as login.
Clients must replace the saved refresh token after each successful refresh and
serialize refresh requests for a session. Refresh tokens contain 256 random bits
from OpenSSL, encoded as 64 hexadecimal characters. Only their SHA-256 hashes
are stored in PostgreSQL. `SELECT ... FOR UPDATE` locks the session until the
hash rotation and JWT generation commit; concurrent reuse has one winner.
Rotation never extends `expires_at`. Invalid, expired, revoked sessions and
wrong/reused tokens return `401` with `error: invalid_session`; malformed
request fields return `400` with `error: invalid_request`.

Logout deletes the session identified by the authenticated JWT and returns
`success: true`; repeated logout with a still-valid JWT is harmless.
`AuthSessionService::RevokeAllSessions(user_id)` supports future logout from all
devices. Access JWTs remain valid until their own expiration after logout,
because JWT validation does not query the session table. Tokens from the old
backend without the required new claims are rejected; users must log in again.
The bundled frontend currently uses only access tokens; automatic refresh and
server logout must be integrated by clients that need persistent sessions.

Fresh databases include `auth.sessions` in the schema. Before deploying against
an existing database, apply `postgresql/migrations/001_auth_sessions.sql` with
`psql -v ON_ERROR_STOP=1 -f postgresql/migrations/001_auth_sessions.sql` using
the target database connection. The script preserves existing sessions and data.
The pre-existing `last_used_at` column is retained for compatibility and is not
updated by this flow. Request/response body logging is disabled for token
endpoints, and token responses use `Cache-Control: no-store`.

Password recovery requires a verified email. The email-check response contains
verification_id. Submit it with the six-digit email code to verify-code to get
reset_token, then pass that token to update-password. After success, log in
with the new password. Codes and reset tokens are single-use; their default
lifetime is 900 seconds and can be changed in configs/config_vars.yaml.
Requesting another code has a 60-second cooldown; reset codes allow five
incorrect attempts. Passwords must contain 1–72 UTF-8 bytes and no NUL byte.
Reset tokens are stored as hashes. Previously issued JWT access tokens remain
valid until their normal expiry.

For an existing development database, apply the additive, repeatable migration
without recreating the Docker volume (PowerShell):

    Get-Content postgresql/migrations/001_password_reset.sql -Raw | docker compose exec -T postgres psql -U rumpelquiz -d rumpelquiz -v ON_ERROR_STOP=1

New databases get the reset tables from postgresql/schemas/db_1.sql.
Set JWT_SECRET (at least 32 characters) in .env before make start-debug. Functional tests use a mock email provider and
a separate temporary database:

    make test-debug

## Frontend and local console codes

Start the backend in the Dev Container:

    make start-debug

Open http://localhost:8080/ (VS Code forwards port 8080). The backend serves
the frontend itself; no npm install, frontend build, CORS setup, or additional
web server is required. Changes in frontend/ are embedded on the next CMake
build. The UI includes registration, email confirmation, code resend, login,
session verification, logout and all three password-recovery steps.

The local config already uses:

    email-send-enabled: false
    email-log-codes: true

Postbox credentials are optional in this mode. Look in the same terminal where
make start-debug is running; registration, resend and recovery print:

    [DEV AUTH CODE] purpose=verify-email email=you@example.com code=123456 ttl_seconds=900
    [DEV AUTH CODE] purpose=reset-password email=you@example.com code=654321 ttl_seconds=900

These are example codes. Enter the actual fresh code from your terminal.
Codes are not included in HTTP responses. To enable real email later, set
email-send-enabled: true and email-log-codes: false and configure the
YANDEX_POSTBOX_* variables. Plaintext code logging is opt-in and defaults to
false in the component and integration-test configuration.

A login session is scoped to the browser tab using sessionStorage. Passwords,
email codes and password-reset tokens are never saved in browser storage.
Reloading the page restores a pending email-code step or verifies an existing
session; reloading at the new-password step requires restarting recovery.

## Test commands

    make test-debug
    make test-ui

The second command requires Node.js 22+ and installs no packages.
On Windows it can also be run as:

    node --test frontend/tests/auth.test.js

The backend command runs unit tests, benchmarks, the API suite with mocked
Postbox, and a separate console-mode suite with Postbox and credentials disabled.
The suites use the temporary testsuite database and are serialized even when
ctest runs with parallel workers.

Coverage includes registration/re-registration, duplicate email, login,
verification and reset code expiry/replay/concurrency, resend cooldown, reset
token rotation/tampering/expiry, invalid JSON/fields/passwords, JWT signature
and claim validation, password hashing, code generation, mail delivery errors,
console-only flows and static frontend responses. Frontend tests exercise the
complete auth state transitions, session restore/logout, timers, duplicate
submissions, stale responses, Unicode password limits, network errors and
HTML escaping. Both backend and frontend suites are wired into Docker CI.

### C++ includes and editor setup

Use **Dev Containers: Reopen in Container** in VS Code. The compilation database
contains Linux paths to userver, OpenSSL and libcrypt; a Windows clangd process
cannot resolve those installed container headers.

CMake exports compile_commands.json; .clangd points to build-debug. The Dev
Container automatically configures CMake when the project opens or CMake files
change. After editing frontend assets, build again to regenerate the embedded
frontend_assets.hpp in build-debug/generated; this file is generated, not a
missing source header.

The RumpelQuiz_headers build target compiles every project header in a separate
translation unit. It is also a dependency of the unit-test target, so
make test-debug checks header self-containment automatically.

For a clean build independent of existing object files:

    cmake --preset debug -B build-auth-check
    CCACHE_DISABLE=1 cmake --build build-auth-check -j 4
    ctest --test-dir build-auth-check --output-on-failure
