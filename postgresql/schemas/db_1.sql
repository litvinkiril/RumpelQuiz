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
