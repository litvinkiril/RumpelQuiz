import uuid


def registration_payload(email, password, confirmation=None):
    return {
        'email': email,
        'password': password,
        'password_confirmation': confirmation or password,
    }


def install_postbox_mock(mockserver, status=200):
    calls = {'count': 0}

    @mockserver.handler('/v2/email/outbound-emails')
    def postbox_handler(request):
        calls['count'] += 1
        return mockserver.make_response('', status)

    return calls


async def test_successful_registration(service_client, pgsql, mockserver):
    postbox_calls = install_postbox_mock(mockserver)
    email = 'successful-registration@example.invalid'
    password = 'plain-password'

    response = await service_client.post(
        '/v1/auth/register',
        json=registration_payload(email, password),
    )

    assert response.status == 201
    response_body = response.json()
    assert response_body['success'] is True
    verification_id = str(uuid.UUID(response_body['verification_id']))

    cursor = pgsql['db_1'].cursor()
    cursor.execute(
        '''
        SELECT id::text, email, password_hash, email_verified
        FROM auth.users
        WHERE email = %s
        ''',
        (email,),
    )
    user_id, stored_email, password_hash, email_verified = cursor.fetchone()

    assert stored_email == email
    assert email_verified is False
    assert password_hash
    assert password_hash != password
    assert password_hash.startswith(('$2a$', '$2b$', '$2y$'))

    cursor.execute(
        '''
        SELECT id::text, user_id::text, code_hash, expires_at
        FROM auth.verification_codes
        WHERE user_id = %s
        ''',
        (user_id,),
    )
    stored_verification_id, stored_user_id, code_hash, expires_at = (
        cursor.fetchone()
    )

    assert stored_verification_id == verification_id
    assert stored_user_id == user_id
    assert code_hash
    assert not code_hash.isdigit()
    assert code_hash.startswith(('$2a$', '$2b$', '$2y$'))
    assert expires_at is not None
    assert postbox_calls['count'] == 1


async def test_passwords_do_not_match(service_client, pgsql):
    email = 'mismatch@example.invalid'
    response = await service_client.post(
        '/v1/auth/register',
        json=registration_payload(
            email,
            'password-one',
            'password-two',
        ),
    )

    assert response.status == 400
    assert response.json() == {
        'success': False,
        'error': 'passwords_do_not_match',
    }

    cursor = pgsql['db_1'].cursor()
    cursor.execute('SELECT COUNT(*) FROM auth.users WHERE email = %s', (email,))
    assert cursor.fetchone()[0] == 0
    cursor.execute('SELECT COUNT(*) FROM auth.verification_codes')
    assert cursor.fetchone()[0] == 0


async def test_register_and_reregister_unverified(
    service_client,
    pgsql,
    mockserver,
):
    postbox_calls = install_postbox_mock(mockserver)
    email = 'unverified@example.invalid'

    first_response = await service_client.post(
        '/v1/auth/register',
        json=registration_payload(email, 'password-one'),
    )

    assert first_response.status == 201
    first_body = first_response.json()
    assert first_body['success'] is True
    first_verification_id = str(uuid.UUID(first_body['verification_id']))

    cursor = pgsql['db_1'].cursor()
    cursor.execute(
        'SELECT id::text, password_hash FROM auth.users WHERE email = %s',
        (email,),
    )
    first_user_id, first_password_hash = cursor.fetchone()
    cursor.execute(
        '''
        SELECT id::text, code_hash
        FROM auth.verification_codes
        WHERE user_id = %s
        ''',
        (first_user_id,),
    )
    stored_first_verification_id, first_code_hash = cursor.fetchone()
    assert stored_first_verification_id == first_verification_id

    second_response = await service_client.post(
        '/v1/auth/register',
        json=registration_payload(email, 'password-two'),
    )

    assert second_response.status == 201
    second_body = second_response.json()
    assert second_body['success'] is True
    second_verification_id = str(uuid.UUID(second_body['verification_id']))

    cursor.execute(
        'SELECT id::text, password_hash FROM auth.users WHERE email = %s',
        (email,),
    )
    second_user_id, second_password_hash = cursor.fetchone()
    cursor.execute(
        '''
        SELECT id::text, code_hash
        FROM auth.verification_codes
        WHERE user_id = %s
        ''',
        (second_user_id,),
    )
    stored_second_verification_id, second_code_hash = cursor.fetchone()

    assert second_user_id == first_user_id
    assert second_password_hash != first_password_hash
    assert second_verification_id != first_verification_id
    assert stored_second_verification_id == second_verification_id
    assert second_code_hash != first_code_hash
    assert postbox_calls['count'] == 2

    cursor.execute('SELECT COUNT(*) FROM auth.users WHERE email = %s', (email,))
    assert cursor.fetchone()[0] == 1
    cursor.execute(
        'SELECT COUNT(*) FROM auth.verification_codes WHERE user_id = %s',
        (first_user_id,),
    )
    assert cursor.fetchone()[0] == 1


async def test_verified_email_conflict(service_client, pgsql):
    email = 'verified@example.invalid'
    original_password_hash = 'existing-hash'
    cursor = pgsql['db_1'].cursor()
    cursor.execute(
        '''
        INSERT INTO auth.users (email, password_hash, email_verified)
        VALUES (%s, %s, TRUE)
        ''',
        (email, original_password_hash),
    )

    response = await service_client.post(
        '/v1/auth/register',
        json=registration_payload(email, 'another-password'),
    )

    assert response.status == 409
    assert response.json() == {
        'success': False,
        'error': 'email_already_exists',
    }

    cursor.execute('SELECT COUNT(*) FROM auth.users WHERE email = %s', (email,))
    assert cursor.fetchone()[0] == 1
    cursor.execute(
        'SELECT password_hash FROM auth.users WHERE email = %s',
        (email,),
    )
    assert cursor.fetchone()[0] == original_password_hash
    cursor.execute(
        '''
        SELECT COUNT(*)
        FROM auth.verification_codes vc
        JOIN auth.users u ON u.id = vc.user_id
        WHERE u.email = %s
        ''',
        (email,),
    )
    assert cursor.fetchone()[0] == 0


async def test_postbox_failure_keeps_committed_data(
    service_client,
    pgsql,
    mockserver,
):
    install_postbox_mock(mockserver, status=500)
    email = 'postbox-failure@example.invalid'

    response = await service_client.post(
        '/v1/auth/register',
        json=registration_payload(email, 'password-one'),
    )

    assert response.status == 500

    cursor = pgsql['db_1'].cursor()
    cursor.execute(
        'SELECT id::text, email_verified FROM auth.users WHERE email = %s',
        (email,),
    )
    user_id, email_verified = cursor.fetchone()
    assert email_verified is False

    cursor.execute(
        'SELECT COUNT(*) FROM auth.verification_codes WHERE user_id = %s',
        (user_id,),
    )
    assert cursor.fetchone()[0] == 1
