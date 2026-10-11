# Реализованные HTTP-хендлеры RumpelQuiz

Состояние проекта на 9 октября 2026 года. Список составлен по исходникам,
регистрации компонентов в `src/main.cpp` и маршрутам в `configs/static_config.yaml`.
Здесь хендлеры — серверные HTTP-обработчики; обработчики событий интерфейса в JavaScript не входят в список.

Всего настроено 54 компонента с HTTP-маршрутами: 43 для API, 7 для фронтенда,
2 демонстрационных и 2 встроенных служебных. Один C++-класс может обслуживать
несколько компонентов и маршрутов. В таблицах указаны явные методы из конфигурации.
Наличие реализации подтверждено по коду; это не отчёт о запуске сервера или прохождении тестов.

## Авторизация

«JWT» означает заголовок `Authorization: Bearer <access_token>`.
Он обязателен для всех API-маршрутов ниже, кроме отмеченных как публичные.
Проверка токена реализована в `src/auth/middleware/jwt/jwt_auth_checker.cpp`;
дополнительные ограничения ролей и доступа проверяются соответствующими сервисами.
`/v1/auth/refresh` не требует access token, но требует `session_id` и `refresh_token`.

## Регистрация, вход и восстановление пароля — 10 маршрутов

| Метод | Маршрут | Компонент / класс | Доступ | Назначение и входные данные | Реализация |
| --- | --- | --- | --- | --- | --- |
| POST | `/v1/auth/register` | `handler-auth-register` / `RegisterHandler` | Публичный | Регистрация; JSON: `email`, `password`, `password_confirmation`. Возвращает `verification_id`. | `src/auth/handlers/registration/registration_handler.cpp` |
| POST | `/v1/auth/verify-email` | `handler-auth-verify-email` / `VerifyEmailHandler` | Публичный | Подтверждение почты; JSON: `verification_id`, `code`. При успехе создаёт сессию и возвращает токены. | `src/auth/handlers/email_verification/email_verification_handler.cpp` |
| POST | `/v1/auth/login` | `handler-auth-login` / `LoginHandler` | Публичный | Вход; JSON: `email`, `password`. Возвращает `access_token`, `refresh_token`, `session_id`, `token_type`. | `src/auth/handlers/login/login_handler.cpp` |
| POST | `/v1/auth/resend-code` | `handler-resend-code` / `ResendCodeHandler` | Публичный | Повторная отправка кода подтверждения; JSON: `verification_id`. | `src/auth/handlers/resend_code/resend_code_handler.cpp` |
| GET | `/v1/auth/me` | `handler-auth-current-user` / `CurrentUserHandler` | JWT | Возвращает `user_id` из контекста авторизации. | `src/auth/handlers/current_user/current_user_handler.cpp` |
| POST | `/v1/auth/refresh` | `handler-auth-refresh` / `RefreshHandler` | Refresh token | Обновление токенов; JSON: `session_id`, `refresh_token`. | `src/auth/handlers/session/session_handlers.cpp` |
| POST | `/v1/auth/logout` | `handler-auth-logout` / `LogoutHandler` | JWT | Отзывает текущую сессию из контекста авторизации. | `src/auth/handlers/session/session_handlers.cpp` |
| POST | `/v1/auth/forgot-password/email-check` | `handler-forgot-password-email-check` / `ForgotPasswordEmailHandler` | Публичный | Начало восстановления; JSON: `email`. Возвращает `verification_id`. | `src/auth/handlers/forgot_password/email_check/email_check.cpp` |
| POST | `/v1/auth/forgot-password/verify-code` | `handler-forgot-password-verify-code` / `ForgotPasswordVerifyHandler` | Публичный | Проверяет код восстановления; JSON: `verification_id`, `code`. Возвращает `reset_token`. | `src/auth/handlers/forgot_password/verify_code/verify_code.cpp` |
| POST | `/v1/auth/forgot-password/update-password` | `handler-forgot-password-update-password` / `ForgotPasswordUpdateHandler` | Reset token | Устанавливает новый пароль; JSON: `reset_token`, `password`, `password_confirmation`. | `src/auth/handlers/forgot_password/update_password/update_password.cpp` |

## Профиль и пользователи вуза — 3 маршрута

Все требуют JWT.

