# Создание пользователя администратором

`POST /v1/user/create` требует Bearer JWT. Обязательные JSON-поля:
`email`, `password`, `first_name`, `last_name`, `university` (UUID вуза),
`role` (`student`, `teacher`, `admin`). Необязательные поля: `middle_name`,
`description`, `facultet_id` (UUID факультета); допускаются отсутствие и `null`.
Название `facultet_id` сохранено в API.

Администратор вуза может создать любую из трёх ролей в своём вузе.
`facultet_id` допустим только для `admin` и должен принадлежать указанному вузу.
С ним новый администратор получает `admin_scope=faculties` и назначение на
этот факультет; без него — `admin_scope=university`.
Администратор факультета может создать администратора только своего факультета.
Создание студента или преподавателя без привязки к факультету требует прав
администратора всего вуза. Неактивные роли доступа не дают.

Успех: 201 с `{"success":true,"user_id":"UUID"}`. Пользователь сразу имеет
подтверждённый email и может войти с переданным паролем. Учётная запись,
профиль, роль и назначение факультета сохраняются одной транзакцией.
Существующий email, включая неподтверждённую учётную запись, возвращает 409
`email_already_exists` без изменения её данных. Ошибки полей — 400
`invalid_request`, факультета — 400 `invalid_faculty`, доступа — 403
`university_access_denied`, отсутствующего вуза — 404 `university_not_found`.

`GET /v1/user/profile` теперь возвращает `description`: строку или `null`.
Перед запуском обновлённого сервиса на существующей БД примените миграцию:

```powershell
Get-Content postgresql/migrations/010_profile_description.sql -Raw | docker compose exec -T postgres psql -U rumpelquiz -d rumpelquiz -v ON_ERROR_STOP=1
```

Новая БД получает колонку из основной схемы. Миграцию можно применять повторно.
Проверки создания, прав, дубликатов, отката и миграции находятся в
`tests/test_user_creation.py`; проверки чтения описания — в `tests/test_profile.py`.
