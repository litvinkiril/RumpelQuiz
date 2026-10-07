CREATE EXTENSION IF NOT EXISTS pgcrypto;

CREATE SCHEMA IF NOT EXISTS auth;

CREATE TABLE IF NOT EXISTS auth.users (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    email TEXT NOT NULL UNIQUE,
    password_hash TEXT NOT NULL,
    email_verified BOOLEAN NOT NULL DEFAULT FALSE,
    login_attempts INTEGER NOT NULL DEFAULT 0,
    login_window_started_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE TABLE auth.verification_codes (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),

    user_id UUID NOT NULL UNIQUE
        REFERENCES auth.users(id)
        ON DELETE CASCADE,

    code_hash TEXT NOT NULL,
    attempts INTEGER NOT NULL DEFAULT 0,
    expires_at TIMESTAMPTZ NOT NULL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE TABLE IF NOT EXISTS auth.password_reset_codes (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),

    user_id UUID NOT NULL UNIQUE
        REFERENCES auth.users(id)
        ON DELETE CASCADE,

    code_hash TEXT NOT NULL,
    attempts INTEGER NOT NULL DEFAULT 0,
    expires_at TIMESTAMPTZ NOT NULL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE TABLE IF NOT EXISTS auth.password_reset_token (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),

    user_id UUID NOT NULL UNIQUE
        REFERENCES auth.users(id)
        ON DELETE CASCADE,

    token_hash TEXT NOT NULL,
    expires_at TIMESTAMPTZ NOT NULL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE TABLE auth.sessions (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),

    user_id UUID NOT NULL
        REFERENCES auth.users(id)
        ON DELETE CASCADE,

    refresh_token_hash TEXT NOT NULL UNIQUE,

    expires_at TIMESTAMPTZ NOT NULL,

    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    last_used_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE INDEX sessions_user_id_idx
    ON auth.sessions(user_id);

CREATE INDEX sessions_expires_at_idx
    ON auth.sessions(expires_at);

-- Kept for the existing /hello-postgres handler and its tests.
CREATE SCHEMA IF NOT EXISTS hello_schema;

CREATE TABLE IF NOT EXISTS hello_schema.users (
    name TEXT PRIMARY KEY,
    count INTEGER DEFAULT(1)
);

CREATE SCHEMA IF NOT EXISTS users;

CREATE TABLE users.profiles (
    user_id UUID PRIMARY KEY
        REFERENCES auth.users(id)
        ON DELETE CASCADE,

    first_name TEXT,
    last_name TEXT,
    middle_name TEXT,
    avatar_url TEXT,

    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE SCHEMA IF NOT EXISTS education;

CREATE TABLE IF NOT EXISTS education.universities (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    name TEXT NOT NULL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),

    CONSTRAINT universities_name_nonempty
        CHECK (name ~ '[^[:space:]]')
);

CREATE TABLE IF NOT EXISTS education.study_groups (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    university_id UUID NOT NULL,
    name TEXT NOT NULL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),

    CONSTRAINT study_groups_name_nonempty
        CHECK (name ~ '[^[:space:]]'),
    CONSTRAINT study_groups_university_name_key
        UNIQUE (university_id, name),
    -- Referenced by assignment tables to enforce the university boundary.
    CONSTRAINT study_groups_id_university_key
        UNIQUE (id, university_id),
    CONSTRAINT study_groups_university_fk
        FOREIGN KEY (university_id)
        REFERENCES education.universities(id)
        ON UPDATE RESTRICT ON DELETE RESTRICT
);

