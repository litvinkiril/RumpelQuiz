-- Run in the existing application database using pgAdmin Query Tool.
-- Fresh databases use postgresql/schemas/db_1.sql.
-- Reruns are supported for this exact schema; IF NOT EXISTS does not
-- reconcile pre-existing tables with different definitions.
BEGIN;

SET LOCAL lock_timeout = '5s';

-- Serialize concurrent runs of this migration.
SELECT pg_advisory_xact_lock(60421, 3);

DO $migration$
BEGIN
    IF to_regclass('users.user_roles') IS NOT NULL THEN
        -- Prevent an insert between the emptiness check and DROP TABLE.
        LOCK TABLE users.user_roles IN ACCESS EXCLUSIVE MODE;

        IF EXISTS (SELECT 1 FROM users.user_roles) THEN
            RAISE EXCEPTION
                'Migration aborted: users.user_roles is not empty';
        END IF;

        DROP TABLE users.user_roles RESTRICT;
    END IF;
END
$migration$;

DROP TYPE IF EXISTS users.user_role RESTRICT;

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

COMMIT;