| Метод | Маршрут | Компонент / класс | Назначение | Реализация |
| --- | --- | --- | --- | --- |
| GET | `/v1/user/profile` | `handler-get-profile` / `GetProfileHandler` | Профиль текущего пользователя: почта, ФИО, аватар, роли и принадлежность к вузам/группам (`university_position`). | `src/user/profile/handlers/get_profile_handler.cpp` |
| GET | `/v1/education/universities/{university_id}/admins` | `handler-get-university-admins` / `GetUniversityAdminsHandler` | Список администраторов указанного вуза с проверкой доступа. | `src/education/handlers/get_university_admins_handler.cpp` |
| GET | `/v1/education/universities/{university_id}/people` | `handler-search-university-people` / `SearchUniversityPeopleHandler` | Поиск людей в вузе; обязательный query-параметр `q`, необязательные `limit`, `offset`. Возвращает `people`, `has_more`, `next_offset`. | `src/education/handlers/search_university_people_handler.cpp` |

## Создание, чтение и поиск квизов — 7 маршрутов

Все требуют JWT. Сохранение и чтение используют общие классы для двух маршрутов.

| Метод | Маршрут | Компонент / класс | Назначение | Реализация |
| --- | --- | --- | --- | --- |
| POST | `/v1/quizzes` | `handler-create-quiz` / `SaveQuizHandler` | Создание квиза; проверка и сохранение содержимого. Возвращает `quiz_id`, `status`, `revision`. | `src/quiz/authoring/handlers/save_quiz_handler.cpp` |
| PUT | `/v1/quizzes/{quiz_id}` | `handler-update-quiz` / `SaveQuizHandler` | Обновление квиза с проверкой версии и ограничений публикации. | `src/quiz/authoring/handlers/save_quiz_handler.cpp` |
| GET | `/v1/quizzes/{quiz_id}` | `handler-get-quiz` / `GetQuizHandler` | Получение содержимого доступного автору квиза в поле `quiz`. | `src/quiz/authoring/handlers/get_quiz_handler.cpp` |
| GET | `/v1/quizzes` | `handler-list-quizzes` / `GetQuizHandler` | Список квизов текущего автора в поле `quizzes`. | `src/quiz/authoring/handlers/get_quiz_handler.cpp` |
| GET | `/v1/quizzes/search` | `handler-search-quizzes` / `SearchQuizHandler` | Каталог квизов вузов активного преподавателя/администратора; обязательные `name`, `favourites`, `count_spend`. До 10 результатов в `quizzes`. | `src/catalog/handlers/search_quizzes_handler.cpp` |
| POST | `/v1/quizzes/{quiz_id}/favourite` | `handler-add-quiz-favourite` / `AddQuizFavouriteHandler` | Добавляет доступный в каталоге квиз в личное избранное. Тело не требуется. | `src/catalog/handlers/add_quiz_favourite_handler.cpp` |
| DELETE | `/v1/quizzes/{quiz_id}/favourite` | `handler-remove-quiz-favourite` / `RemoveQuizFavouriteHandler` | Удаляет только личную запись избранного квизов. Тело не требуется. | `src/catalog/handlers/remove_quiz_favourite_handler.cpp` |

## Игровые сессии — 8 маршрутов

Все требуют JWT. Управление сессией доступно ведущему;
доступ к состоянию, событиям и ответам дополнительно проверяется сервисом.

| Метод | Маршрут | Компонент / класс | Назначение и входные данные | Реализация |
| --- | --- | --- | --- | --- |
| POST | `/v1/game/sessions` | `handler-create-game-session` / `CreateGameSessionHandler` | Создание сессии квиза; JSON: `quiz_id`, `session_id`, `name`. | `src/game/handlers/create_game_session_handler.cpp` |
| GET | `/v1/game/sessions/{session_id}` | `handler-game-state` / `GamePlayHandler` | Получение текущего состояния игры. | `src/game/handlers/game_play_handler.cpp` |
| GET | `/v1/game/sessions/{session_id}/events` | `handler-game-events` / `GameEventsHandler` | Поток событий SSE для обновления состояния игры и присутствия участников. | `src/game/handlers/game_events_handler.cpp` |
| POST | `/v1/game/sessions/join` | `handler-game-join` / `GamePlayHandler` | Вход по шестизначному коду; JSON: `code`. Возвращает `session_id`. | `src/game/handlers/game_play_handler.cpp` |
| POST | `/v1/game/sessions/{session_id}/answers` | `handler-game-answer` / `GamePlayHandler` | Отправка ответа; JSON: `question_id`, `answer_ids`. | `src/game/handlers/game_play_handler.cpp` |
| POST | `/v1/game/sessions/{session_id}/presence` | `handler-game-presence` / `GamePlayHandler` | Обновление присутствия; JSON: `client_id`, `sequence`, `online`. | `src/game/handlers/game_play_handler.cpp` |
| POST | `/v1/game/sessions/{session_id}/next` | `handler-next-game-question` / `NextGameQuestionHandler` | Начало игры или переход к следующему вопросу с проверкой ожидаемого состояния. | `src/game/handlers/next_game_question_handler.cpp` |
| POST | `/v1/game/sessions/{session_id}/close` | `handler-close-game-session` / `CloseGameSessionHandler` | Закрытие сессии ведущим. | `src/game/handlers/close_game_session_handler.cpp` |

