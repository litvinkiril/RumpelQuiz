-- Match the favourites table also used by manually prepared development databases.
BEGIN;
SET LOCAL lock_timeout = '5s';
SELECT pg_advisory_xact_lock(60421, 8);
CREATE TABLE IF NOT EXISTS test.favourites (
 user_id UUID NOT NULL REFERENCES auth.users(id) ON DELETE CASCADE,
 test_id UUID NOT NULL REFERENCES test.tests(id) ON DELETE CASCADE,
 created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
 PRIMARY KEY(user_id,test_id)
);
CREATE INDEX IF NOT EXISTS favourites_test_idx ON test.favourites(test_id);
COMMIT;
