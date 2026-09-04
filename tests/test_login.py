def insert_user(pgsql, email, password, email_verified=True):
    cursor = pgsql['db_1'].cursor()
    cursor.execute(
        '''
        INSERT INTO auth.users (
            email,
            password_hash,
            email_verified
        )
        VALUES (%s, crypt(%s, gen_salt('bf')), %s)
        RETURNING id::text
        ''',
        (email, password, email_verified),
    )
    return cursor.fetchone()[0]


async def test_successful_login_returns_usable_access_token(
    service_client,
    pgsql,
):
    email = 'login-success@example.invalid'
    password = 'correct-password'
    user_id = insert_user(pgsql, email, password)

    response = await service_client.post(
        '/v1/auth/login',
        json={'email': email, 'password': password},
    )

    assert response.status == 200
    body = response.json()
    assert body['success'] is True
    assert body['token_type'] == 'Bearer'
    assert body['access_token']

    me_response = await service_client.get(
        '/v1/auth/me',
        headers={
            'Authorization': f"Bearer {body['access_token']}",
        },
    )

    assert me_response.status == 200
    assert me_response.json() == {'user_id': user_id}


async def test_login_rejects_wrong_password(service_client, pgsql):
    email = 'login-wrong-password@example.invalid'
    insert_user(pgsql, email, 'correct-password')

    response = await service_client.post(
        '/v1/auth/login',
        json={'email': email, 'password': 'wrong-password'},
    )

    assert response.status == 401
    assert response.json() == {
        'success': False,
        'error': 'invalid_credentials',
    }


async def test_login_rejects_unknown_email(service_client):
    response = await service_client.post(
        '/v1/auth/login',
        json={
            'email': 'login-unknown@example.invalid',
            'password': 'any-password',
        },
    )

    assert response.status == 401
    assert response.json() == {
        'success': False,
        'error': 'invalid_credentials',
    }


async def test_login_rejects_unverified_email(service_client, pgsql):
    email = 'login-unverified@example.invalid'
    insert_user(pgsql, email, 'correct-password', email_verified=False)

    response = await service_client.post(
        '/v1/auth/login',
        json={'email': email, 'password': 'correct-password'},
    )

    assert response.status == 401
    assert response.json() == {
        'success': False,
        'error': 'invalid_credentials',
    }