## История квизов и результаты сессий — 2 маршрута

Все требуют JWT.

| Метод | Маршрут | Компонент / класс | Назначение | Реализация |
| --- | --- | --- | --- | --- |
| GET | `/v1/quizzes/{quiz_id}/sessions` | `handler-list-quiz-sessions` / `ListQuizSessionsHandler` | История завершённых и отменённых сессий квиза в поле `sessions`. | `src/quiz/info/handlers/list_quiz_sessions_handler.cpp` |
| GET | `/v1/game/sessions/{session_id}/results` | `handler-get-session-results` / `GetSessionResultsHandler` | Результаты завершённой сессии в поле `results`; незавершённая сессия возвращает конфликт. | `src/quiz/info/handlers/get_session_results_handler.cpp` |

## Создание, поиск и прохождение тестов — 12 маршрутов

Все требуют JWT. Авторские операции проверяют права автора;
прохождение и доступный список проверяют роль и принадлежность студента к вузу.

| Метод | Маршрут | Компонент / класс | Назначение | Реализация |
| --- | --- | --- | --- | --- |
| POST | `/v1/tests` | `handler-create-test` / `SaveTestHandler` | Создание черновика или опубликованного теста; возвращает `test_id`, `status`, `revision`. | `src/test/handlers/save_test_handler.cpp` |
| PUT | `/v1/tests/{test_id}` | `handler-update-test` / `SaveTestHandler` | Обновление черновика с проверкой `revision` и ограничений публикации. | `src/test/handlers/save_test_handler.cpp` |
| GET | `/v1/tests/{test_id}` | `handler-get-test` / `GetTestHandler` | Полное содержимое теста для автора в поле `test`. | `src/test/handlers/get_test_handler.cpp` |
| GET | `/v1/tests` | `handler-list-tests` / `GetTestHandler` | Список тестов автора в поле `tests`. | `src/test/handlers/get_test_handler.cpp` |
| GET | `/v1/tests/search` | `handler-search-tests` / `SearchTestHandler` | Каталог тестов вузов активного преподавателя/администратора; обязательные `name`, `favourites`, `count_spend`. До 10 результатов в `tests`. | `src/catalog/handlers/search_tests_handler.cpp` |
| POST | `/v1/tests/{test_id}/favourite` | `handler-add-test-favourite` / `AddTestFavouriteHandler` | Добавляет доступный в каталоге тест в избранное текущего пользователя. Тело не требуется. | `src/catalog/handlers/add_test_favourite_handler.cpp` |
| DELETE | `/v1/tests/{test_id}/favourite` | `handler-remove-test-favourite` / `RemoveTestFavouriteHandler` | Удаляет только личную запись избранного. Тело не требуется. | `src/catalog/handlers/remove_test_favourite_handler.cpp` |
| GET | `/v1/tests/available` | `handler-available-tests` / `TestPlayHandler` | Доступные студенту публичные тесты и его попытки. | `src/test/handlers/test_play_handler.cpp` |
| POST | `/v1/tests/{test_id}/start` | `handler-test-start` / `TestPlayHandler` | Начать или продолжить попытку; тело `{}`. | `src/test/handlers/test_play_handler.cpp` |
| GET | `/v1/tests/{test_id}/progress` | `handler-test-progress` / `TestPlayHandler` | Восстановить сохранённое состояние прохождения. | `src/test/handlers/test_play_handler.cpp` |
| POST | `/v1/tests/{test_id}/answers` | `handler-test-answer` / `TestPlayHandler` | Сохранить ответ; JSON: `question_id`, `answer_ids`. Возвращает следующий вопрос или итог попытки. | `src/test/handlers/test_play_handler.cpp` |
| GET | `/v1/tests/{test_id}/results` | `handler-test-results` / `TestPlayHandler` | Отчёт автора о прохождениях студентами. | `src/test/handlers/test_play_handler.cpp` |

