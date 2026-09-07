import asyncio
import re
import uuid

import pytest

from test_login import insert_user

BASE = '/v1/auth/forgot-password/'


@pytest.fixture
def reset_mail(mockserver):
    messages = []

    @mockserver.handler('/v2/email/outbound-emails')
    def handler(request):
        text = request.json['Content']['Simple']['Body']['Text']['Data']
        messages.append(re.search(r'\b\d{6}\b', text).group())
        return mockserver.make_response('', 200)

    return messages


async def request_code(service_client, email):
    return await service_client.post(BASE + 'email-check', json={'email': email})


async def verify_code(service_client, verification_id, code):
    return await service_client.post(
        BASE + 'verify-code', json={'verification_id': verification_id, 'code': code},
    )


async def update_password(service_client, token, password='new-password', confirmation=None):
    return await service_client.post(
        BASE + 'update-password',
        json={'reset_token': token, 'password': password,
              'password_confirmation': password if confirmation is None else confirmation},
    )


async def prepare_token(service_client, pgsql, reset_mail):
    email = 'reset@example.invalid'
    insert_user(pgsql, email, 'old-password')
    response = await request_code(service_client, email)
    assert response.status == 200
    verification_id = response.json()['verification_id']
    response = await verify_code(service_client, verification_id, reset_mail[-1])
    assert response.status == 200
    return email, verification_id, response.json()['reset_token']


async def test_reset_full_flow(service_client, pgsql, reset_mail):
    email, verification_id, token = await prepare_token(service_client, pgsql, reset_mail)
    cursor = pgsql['db_1'].cursor()
    cursor.execute('SELECT token_hash FROM auth.password_reset_token WHERE id = %s',
                   (token.split('.')[0],))
    assert token.split('.')[1] not in cursor.fetchone()[0]
    assert (await verify_code(service_client, verification_id, reset_mail[-1])).status == 400
    assert (await update_password(service_client, token)).status == 200
    assert (await update_password(service_client, token)).status == 400
    for password, expected in [('old-password', 401), ('new-password', 200)]:
        response = await service_client.post('/v1/auth/login',
                                             json={'email': email, 'password': password})
        assert response.status == expected
        if expected == 200:
            me = await service_client.get('/v1/auth/me', headers={
                'Authorization': 'Bearer ' + response.json()['access_token'],
            })
            assert me.status == 200


@pytest.mark.parametrize('kind', ['unknown', 'unverified'])
async def test_reset_unavailable_user(service_client, pgsql, reset_mail, kind):
    email = 'unavailable@example.invalid'
    if kind == 'unverified':
        insert_user(pgsql, email, 'password', email_verified=False)
    response = await request_code(service_client, email)
    assert response.status == 400
    assert reset_mail == []


async def test_reset_cooldown_and_rotation(service_client, pgsql, reset_mail):
    email = 'rotation@example.invalid'
    insert_user(pgsql, email, 'password')
    first = await request_code(service_client, email)
    old_id = first.json()['verification_id']
    old_code = reset_mail[-1]
    assert (await request_code(service_client, email)).status == 429
    cursor = pgsql['db_1'].cursor()
    cursor.execute("UPDATE auth.password_reset_codes SET created_at = NOW() - INTERVAL '2 minutes'")
    second = await request_code(service_client, email)
    assert second.status == 200
    assert second.json()['verification_id'] != old_id
    assert (await verify_code(service_client, old_id, old_code)).status == 400
    assert (await verify_code(service_client, second.json()['verification_id'], reset_mail[-1])).status == 200


@pytest.mark.parametrize('expired', [False, True])
async def test_reset_invalid_code(service_client, pgsql, reset_mail, expired):
    email = 'code@example.invalid'
    insert_user(pgsql, email, 'password')
    response = await request_code(service_client, email)
    verification_id = response.json()['verification_id']
    code = reset_mail[-1]
    if expired:
        pgsql['db_1'].cursor().execute(
            "UPDATE auth.password_reset_codes SET expires_at = NOW() - INTERVAL '1 second'")
    else:
        code = '000000' if code != '000000' else '111111'
    assert (await verify_code(service_client, verification_id, code)).status == 400
    cursor = pgsql['db_1'].cursor()
    cursor.execute('SELECT count(*) FROM auth.password_reset_token')
    assert cursor.fetchone()[0] == 0


@pytest.mark.parametrize('kind', ['expired', 'tampered', 'unknown', 'mismatch', 'empty', 'long', 'nul'])
async def test_reset_invalid_update(service_client, pgsql, reset_mail, kind):
    email, _, token = await prepare_token(service_client, pgsql, reset_mail)
    password, confirmation = 'new-password', None
    if kind == 'expired':
        pgsql['db_1'].cursor().execute(
            "UPDATE auth.password_reset_token SET expires_at = NOW() - INTERVAL '1 second'")
    elif kind == 'tampered':
        token = token[:37] + ('0' if token[37] != '0' else '1') + token[38:]
    elif kind == 'unknown':
        token = str(uuid.uuid4()) + token[36:]
    elif kind == 'mismatch':
        confirmation = 'different'
    elif kind == 'empty':
        password = ''
    elif kind == 'long':
        password = 'x' * 73
    elif kind == 'nul':
        password = 'new\0password'
    assert (await update_password(service_client, token, password, confirmation)).status == 400
    login = await service_client.post('/v1/auth/login',
                                      json={'email': email, 'password': 'old-password'})
    assert login.status == 200


