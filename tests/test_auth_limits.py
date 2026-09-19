import asyncio

from test_login import insert_user


async def test_login_limit_and_expiry(service_client, pgsql):
    email = 'limited@example.invalid'
    insert_user(pgsql, email, 'correct-password')
    async def login(password):
        return await service_client.post('/v1/auth/login', json={
            'email': email, 'password': password,
        })
    failures = await asyncio.gather(*(login('wrong') for _ in range(7)))
    assert all(response.status == 401 for response in failures)
    assert (await login('correct-password')).status == 401
    cursor = pgsql['db_1'].cursor()
    cursor.execute('SELECT login_attempts FROM auth.users WHERE email = %s', (email,))
    assert cursor.fetchone()[0] == 5
    cursor.execute("UPDATE auth.users SET login_window_started_at = NOW() - INTERVAL '16 minutes' WHERE email = %s", (email,))
    assert (await login('correct-password')).status == 200
    cursor.execute('SELECT login_attempts FROM auth.users WHERE email = %s', (email,))
    assert cursor.fetchone()[0] == 0


async def test_email_code_limit_serializes_concurrent_attempts(service_client, pgsql):
    user_id = insert_user(pgsql, 'code-limit@example.invalid', 'password', False)
    cursor = pgsql['db_1'].cursor()
    cursor.execute("""
        INSERT INTO auth.verification_codes (user_id, code_hash, expires_at)
        VALUES (%s, crypt('123456', gen_salt('bf')), NOW() + INTERVAL '10 minutes')
        RETURNING id::text
    """, (user_id,))
    verification_id = cursor.fetchone()[0]
    async def verify(code):
        return await service_client.post('/v1/auth/verify-email', json={
            'verification_id': verification_id, 'code': code,
        })
    failures = await asyncio.gather(*(verify('000000') for _ in range(7)))
    assert all(response.status == 400 for response in failures)
    assert (await verify('123456')).status == 400
    cursor.execute('SELECT attempts FROM auth.verification_codes WHERE id = %s', (verification_id,))
    assert cursor.fetchone()[0] == 5
