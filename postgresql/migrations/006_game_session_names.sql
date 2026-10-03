BEGIN;
SET LOCAL lock_timeout = '5s';
SELECT pg_advisory_xact_lock(60421, 6);
-- Required names for game sessions; safe to run repeatedly.
ALTER TABLE game.sessions ADD COLUMN IF NOT EXISTS name TEXT;
UPDATE game.sessions SET name = 'Сессия ' || id::text WHERE name IS NULL;
ALTER TABLE game.sessions ALTER COLUMN name SET NOT NULL;
ALTER TABLE game.sessions DROP CONSTRAINT IF EXISTS sessions_name_check;
ALTER TABLE game.sessions ADD CONSTRAINT sessions_name_check
    CHECK (char_length(name) BETWEEN 1 AND 200 AND btrim(name, U&'\0009\000A\000B\000C\000D\0020\0085\00A0\1680\2000\2001\2002\2003\2004\2005\2006\2007\2008\2009\200A\2028\2029\202F\205F\3000\FEFF') <> '');
COMMIT;
