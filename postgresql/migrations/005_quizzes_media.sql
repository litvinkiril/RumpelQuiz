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