CREATE TABLE IF NOT EXISTS education.memberships (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    user_id UUID NOT NULL,
    university_id UUID NOT NULL,
    role TEXT NOT NULL,
    status TEXT NOT NULL DEFAULT 'active',
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),

    CONSTRAINT memberships_role_check
        CHECK (role IN ('student', 'teacher', 'admin')),
    CONSTRAINT memberships_status_check
        CHECK (status IN ('active', 'inactive')),
    CONSTRAINT memberships_user_university_role_key
        UNIQUE (user_id, university_id, role),
    -- Referenced by assignment tables to enforce both role and university.
    CONSTRAINT memberships_id_university_role_key
        UNIQUE (id, university_id, role),
    CONSTRAINT memberships_user_fk
        FOREIGN KEY (user_id)
        REFERENCES auth.users(id)
        ON UPDATE RESTRICT ON DELETE CASCADE,
    CONSTRAINT memberships_university_fk
        FOREIGN KEY (university_id)
        REFERENCES education.universities(id)
        ON UPDATE RESTRICT ON DELETE RESTRICT
);

CREATE TABLE IF NOT EXISTS education.student_groups (
    -- A student membership may have zero or one current group.
    membership_id UUID PRIMARY KEY,
    university_id UUID NOT NULL,
    group_id UUID NOT NULL,
    role TEXT NOT NULL DEFAULT 'student',
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),

    CONSTRAINT student_groups_role_check
        CHECK (role = 'student'),
    CONSTRAINT student_groups_membership_fk
        FOREIGN KEY (membership_id, university_id, role)
        REFERENCES education.memberships(id, university_id, role)
        ON UPDATE RESTRICT ON DELETE CASCADE,
    CONSTRAINT student_groups_group_fk
        FOREIGN KEY (group_id, university_id)
        REFERENCES education.study_groups(id, university_id)
        ON UPDATE RESTRICT ON DELETE RESTRICT
);

CREATE TABLE IF NOT EXISTS education.teacher_groups (
    membership_id UUID NOT NULL,
    university_id UUID NOT NULL,
    group_id UUID NOT NULL,
    role TEXT NOT NULL DEFAULT 'teacher',
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),

    CONSTRAINT teacher_groups_pkey
        PRIMARY KEY (membership_id, group_id),
    CONSTRAINT teacher_groups_role_check
        CHECK (role = 'teacher'),
    CONSTRAINT teacher_groups_membership_fk
        FOREIGN KEY (membership_id, university_id, role)
        REFERENCES education.memberships(id, university_id, role)
        ON UPDATE RESTRICT ON DELETE CASCADE,
    CONSTRAINT teacher_groups_group_fk
        FOREIGN KEY (group_id, university_id)
        REFERENCES education.study_groups(id, university_id)
        ON UPDATE RESTRICT ON DELETE RESTRICT
);

-- The memberships unique key already indexes memberships by user_id.
-- The teacher_groups primary key already indexes groups by membership_id.
CREATE INDEX IF NOT EXISTS memberships_university_id_idx
    ON education.memberships(university_id);

CREATE INDEX IF NOT EXISTS student_groups_group_id_idx
    ON education.student_groups(group_id, university_id);

CREATE INDEX IF NOT EXISTS teacher_groups_group_id_idx
    ON education.teacher_groups(group_id, university_id);

-- Inactive memberships retain assignments. Application authorization must
-- check membership.status = 'active' in the requested university.

-- BEGIN UNIVERSITY STRUCTURE
CREATE TABLE IF NOT EXISTS education.faculties (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    university_id UUID NOT NULL REFERENCES education.universities(id)
        ON UPDATE RESTRICT ON DELETE RESTRICT,
    name TEXT NOT NULL CHECK (name ~ '[^[:space:]]'),
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    UNIQUE (university_id, name),
    UNIQUE (id, university_id)
);

CREATE TABLE IF NOT EXISTS education.programs (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    university_id UUID NOT NULL REFERENCES education.universities(id)
        ON UPDATE RESTRICT ON DELETE RESTRICT,
    name TEXT NOT NULL CHECK (name ~ '[^[:space:]]'),
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    UNIQUE (university_id, name),
    UNIQUE (id, university_id)
);

