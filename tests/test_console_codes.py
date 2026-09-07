import re


from test_login import insert_user


def logged_code(capture, purpose, email):
    logs = capture.select()
    texts = [entry.get('text', '') for entry in logs]
    matching = [text for text in texts if '[DEV AUTH CODE]' in text and
                'purpose=' + purpose in text and 'email=' + email in text]
    assert len(matching) == 1
    assert 'ttl_seconds=900' in matching[0]
    return re.search(r'code=(\d{6})\b', matching[0]).group(1)


async def test_console_registration_and_resend(service_client, pgsql, mockserver):
    email = 'console-register@example.invalid'
    async with service_client.capture_logs() as capture:
        response = await service_client.post('/v1/auth/register', json={
            'email': email, 'password': 'password', 'password_confirmation': 'password',
        })
    assert response.status == 201
    verification_id = response.json()['verification_id']
    code = logged_code(capture, 'verify-email', email)
    assert 'code' not in response.json()
    cursor = pgsql['db_1'].cursor()
    cursor.execute("UPDATE auth.verification_codes SET created_at = NOW() - INTERVAL '2 minutes'")
    async with service_client.capture_logs() as capture:
        response = await service_client.post('/v1/auth/resend-code',
                                             json={'verification_id': verification_id})
    assert response.status == 200
    code = logged_code(capture, 'verify-email', email)
    response = await service_client.post('/v1/auth/verify-email', json={
        'verification_id': response.json()['verification_id'], 'code': code,
    })
    assert response.status == 200


async def test_console_reset_works_without_postbox(service_client, pgsql, mockserver):
    email = 'console-reset@example.invalid'
    insert_user(pgsql, email, 'old-password')
    async with service_client.capture_logs() as capture:
        response = await service_client.post('/v1/auth/forgot-password/email-check',
                                             json={'email': email})
    assert response.status == 200
    code = logged_code(capture, 'reset-password', email)
    response = await service_client.post('/v1/auth/forgot-password/verify-code', json={
        'verification_id': response.json()['verification_id'], 'code': code,
    })
    assert response.status == 200
    response = await service_client.post('/v1/auth/forgot-password/update-password', json={
        'reset_token': response.json()['reset_token'], 'password': 'new-password',
        'password_confirmation': 'new-password',
    })
    assert response.status == 200
