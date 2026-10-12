-- Run after 004_university_structure.sql. Preserves all existing IDs and rows.
-- Repeatable; the original university-wide name constraints are stricter.
BEGIN;
SET LOCAL lock_timeout = '5s';

-- BEGIN EDUCATION NAME SCOPES
SELECT pg_advisory_xact_lock(60421, 11);
LOCK TABLE education.programs, education.faculty_programs,
    education.study_groups IN ACCESS EXCLUSIVE MODE;

ALTER TABLE education.programs
    DROP CONSTRAINT IF EXISTS programs_university_id_name_key;
ALTER TABLE education.study_groups
    DROP CONSTRAINT IF EXISTS study_groups_university_name_key;

DO $migration$
BEGIN
    IF NOT EXISTS (SELECT 1 FROM pg_constraint
        WHERE conrelid = 'education.programs'::regclass
            AND conname = 'programs_id_university_name_key') THEN
        ALTER TABLE education.programs
            ADD CONSTRAINT programs_id_university_name_key
            UNIQUE (id, university_id, name);
    END IF;

    IF NOT EXISTS (SELECT 1 FROM pg_constraint
        WHERE conrelid = 'education.study_groups'::regclass
            AND conname = 'study_groups_program_name_key') THEN
        ALTER TABLE education.study_groups
            ADD CONSTRAINT study_groups_program_name_key
            UNIQUE (program_id, name);
    END IF;
END
$migration$;

-- A program can belong to several faculties. Store its name in each link so
-- that an ordinary UNIQUE constraint enforces the faculty boundary.
ALTER TABLE education.faculty_programs ADD COLUMN IF NOT EXISTS program_name TEXT;
UPDATE education.faculty_programs AS fp SET program_name = p.name
    FROM education.programs AS p
    WHERE p.id = fp.program_id AND p.university_id = fp.university_id
        AND fp.program_name IS DISTINCT FROM p.name;
-- Flush the deferred faculty-link checks before altering the same table.
SET CONSTRAINTS education.program_retains_faculty IMMEDIATE;
SET CONSTRAINTS education.program_retains_faculty DEFERRED;
ALTER TABLE education.faculty_programs ALTER COLUMN program_name SET NOT NULL;

DO $migration$
BEGIN
    IF NOT EXISTS (SELECT 1 FROM pg_constraint
        WHERE conrelid = 'education.faculty_programs'::regclass
            AND conname = 'faculty_programs_faculty_name_key') THEN
        ALTER TABLE education.faculty_programs
            ADD CONSTRAINT faculty_programs_faculty_name_key
            UNIQUE (faculty_id, program_name);
    END IF;

    IF NOT EXISTS (SELECT 1 FROM pg_constraint
        WHERE conrelid = 'education.faculty_programs'::regclass
            AND conname = 'faculty_programs_program_name_fk') THEN
        ALTER TABLE education.faculty_programs
            ADD CONSTRAINT faculty_programs_program_name_fk
            FOREIGN KEY (program_id, university_id, program_name)
            REFERENCES education.programs(id, university_id, name)
            ON UPDATE CASCADE ON DELETE CASCADE;
    END IF;
END
$migration$;

-- Callers still supply only faculty_id, program_id and university_id.
-- The FK above keeps this copy synchronized when a program is renamed.
CREATE OR REPLACE FUNCTION education.sync_faculty_program_name()
RETURNS TRIGGER LANGUAGE plpgsql AS $function$
BEGIN
    SELECT p.name INTO NEW.program_name FROM education.programs AS p
        WHERE p.id = NEW.program_id AND p.university_id = NEW.university_id
        FOR SHARE;
    IF NOT FOUND THEN
        RAISE EXCEPTION 'Program % does not belong to university %',
            NEW.program_id, NEW.university_id
            USING ERRCODE = '23503',
                CONSTRAINT = 'faculty_programs_program_name_fk';
    END IF;
    RETURN NEW;
END
$function$;
DROP TRIGGER IF EXISTS sync_faculty_program_name ON education.faculty_programs;
CREATE TRIGGER sync_faculty_program_name
    BEFORE INSERT OR UPDATE ON education.faculty_programs
    FOR EACH ROW EXECUTE FUNCTION education.sync_faculty_program_name();
-- END EDUCATION NAME SCOPES

COMMIT;
