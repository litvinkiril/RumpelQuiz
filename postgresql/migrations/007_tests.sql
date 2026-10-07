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
