import uuid


async def test_verify_email_rejects_unknown_verification(service_client):
    response = await service_client.post(
        '/v1/auth/verify-email',
        json={
            'verification_id': str(uuid.uuid4()),
            'code': '123456',
        },
    )

    assert response.status == 400
    assert response.json() == {
        'success': False,
        'error': 'invalid_or_expired_code',
    }


async def test_verify_email_rejects_invalid_verification_id(service_client):
    response = await service_client.post(
        '/v1/auth/verify-email',
        json={
            'verification_id': 'not-a-uuid',
            'code': '123456',
        },
    )

    assert response.status == 400
    assert response.json() == {
        'success': False,
        'error': 'invalid_or_expired_code',
    }


async def test_verify_email_rejects_expired_code(service_client, pgsql):
    verification_id = str(uuid.uuid4())
    cursor = pgsql['db_1'].cursor()
    cursor.execute(
        '''
        INSERT INTO auth.users (email, password_hash)
        VALUES (%s, 'unused-test-password-hash')
        RETURNING id::text
        ''',
        ('verify-expired@example.invalid',),
    )
    user_id = cursor.fetchone()[0]
    cursor.execute(
        '''
        INSERT INTO auth.verification_codes (
            id,
            user_id,
            code_hash,
            expires_at
        )
        VALUES (
            %s,
            %s,
            crypt('123456', gen_salt('bf')),
            NOW() - INTERVAL '1 minute'
        )
        ''',
        (verification_id, user_id),
    )

    response = await service_client.post(
        '/v1/auth/verify-email',
        json={
            'verification_id': verification_id,
            'code': '123456',
        },
    )

    assert response.status == 400
    assert response.json() == {
        'success': False,
        'error': 'invalid_or_expired_code',
    }
    cursor.execute(
        'SELECT email_verified FROM auth.users WHERE id = %s',
        (user_id,),
    )
    assert cursor.fetchone()[0] is False
