# JWT и пользовательские сессии

## Реализация

Login и успешное подтверждение email создают сессию и возвращают
`access_token`, `refresh_token`, `session_id`, `token_type`, `success`.
Access JWT живёт 900 секунд и содержит `user_id`, `session_id`, `exp`, `iat`,
`sub`. В БД JWT не сохраняется. JwtAuthChecker проверяет подпись и claims,
сохраняет user_id и session_id в RequestContext.

`POST /v1/auth/refresh` принимает session_id и refresh_token без access JWT.
Сервис блокирует строку сессии через SELECT FOR UPDATE, проверяет абсолютный
срок действия и hash, заменяет hash, генерирует JWT и делает commit перед
возвратом ответа. Два одновременных запроса со старым токеном имеют ровно
одного победителя. Срок сессии — 180 дней от создания; rotation его не меняет.

RefreshTokenService использует OpenSSL RAND_bytes (256 случайных бит),
hex-кодирование, SHA-256 и CRYPTO_memcmp. Хранится только hash.
AuthSessionService управляет транзакциями, SessionRepository выполняет SQL;
использованы существующие component/service/repository/handler conventions.

`POST /v1/auth/logout` удаляет сессию из проверенного JWT.
RevokeAllSessions(user_id) доступен на уровне сервиса без нового HTTP endpoint.
Access JWT после logout действует до exp; refresh удалённой сессии отклоняется.

## Файлы

Новые файлы:

- `src/auth/services/refresh_token/refresh_token_service.hpp`
- `src/auth/services/refresh_token/refresh_token_service.cpp`
- `src/auth/services/auth_session/auth_session_service.hpp`
- `src/auth/services/auth_session/auth_session_service.cpp`
- `src/auth/handlers/session/session_handlers.hpp`
- `src/auth/handlers/session/session_handlers.cpp`
- `postgresql/migrations/001_auth_sessions.sql`
- `tests/unit/auth/refresh_token_service_test.cpp`
- `tests/test_sessions.py`
- `docs/auth-session-changes.md`

Изменённые файлы:

- `src/auth/repositories/session/session_repository.hpp`
- `src/auth/repositories/session/session_repository.cpp`
- `src/auth/services/jwt/jwt_service.hpp`
- `src/auth/services/jwt/jwt_service.cpp`
- `src/auth/middleware/jwt/auth_context.hpp`
- `src/auth/middleware/jwt/jwt_auth_checker.cpp`
- `src/auth/handlers/login/login_handler.hpp`
- `src/auth/handlers/login/login_handler.cpp`
- `src/auth/handlers/email_verification/email_verification_handler.hpp`
- `src/auth/handlers/email_verification/email_verification_handler.cpp`
- `src/main.cpp`
- `CMakeLists.txt`
- `configs/static_config.yaml`
- `configs/config_vars.yaml`
- `configs/config_vars.testing.yaml`
- `tests/unit/auth/jwt_service_test.cpp`
- `tests/test_jwt_auth.py`
- `tests/test_auth_edges.py`
- `README.md`

SessionRepository уже присутствовал в незакоммиченных файлах до начала работы:
его API доработан, UpdateRefreshToken больше не меняет expires_at, добавлены
блокировка строки и необходимый PostgreSQL UUID include. Изменения таблицы
auth.sessions в `postgresql/schemas/db_1.sql` также существовали до начала
работы и сохранены. Существующее поле last_used_at не используется новым flow.

## Проверки

- Debug-сборка в штатном userver v3.1 dev-контейнере прошла:
  `cmake --build build-debug -j 4`.
- Все публичные заголовки прошли самостоятельную компиляцию.
- 10 unit-тестов прошли, включая JWT claims и refresh token/hash.
- CTest benchmark прошёл.
- Оба интеграционных набора `testsuite-RumpelQuiz` и
  `testsuite-RumpelQuiz-console` прошли под пользователем user.
- Новые проверки покрывают создание сессии, claims и TTL, правильный/неправильный
  refresh, rotation и replay, фиксированный expires_at, истёкшую/отсутствующую
  сессию, logout, независимые сессии, два конкурентных refresh, неверные JSON
  данные и отсутствие токенов/хешей в захваченных логах.
- `git diff --check` прошёл.

Первый запуск testsuite под root был отклонён самим framework; повторный
запуск под обычным пользователем прошёл. В существующем каталоге сборки
артефакты принадлежат root, поэтому сборка и testsuite запускались раздельно.

## Найденное в прежней JWT-реализации

- У login и verify-email не было запрета логирования тела ответа с JWT.
  Добавлен response_data_size_log_limit: 0; аналогично защищён refresh.
- JWT содержал только sub/iat/exp, без привязки к сессии. Добавлены обязательные
  user_id и session_id, проверка UUID и согласованности sub/user_id.
- Access TTL по умолчанию и в config vars был 3600 секунд; изменён на 900.
- Проверки HS256, подписи через constant-time comparison и временных claims
  сохранены; дополнительных ошибок в них в рамках этой работы не обнаружено.

## Применение

Контракт API и команда применения миграции описаны в README.
Для существующей БД перед запуском новой версии нужен
`postgresql/migrations/001_auth_sessions.sql`; к постоянной БД миграция в этой
работе не применялась. Старые JWT без новых claims требуют повторного входа.
Frontend не менялся: для автоматического refresh и серверного logout клиенту
нужно использовать новые endpoints и сохранять обновлённую пару токенов.
