-- Apply once to an existing database before deploying the new backend.
-- Fresh databases use postgresql/schemas/db_1.sql.
BEGIN;

CREATE TABLE IF NOT EXISTS auth.sessions (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    user_id UUID NOT NULL REFERENCES auth.users(id) ON DELETE CASCADE,
    refresh_token_hash TEXT NOT NULL UNIQUE,
    expires_at TIMESTAMPTZ NOT NULL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    last_used_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE INDEX IF NOT EXISTS sessions_user_id_idx ON auth.sessions(user_id);
CREATE INDEX IF NOT EXISTS sessions_expires_at_idx ON auth.sessions(expires_at);

COMMIT;
