-- Match the favourites table also used by manually prepared development databases.
BEGIN;
SET LOCAL lock_timeout = '5s';
SELECT pg_advisory_xact_lock(60421, 9);
CREATE TABLE IF NOT EXISTS quiz.favourites (
 user_id UUID NOT NULL REFERENCES auth.users(id) ON DELETE CASCADE,
 quiz_id UUID NOT NULL REFERENCES quiz.quizzes(id) ON DELETE CASCADE,
 created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
 PRIMARY KEY(user_id,quiz_id)
);
CREATE INDEX IF NOT EXISTS favourites_quiz_idx ON quiz.favourites(quiz_id);
COMMIT;
