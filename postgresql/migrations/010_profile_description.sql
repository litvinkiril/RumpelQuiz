ALTER TABLE users.profiles
    ADD COLUMN IF NOT EXISTS description TEXT;
