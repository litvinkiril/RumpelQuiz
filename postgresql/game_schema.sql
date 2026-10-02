-- Игровые сессии. Выполнять после схем auth, education, quiz
-- (включая миграцию 005_quizzes_media.sql).
-- Существующие данные не удаляются. Повторный запуск допустим.
BEGIN;
SET LOCAL lock_timeout = '5s';
SELECT pg_advisory_xact_lock(60421, 6);

CREATE SCHEMA IF NOT EXISTS game;

-- Составные ключи нужны для проверки принадлежности квиза вузу,
-- вопроса квизу и варианта вопросу через внешние ключи.
CREATE UNIQUE INDEX IF NOT EXISTS quizzes_id_university_game_key
    ON quiz.quizzes (id, university_id);
CREATE UNIQUE INDEX IF NOT EXISTS questions_id_quiz_game_key
    ON quiz.questions (id, quiz_id);
CREATE UNIQUE INDEX IF NOT EXISTS answers_id_question_game_key
    ON quiz.answers (id, question_id);

CREATE TABLE IF NOT EXISTS game.sessions (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    quiz_id UUID NOT NULL,
    host_user_id UUID NOT NULL REFERENCES auth.users(id) ON DELETE RESTRICT,
    university_id UUID NOT NULL,
    -- Код генерирует сервер; ведущие нули сохраняются.
    join_code TEXT NOT NULL CHECK (join_code ~ '^[0-9]{6}$'),
    status TEXT NOT NULL DEFAULT 'waiting'
        CHECK (status IN ('waiting', 'running', 'finished', 'cancelled')),
    created_at TIMESTAMPTZ NOT NULL DEFAULT clock_timestamp(),
    started_at TIMESTAMPTZ,
    ended_at TIMESTAMPTZ,
    UNIQUE (id, quiz_id),
    FOREIGN KEY (quiz_id, university_id)
        REFERENCES quiz.quizzes(id, university_id) ON DELETE RESTRICT,
    CHECK (
        (status = 'waiting' AND started_at IS NULL AND ended_at IS NULL)
        OR (status = 'running' AND started_at IS NOT NULL AND ended_at IS NULL)
        OR (status = 'finished' AND started_at IS NOT NULL AND ended_at IS NOT NULL)
        OR (status = 'cancelled' AND ended_at IS NOT NULL)
    ),
    CHECK (started_at IS NULL OR started_at >= created_at),
    CHECK (ended_at IS NULL OR ended_at >= COALESCE(started_at, created_at))
);

-- После завершения код можно использовать повторно.
-- История и повторное подключение адресуются UUID сессии, а не старым кодом.
CREATE UNIQUE INDEX IF NOT EXISTS sessions_active_join_code_key
    ON game.sessions(join_code) WHERE status IN ('waiting', 'running');
CREATE INDEX IF NOT EXISTS sessions_host_created_idx
    ON game.sessions(host_user_id, created_at DESC);
CREATE INDEX IF NOT EXISTS sessions_quiz_idx ON game.sessions(quiz_id);

CREATE TABLE IF NOT EXISTS game.participants (
    session_id UUID NOT NULL REFERENCES game.sessions(id) ON DELETE RESTRICT,
    user_id UUID NOT NULL REFERENCES auth.users(id) ON DELETE RESTRICT,
    joined_at TIMESTAMPTZ NOT NULL DEFAULT clock_timestamp(),
    PRIMARY KEY (session_id, user_id)
);
CREATE INDEX IF NOT EXISTS participants_user_idx ON game.participants(user_id);

-- Строка создаётся при открытии вопроса. Для будущих вопросов строк ещё нет.
-- Одна сессия проходит каждый вопрос максимум один раз.
CREATE TABLE IF NOT EXISTS game.session_questions (
    session_id UUID NOT NULL,
    question_id UUID NOT NULL,
    quiz_id UUID NOT NULL,
    opened_at TIMESTAMPTZ NOT NULL DEFAULT clock_timestamp(),
    deadline_at TIMESTAMPTZ NOT NULL,
    closed_at TIMESTAMPTZ,
    PRIMARY KEY (session_id, question_id),
    FOREIGN KEY (session_id, quiz_id)
        REFERENCES game.sessions(id, quiz_id) ON DELETE RESTRICT,
    FOREIGN KEY (question_id, quiz_id)
        REFERENCES quiz.questions(id, quiz_id) ON DELETE RESTRICT,
    CHECK (deadline_at > opened_at),
    CHECK (closed_at IS NULL OR closed_at >= opened_at)
);
CREATE UNIQUE INDEX IF NOT EXISTS session_questions_one_open_key
    ON game.session_questions(session_id) WHERE closed_at IS NULL;
CREATE INDEX IF NOT EXISTS session_questions_question_idx
    ON game.session_questions(question_id, quiz_id);

-- Один окончательный ответ участника на вопрос, независимо от числа вариантов.
-- Отсутствие строки означает, что ответ не был принят (включая пропуск).
CREATE TABLE IF NOT EXISTS game.submissions (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    session_id UUID NOT NULL,
    user_id UUID NOT NULL,
    question_id UUID NOT NULL,
    submitted_at TIMESTAMPTZ NOT NULL DEFAULT clock_timestamp(),
    UNIQUE (session_id, user_id, question_id),
    UNIQUE (id, question_id),
    FOREIGN KEY (session_id, user_id)
        REFERENCES game.participants(session_id, user_id) ON DELETE RESTRICT,
    FOREIGN KEY (session_id, question_id)
        REFERENCES game.session_questions(session_id, question_id) ON DELETE RESTRICT
);
CREATE INDEX IF NOT EXISTS submissions_session_question_idx
    ON game.submissions(session_id, question_id);

-- Для single одна строка; для multy несколько. Баллы здесь не хранятся.
CREATE TABLE IF NOT EXISTS game.submission_choices (
    submission_id UUID NOT NULL,
    question_id UUID NOT NULL,
    answer_id UUID NOT NULL,
    PRIMARY KEY (submission_id, answer_id),
    FOREIGN KEY (submission_id, question_id)
        REFERENCES game.submissions(id, question_id) ON DELETE RESTRICT,
    FOREIGN KEY (answer_id, question_id)
        REFERENCES quiz.answers(id, question_id) ON DELETE RESTRICT
);
CREATE INDEX IF NOT EXISTS submission_choices_answer_idx
    ON game.submission_choices(answer_id, question_id);

-- Правила обработчиков (одних CREATE TABLE для них недостаточно):
-- 1. Запускать только ready-квиз. Проверять активную роль ведущего в вузе.
-- 2. При входе проверять активную роль student в university_id сессии.
--    INSERT участника ON CONFLICT DO NOTHING; при отключении не удалять.
-- 3. user_id брать из авторизации, времена вычислять на сервере.
-- 4. При ответе проверять running, closed_at IS NULL и текущее время < deadline_at;
--    проверку и запись выполнять в транзакции с блокировкой, согласованной
--    с закрытием вопроса/сессии. Время проверять после получения блокировки.
-- 5. Записывать submission и все choices в одной транзакции; проверять
--    непустой набор, отсутствие дублей и ровно один вариант для single.
-- 6. Повтор того же ответа подтверждать без изменения submitted_at;
--    попытку заменить уже сохранённый ответ отклонять.
-- 7. При завершении закрывать открытый вопрос в той же транзакции.
--    Результаты выдавать только для finished, с проверкой прав просмотра.
-- 8. Не предоставлять удаление участников или изменение принятых ответов.
COMMIT;