CREATE TABLE IF NOT EXISTS education.faculty_programs (
    faculty_id UUID NOT NULL,
    program_id UUID NOT NULL,
    university_id UUID NOT NULL,
    PRIMARY KEY (faculty_id, program_id),
    FOREIGN KEY (faculty_id, university_id)
        REFERENCES education.faculties(id, university_id)
        ON UPDATE RESTRICT ON DELETE RESTRICT,
    FOREIGN KEY (program_id, university_id)
        REFERENCES education.programs(id, university_id)
        ON UPDATE RESTRICT ON DELETE CASCADE
);
CREATE INDEX IF NOT EXISTS faculty_programs_program_idx
    ON education.faculty_programs(program_id, university_id);

-- NULL temporarily supports existing groups awaiting classification.
ALTER TABLE education.study_groups ADD COLUMN IF NOT EXISTS program_id UUID;
DO $migration$
BEGIN
    IF NOT EXISTS (SELECT 1 FROM pg_constraint
        WHERE conrelid = 'education.study_groups'::regclass
            AND conname = 'study_groups_program_fk') THEN
        ALTER TABLE education.study_groups ADD CONSTRAINT study_groups_program_fk
            FOREIGN KEY (program_id, university_id)
            REFERENCES education.programs(id, university_id)
            ON UPDATE RESTRICT ON DELETE RESTRICT;
    END IF;

    -- Backfill only on the first run. Never promote faculty admins on reruns.
    IF NOT EXISTS (SELECT 1 FROM information_schema.columns
        WHERE table_schema = 'education' AND table_name = 'memberships'
            AND column_name = 'admin_scope') THEN
        ALTER TABLE education.memberships ADD COLUMN admin_scope TEXT;
        UPDATE education.memberships SET admin_scope = 'university'
            WHERE role = 'admin';
    END IF;

    IF NOT EXISTS (SELECT 1 FROM pg_constraint
        WHERE conrelid = 'education.memberships'::regclass
            AND conname = 'memberships_admin_scope_check') THEN
        ALTER TABLE education.memberships ADD CONSTRAINT memberships_admin_scope_check
            CHECK (
                (role = 'admin' AND admin_scope IS NOT NULL
                    AND admin_scope IN ('university', 'faculties'))
                OR (role <> 'admin' AND admin_scope IS NULL)
            );
    END IF;
    IF NOT EXISTS (SELECT 1 FROM pg_constraint
        WHERE conrelid = 'education.memberships'::regclass
            AND conname = 'memberships_admin_scope_key') THEN
        ALTER TABLE education.memberships ADD CONSTRAINT memberships_admin_scope_key
            UNIQUE (id, university_id, role, admin_scope);
    END IF;
END
$migration$;
CREATE INDEX IF NOT EXISTS study_groups_program_idx
    ON education.study_groups(program_id, university_id);

CREATE TABLE IF NOT EXISTS education.admin_faculties (
    membership_id UUID NOT NULL,
    faculty_id UUID NOT NULL,
    university_id UUID NOT NULL,
    role TEXT NOT NULL DEFAULT 'admin' CHECK (role = 'admin'),
    admin_scope TEXT NOT NULL DEFAULT 'faculties' CHECK (admin_scope = 'faculties'),
    PRIMARY KEY (membership_id, faculty_id),
    FOREIGN KEY (membership_id, university_id, role, admin_scope)
        REFERENCES education.memberships(id, university_id, role, admin_scope)
        ON UPDATE RESTRICT ON DELETE CASCADE,
    FOREIGN KEY (faculty_id, university_id)
        REFERENCES education.faculties(id, university_id)
        ON UPDATE RESTRICT ON DELETE RESTRICT
);
CREATE INDEX IF NOT EXISTS admin_faculties_faculty_idx
    ON education.admin_faculties(faculty_id, university_id);

-- A program and its faculty links must be created in one transaction.
-- Lock the parent before removing links, serializing concurrent removals.
CREATE OR REPLACE FUNCTION education.lock_program_faculty_changes()
RETURNS TRIGGER LANGUAGE plpgsql AS $function$
BEGIN
    PERFORM id FROM education.programs WHERE id = OLD.program_id FOR UPDATE;
    IF TG_OP = 'DELETE' THEN RETURN OLD; END IF;
    RETURN NEW;