## Загрузка изображений — 1 маршрут

| Метод | Маршрут | Компонент / класс | Доступ | Назначение | Реализация |
| --- | --- | --- | --- | --- | --- |
| POST | `/v1/media/images` | `handler-upload-image` / `UploadImageHandler` | JWT и права на загрузку | `multipart/form-data`, ровно один непустой файл в поле `file`, максимум 5 МиБ. Проверяет изображение, сохраняет через сервис медиа; возвращает `media_id`, `image_url` с HTTP 201. | `src/media/handlers/upload_image_handler.cpp` |

## Фронтенд — 7 маршрутов

Все публичные, обслуживаются классом `FrontendHandler`.
Реализация: `src/frontend/frontend_handler.cpp`. Ресурсы встроены в приложение.

| Метод | Маршрут | Компонент | Назначение |
| --- | --- | --- | --- |
| GET | `/` | `handler-frontend` | HTML приложения |
| GET | `/styles.css` | `handler-frontend-css` | Стили |
| GET | `/app.js` | `handler-frontend-js` | Основной JavaScript приложения |
| GET | `/quiz.js` | `handler-frontend-quiz-js` | Интерфейс квизов |
| GET | `/game.js` | `handler-frontend-game-js` | Интерфейс игровой сессии |
| GET | `/test.js` | `handler-frontend-test-js` | Интерфейс тестов |
| GET | `/vendor/qrcode.js` | `handler-frontend-qr-js` | Библиотека QR-кодов |
| GET | `/assets/*` | `handler-frontend-assets` | Встроенные ES-модули и CSS по разделам; отсутствующие ресурсы возвращают 404 |

## Демонстрационные и служебные — 4 маршрута

Авторизация JWT в конфигурации не задана.

| Метод | Маршрут | Компонент / класс | Назначение | Реализация |
| --- | --- | --- | --- | --- |
| GET, POST | `/hello` | `handler-hello` / `Hello` | Приветствие; принимает query-параметр `name`. | `src/demo/handlers/hello/hello_handler.cpp` |
| GET, POST | `/hello-postgres` | `handler-hello-postgres` / `HelloPostgres` | Приветствие с обращением к PostgreSQL; принимает query-параметр `name`. | `src/demo/handlers/hello_postgres/hello_postgres_handler.cpp` |
| GET | `/ping` | `handler-ping` / `userver::server::handlers::Ping` | Проверка доступности сервиса. | Встроенный хендлер userver; подключён в `src/main.cpp`. |
| POST | `/tests/{action}` | `tests-control` / `userver::server::handlers::TestsControl` | Управление testsuite; загрузка зависит от `$is-testing`. | Встроенный хендлер userver; подключён в `src/main.cpp`. |

## Файлы-заготовки без реализации

Эти файлы существуют, но не содержат реализованных HTTP-хендлеров и не зарегистрированы в приложении:

| Файлы | Состояние | Где реализована соответствующая операция |
| --- | --- | --- |
| `src/user/profile/handlers/update_profile_handler.cpp`, `.hpp` | `.cpp` пуст; `.hpp` содержит только директиву и include, класса нет. | HTTP-маршрут обновления профиля не зарегистрирован. |
| `src/quiz/authoring/handlers/update_quiz_handler.cpp`, `.hpp` | Оба файла пусты. | Обновление квиза реализовано в `SaveQuizHandler`, `PUT /v1/quizzes/{quiz_id}`. |
| `src/quiz/authoring/handlers/list_my_quizzes_handler.cpp`, `.hpp` | Оба файла пусты. | Список квизов реализован в `GetQuizHandler`, `GET /v1/quizzes`. |

Парсеры, сериализаторы и вспомогательные функции в каталогах `handlers`
не являются отдельными HTTP-хендлерами: например, `game_handler_utils.hpp`,
`search_university_people_params_parser.cpp`, `quiz_json.cpp`, `test_json.cpp`.

## Где проверять и обновлять список

- `configs/static_config.yaml` — имя компонента, URL, методы, JWT и лимиты запросов.
- `src/main.cpp` — фактическая регистрация класса и его экземпляров.
- Соответствующий `.cpp` — обработка запроса и вызовы сервиса.
- `tests/test_*.py` — существующие API-тесты; `tests/test_frontend.py` — выдача ресурсов интерфейса.

При добавлении маршрута обновляйте таблицу соответствующего раздела и общий счётчик.
