import uuid


def install_postbox_mock(mockserver):
    calls = {'count': 0}

    @mockserver.handler('/v2/email/outbound-emails')
    def postbox_handler(request):
        calls['count'] += 1
        return mockserver.make_response('', 200)

    return calls


def insert_verification(pgsql, email):
    cursor = pgsql['db_1'].cursor()
    cursor.execute(
        '''
        INSERT INTO auth.users (email, password_hash, email_verified)
        VALUES (%s, 'unused-test-password-hash', FALSE)
        RETURNING id::text
        ''',
        (email,),
    )
    user_id = cursor.fetchone()[0]
    verification_id = str(uuid.uuid4())
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
            crypt('111111', gen_salt('bf')),
            NOW()
        )
        ''',
        (verification_id, user_id),
    )
    cursor.execute(
        '''
        UPDATE auth.verification_codes
        SET created_at = NOW() - INTERVAL '2 minutes'
        WHERE id = %s
        ''',
        (verification_id,),
    )
    return user_id, verification_id


async def test_resend_code_replaces_verification_and_sends_email(
    service_client,
    pgsql,
    mockserver,
):
    postbox_calls = install_postbox_mock(mockserver)
    user_id, old_verification_id = insert_verification(
        pgsql,
        'resend-success@example.invalid',
    )

    response = await service_client.post(
        '/v1/auth/resend-code',
        json={'verification_id': old_verification_id},
    )

    assert response.status == 200
    body = response.json()
    assert body['success'] is True
    new_verification_id = str(uuid.UUID(body['verification_id']))
    assert new_verification_id != old_verification_id
    assert postbox_calls['count'] == 1

    cursor = pgsql['db_1'].cursor()
    cursor.execute(
        '''
        SELECT id::text, user_id::text, expires_at > NOW()
        FROM auth.verification_codes
        WHERE user_id = %s
        ''',
        (user_id,),
    )
    stored_verification_id, stored_user_id, is_active = cursor.fetchone()
    assert stored_verification_id == new_verification_id
    assert stored_user_id == user_id
    assert is_active is True


async def test_resend_code_rejects_unknown_verification(
    service_client,
    mockserver,
):
    postbox_calls = install_postbox_mock(mockserver)

    response = await service_client.post(
        '/v1/auth/resend-code',
        json={'verification_id': str(uuid.uuid4())},
    )

    assert response.status == 404
    assert response.json() == {
        'success': False,
        'error': 'verification_not_found',
    }
    assert postbox_calls['count'] == 0


async def test_resend_code_rejects_invalid_verification_id(service_client):
    response = await service_client.post(
        '/v1/auth/resend-code',
        json={'verification_id': 'not-a-uuid'},
    )

    assert response.status == 400
    assert response.json() == {
        'success': False,
        'error': 'invalid_verification_id',
    }


async def test_resend_code_enforces_cooldown(
    service_client,
    pgsql,
    mockserver,
):
    postbox_calls = install_postbox_mock(mockserver)
    _, verification_id = insert_verification(
        pgsql,
        'resend-cooldown@example.invalid',
    )
    cursor = pgsql['db_1'].cursor()
    cursor.execute(
        '''
        UPDATE auth.verification_codes
        SET created_at = NOW()
        WHERE id = %s
        ''',
        (verification_id,),
    )

    response = await service_client.post(
        '/v1/auth/resend-code',
        json={'verification_id': verification_id},
    )

    assert response.status == 429
    assert response.json() == {
        'success': False,
        'error': 'resend_too_soon',
    }
    assert postbox_calls['count'] == 0