@pytest.mark.parametrize('route,payload', [
    ('email-check', {}), ('email-check', {'email': 123}),
    ('verify-code', {}), ('verify-code', {'verification_id': 'bad', 'code': '123456'}),
    ('verify-code', {'verification_id': str(uuid.uuid4()), 'code': 123456}),
    ('update-password', {}), ('update-password', {'reset_token': 123}),
])
async def test_reset_malformed_input(service_client, route, payload):
    response = await service_client.post(BASE + route, json=payload)
    assert response.status == 400
    assert response.json()['success'] is False


async def test_reset_token_consumed_atomically(service_client, pgsql, reset_mail):
    _, _, token = await prepare_token(service_client, pgsql, reset_mail)
    responses = await asyncio.gather(
        update_password(service_client, token, 'first-password'),
        update_password(service_client, token, 'second-password'),
    )
    assert sorted(response.status for response in responses) == [200, 400]


async def test_reset_code_attempt_limit(service_client, pgsql, reset_mail):
    email = 'attempts@example.invalid'
    insert_user(pgsql, email, 'password')
    response = await request_code(service_client, email)
    verification_id = response.json()['verification_id']
    code = reset_mail[-1]
    wrong = '000000' if code != '000000' else '111111'
    for _ in range(5):
        assert (await verify_code(service_client, verification_id, wrong)).status == 400
    assert (await verify_code(service_client, verification_id, code)).status == 400
    pgsql['db_1'].cursor().execute(
        "UPDATE auth.password_reset_codes SET created_at = NOW() - INTERVAL '2 minutes'")
    response = await request_code(service_client, email)
    assert response.status == 200
    assert (await verify_code(service_client, response.json()['verification_id'], reset_mail[-1])).status == 200


async def test_reset_code_consumed_atomically(service_client, pgsql, reset_mail):
    email = 'reset-code-race@example.invalid'
    insert_user(pgsql, email, 'password')
    response = await request_code(service_client, email)
    verification_id = response.json()['verification_id']
    code = reset_mail[-1]
    responses = await asyncio.gather(
        verify_code(service_client, verification_id, code),
        verify_code(service_client, verification_id, code),
    )
    assert sorted(response.status for response in responses) == [200, 400]


async def test_new_reset_token_invalidates_previous_token(service_client, pgsql, reset_mail):
    email, _, old_token = await prepare_token(service_client, pgsql, reset_mail)
    response = await request_code(service_client, email)
    assert response.status == 200
    response = await verify_code(service_client, response.json()['verification_id'], reset_mail[-1])
    new_token = response.json()['reset_token']
    assert old_token != new_token
    assert (await update_password(service_client, old_token)).status == 400
    assert (await update_password(service_client, new_token)).status == 200


async def test_reset_code_cannot_verify_email(service_client, pgsql, reset_mail):
    email = 'separate-codes@example.invalid'
    insert_user(pgsql, email, 'password')
    response = await request_code(service_client, email)
    verification_id = response.json()['verification_id']
    response = await service_client.post('/v1/auth/verify-email', json={
        'verification_id': verification_id, 'code': reset_mail[-1],
    })
    assert response.status == 400
    assert (await verify_code(service_client, verification_id, reset_mail[-1])).status == 200


async def test_reset_email_failure_can_be_retried(service_client, pgsql, mockserver):
    email = 'reset-mail-failure@example.invalid'
    insert_user(pgsql, email, 'password')

    @mockserver.handler('/v2/email/outbound-emails')
    def failing_mail(request):
        return mockserver.make_response('', 503)

    response = await request_code(service_client, email)
    assert response.status == 500
    cursor = pgsql['db_1'].cursor()
    cursor.execute('SELECT count(*) FROM auth.password_reset_codes')
    assert cursor.fetchone()[0] == 1
    assert (await request_code(service_client, email)).status == 429

    @mockserver.handler('/v2/email/outbound-emails')
    def working_mail(request):
        return mockserver.make_response('', 200)

    cursor.execute("UPDATE auth.password_reset_codes SET created_at = NOW() - INTERVAL '2 minutes'")
    assert (await request_code(service_client, email)).status == 200


@pytest.mark.parametrize('token', ['', 'not-a-token', 'x' * 101,
                                   str(uuid.uuid4()) + '.' + 'x' * 63,
                                   str(uuid.uuid4()) + '.' + 'x' * 65])
async def test_reset_rejects_malformed_token(service_client, token):
    assert (await update_password(service_client, token)).status == 400