END
$function$;
DROP TRIGGER IF EXISTS lock_program_faculty_changes ON education.faculty_programs;
CREATE TRIGGER lock_program_faculty_changes
    BEFORE DELETE OR UPDATE ON education.faculty_programs
    FOR EACH ROW EXECUTE FUNCTION education.lock_program_faculty_changes();

CREATE OR REPLACE FUNCTION education.require_program_faculty()
RETURNS TRIGGER LANGUAGE plpgsql AS $function$
DECLARE
    target_id UUID;
BEGIN
    IF TG_TABLE_NAME = 'programs' THEN
        target_id := NEW.id;
    ELSE
        target_id := OLD.program_id;
    END IF;
    IF EXISTS (SELECT 1 FROM education.programs WHERE id = target_id)
        AND NOT EXISTS (SELECT 1 FROM education.faculty_programs
            WHERE program_id = target_id) THEN
        RAISE EXCEPTION 'Program % must belong to at least one faculty', target_id
            USING ERRCODE = '23514', CONSTRAINT = 'program_requires_faculty';
    END IF;
    RETURN NULL;
END
$function$;
DROP TRIGGER IF EXISTS program_requires_faculty ON education.programs;
CREATE CONSTRAINT TRIGGER program_requires_faculty
    AFTER INSERT OR UPDATE ON education.programs
    DEFERRABLE INITIALLY DEFERRED
    FOR EACH ROW EXECUTE FUNCTION education.require_program_faculty();
DROP TRIGGER IF EXISTS program_retains_faculty ON education.faculty_programs;
CREATE CONSTRAINT TRIGGER program_retains_faculty
    AFTER DELETE OR UPDATE ON education.faculty_programs
    DEFERRABLE INITIALLY DEFERRED
    FOR EACH ROW EXECUTE FUNCTION education.require_program_faculty();
-- END UNIVERSITY STRUCTURE

BEGIN;
SELECT pg_advisory_xact_lock(60421, 5);
CREATE SCHEMA IF NOT EXISTS media;
CREATE TABLE IF NOT EXISTS media.images (
 id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
 owner_id UUID NOT NULL REFERENCES auth.users(id) ON DELETE RESTRICT,
 storage_key TEXT NOT NULL UNIQUE CHECK (storage_key <> ''),
 content_type TEXT NOT NULL CHECK (content_type IN ('image/png','image/jpeg','image/webp')),
 size_bytes BIGINT NOT NULL CHECK (size_bytes BETWEEN 1 AND 5242880),
 width INTEGER NOT NULL CHECK (width > 0), height INTEGER NOT NULL CHECK (height > 0),
 status TEXT NOT NULL DEFAULT 'pending' CHECK (status IN ('pending','ready')),
 created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(), ready_at TIMESTAMPTZ,
 CHECK ((status = 'pending' AND ready_at IS NULL) OR (status = 'ready' AND ready_at IS NOT NULL))
);
CREATE INDEX IF NOT EXISTS images_owner_created_idx ON media.images(owner_id,created_at DESC);
CREATE INDEX IF NOT EXISTS images_pending_created_idx ON media.images(created_at) WHERE status='pending';
CREATE SCHEMA IF NOT EXISTS quiz;
CREATE TABLE IF NOT EXISTS quiz.quizzes (
 id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
 author_id UUID NOT NULL REFERENCES auth.users(id) ON DELETE RESTRICT,
 university_id UUID NOT NULL REFERENCES education.universities(id) ON DELETE RESTRICT,
 name TEXT NOT NULL DEFAULT '', description TEXT NOT NULL DEFAULT '',
 default_time_seconds INTEGER NOT NULL DEFAULT 60 CHECK(default_time_seconds > 0),
 status TEXT NOT NULL DEFAULT 'draft' CHECK(status IN ('draft','ready')),
 revision BIGINT NOT NULL DEFAULT 1 CHECK(revision > 0),
 created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(), updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
 CHECK(status <> 'ready' OR name ~ '[^[:space:]]')
);
CREATE INDEX IF NOT EXISTS quizzes_author_updated_idx ON quiz.quizzes(author_id,updated_at DESC);
CREATE INDEX IF NOT EXISTS quizzes_university_status_idx ON quiz.quizzes(university_id,status);
CREATE TABLE IF NOT EXISTS quiz.questions (
 id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
 quiz_id UUID NOT NULL REFERENCES quiz.quizzes(id) ON DELETE CASCADE,
 text TEXT NOT NULL DEFAULT '', type TEXT NOT NULL DEFAULT 'single' CHECK(type IN ('single','multy')),
 time_seconds INTEGER CHECK(time_seconds > 0),
 image_id UUID REFERENCES media.images(id) ON DELETE RESTRICT,
 position INTEGER NOT NULL CHECK(position >= 0),
 UNIQUE(quiz_id,position) DEFERRABLE INITIALLY DEFERRED
);
CREATE TABLE IF NOT EXISTS quiz.answers (
 id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
 question_id UUID NOT NULL REFERENCES quiz.questions(id) ON DELETE CASCADE,
 text TEXT NOT NULL DEFAULT '', is_correct BOOLEAN NOT NULL DEFAULT FALSE,
 image_id UUID REFERENCES media.images(id) ON DELETE RESTRICT,
 position INTEGER NOT NULL CHECK(position >= 0),
 UNIQUE(question_id,position) DEFERRABLE INITIALLY DEFERRED
);
CREATE INDEX IF NOT EXISTS questions_image_idx ON quiz.questions(image_id) WHERE image_id IS NOT NULL;
CREATE INDEX IF NOT EXISTS answers_image_idx ON quiz.answers(image_id) WHERE image_id IS NOT NULL;
COMMIT;

-- BEGIN GAME SCHEMA
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

-- Required names for game sessions; safe to run repeatedly.
ALTER TABLE game.sessions ADD COLUMN IF NOT EXISTS name TEXT;
UPDATE game.sessions SET name = 'Сессия ' || id::text WHERE name IS NULL;
ALTER TABLE game.sessions ALTER COLUMN name SET NOT NULL;
ALTER TABLE game.sessions DROP CONSTRAINT IF EXISTS sessions_name_check;
ALTER TABLE game.sessions ADD CONSTRAINT sessions_name_check
    CHECK (char_length(name) BETWEEN 1 AND 200 AND btrim(name, U&'\0009\000A\000B\000C\000D\0020\0085\00A0\1680\2000\2001\2002\2003\2004\2005\2006\2007\2008\2009\200A\2028\2029\202F\205F\3000\FEFF') <> '');

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

-- END GAME SCHEMA

-- BEGIN TEST SCHEMA
-- Standalone tests and individual student progress; repeatable and additive.
BEGIN;
SET LOCAL lock_timeout = '5s';
SELECT pg_advisory_xact_lock(60421, 7);
CREATE SCHEMA IF NOT EXISTS test;
CREATE TABLE IF NOT EXISTS test.tests (
 id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
 author_id UUID NOT NULL REFERENCES auth.users(id) ON DELETE RESTRICT,
 university_id UUID NOT NULL REFERENCES education.universities(id) ON DELETE RESTRICT,
 name TEXT NOT NULL DEFAULT '', description TEXT NOT NULL DEFAULT '',
 time_to_complete INTEGER NOT NULL CHECK(time_to_complete > 0),
 status TEXT NOT NULL DEFAULT 'draft' CHECK(status IN ('draft','public','private')),
 revision BIGINT NOT NULL DEFAULT 1 CHECK(revision > 0),
 created_at TIMESTAMPTZ NOT NULL DEFAULT clock_timestamp(),
 CHECK(status = 'draft' OR name ~ '[^[:space:]]')
);
CREATE INDEX IF NOT EXISTS tests_author_created_idx ON test.tests(author_id,created_at DESC);
CREATE INDEX IF NOT EXISTS tests_university_status_idx ON test.tests(university_id,status);
CREATE TABLE IF NOT EXISTS test.questions (
 id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
 test_id UUID NOT NULL REFERENCES test.tests(id) ON DELETE CASCADE,
 text TEXT NOT NULL DEFAULT '', type TEXT NOT NULL DEFAULT 'single' CHECK(type IN ('single','multy')),
 image_id UUID REFERENCES media.images(id) ON DELETE RESTRICT,
 position INTEGER NOT NULL CHECK(position >= 0),
 UNIQUE(test_id,position) DEFERRABLE INITIALLY DEFERRED,
 UNIQUE(id,test_id)
);
CREATE TABLE IF NOT EXISTS test.answers (
 id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
 question_id UUID NOT NULL REFERENCES test.questions(id) ON DELETE CASCADE,
 text TEXT NOT NULL DEFAULT '', is_correct BOOLEAN NOT NULL DEFAULT FALSE,
 image_id UUID REFERENCES media.images(id) ON DELETE RESTRICT,
 position INTEGER NOT NULL CHECK(position >= 0),
 UNIQUE(question_id,position) DEFERRABLE INITIALLY DEFERRED,
 UNIQUE(id,question_id)
);
CREATE INDEX IF NOT EXISTS test_questions_image_idx ON test.questions(image_id) WHERE image_id IS NOT NULL;
CREATE INDEX IF NOT EXISTS test_answers_image_idx ON test.answers(image_id) WHERE image_id IS NOT NULL;
-- One attempt per student/test for now. Resuming never resets the deadline.
CREATE TABLE IF NOT EXISTS test.test_in_process (
 id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
 test_id UUID NOT NULL REFERENCES test.tests(id) ON DELETE RESTRICT,
 student_id UUID NOT NULL REFERENCES auth.users(id) ON DELETE RESTRICT,
 current_question_id UUID,
 started_at TIMESTAMPTZ NOT NULL DEFAULT clock_timestamp(),
 deadline_at TIMESTAMPTZ NOT NULL,
 status TEXT NOT NULL DEFAULT 'in_progress' CHECK(status IN ('in_progress','completed','expired')),
 finished_at TIMESTAMPTZ,
 UNIQUE(id,test_id),
 FOREIGN KEY(current_question_id,test_id) REFERENCES test.questions(id,test_id) ON DELETE RESTRICT,
 CHECK(deadline_at > started_at),
 CHECK((status='in_progress' AND current_question_id IS NOT NULL AND finished_at IS NULL)
    OR (status IN ('completed','expired') AND current_question_id IS NULL AND finished_at IS NOT NULL)),
 CHECK(finished_at IS NULL OR finished_at >= started_at)
);
-- Also supports the pre-existing manually prepared test schema.
CREATE UNIQUE INDEX IF NOT EXISTS test_progress_test_student_key ON test.test_in_process(test_id,student_id);
CREATE INDEX IF NOT EXISTS test_progress_student_idx ON test.test_in_process(student_id);
CREATE TABLE IF NOT EXISTS test.student_answers (
 id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
 process_id UUID NOT NULL,
 test_id UUID NOT NULL,
 question_id UUID NOT NULL,
 submitted_at TIMESTAMPTZ NOT NULL DEFAULT clock_timestamp(),
 UNIQUE(process_id,question_id), UNIQUE(id,question_id),
 FOREIGN KEY(process_id,test_id) REFERENCES test.test_in_process(id,test_id) ON DELETE RESTRICT,
 FOREIGN KEY(question_id,test_id) REFERENCES test.questions(id,test_id) ON DELETE RESTRICT
);
CREATE TABLE IF NOT EXISTS test.student_answer_choices (
 student_answer_id UUID NOT NULL,
 question_id UUID NOT NULL,
 answer_id UUID NOT NULL,
 PRIMARY KEY(student_answer_id,answer_id),
 FOREIGN KEY(student_answer_id,question_id) REFERENCES test.student_answers(id,question_id) ON DELETE RESTRICT,
 FOREIGN KEY(answer_id,question_id) REFERENCES test.answers(id,question_id) ON DELETE RESTRICT
);
COMMIT;

-- END TEST SCHEMA
