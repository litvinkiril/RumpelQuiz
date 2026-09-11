import asyncio
import base64
import hashlib
import json
import uuid

import pytest

from test_login import insert_user


async def login(service_client, pgsql):
    email = f'{uuid.uuid4()}@example.invalid'
    user_id = insert_user(pgsql, email, 'correct-password')
    response = await service_client.post(
        '/v1/auth/login', json={'email': email, 'password': 'correct-password'},
    )
    assert response.status == 200
    assert response.headers['Cache-Control'] == 'no-store'
    return user_id, response.json()


async def refresh(service_client, tokens, **overrides):
    return await service_client.post('/v1/auth/refresh', json={
        'session_id': tokens['session_id'],
        'refresh_token': tokens['refresh_token'],
        **overrides,
    })


def session(pgsql, session_id):
    cursor = pgsql['db_1'].cursor()
    cursor.execute(
        'SELECT user_id::text, refresh_token_hash, expires_at, created_at '
        'FROM auth.sessions WHERE id = %s', (session_id,),
    )
    return cursor.fetchone()


async def test_login_creates_session_and_claims(service_client, pgsql):
    user_id, tokens = await login(service_client, pgsql)
    row = session(pgsql, tokens['session_id'])
    assert row[0] == user_id
    assert row[1] == hashlib.sha256(tokens['refresh_token'].encode()).hexdigest()
    assert row[1] != tokens['refresh_token']
    assert abs((row[2] - row[3]).total_seconds() - 180 * 86400) < 10
    payload = tokens['access_token'].split('.')[1]
    claims = json.loads(base64.urlsafe_b64decode(payload + '=' * (-len(payload) % 4)))
    assert claims['user_id'] == user_id
    assert claims['session_id'] == tokens['session_id']
    assert claims['exp'] - claims['iat'] == 900


async def test_refresh_rotates_without_extending_session(service_client, pgsql):
    user_id, tokens = await login(service_client, pgsql)
    before = session(pgsql, tokens['session_id'])
    response = await refresh(service_client, tokens)
    assert response.status == 200
    assert response.headers['Cache-Control'] == 'no-store'
    rotated = response.json()
    assert rotated['refresh_token'] != tokens['refresh_token']
    assert rotated['session_id'] == tokens['session_id']
    after = session(pgsql, tokens['session_id'])
    assert after[1] == hashlib.sha256(rotated['refresh_token'].encode()).hexdigest()
    assert after[2:] == before[2:]
    me = await service_client.get('/v1/auth/me', headers={
        'Authorization': f"Bearer {rotated['access_token']}",
    })
    assert me.status == 200
    assert me.json()['user_id'] == user_id
    assert (await refresh(service_client, tokens)).status == 401
    assert (await refresh(service_client, rotated)).status == 200


async def test_wrong_token_does_not_revoke_valid_session(service_client, pgsql):
    _, tokens = await login(service_client, pgsql)
    before = session(pgsql, tokens['session_id'])
    assert (await refresh(service_client, tokens, refresh_token='0' * 64)).status == 401
    assert session(pgsql, tokens['session_id']) == before
    assert (await refresh(service_client, tokens)).status == 200


async def test_expired_session_rejected(service_client, pgsql):
    _, tokens = await login(service_client, pgsql)
    cursor = pgsql['db_1'].cursor()
    cursor.execute("UPDATE auth.sessions SET expires_at = NOW() - INTERVAL '1 second' "
                   'WHERE id = %s', (tokens['session_id'],))
    assert (await refresh(service_client, tokens)).status == 401


async def test_logout_removes_only_current_session(service_client, pgsql):
    _, tokens = await login(service_client, pgsql)
    _, other = await login(service_client, pgsql)
    response = await service_client.post('/v1/auth/logout', json={
        'session_id': other['session_id'],
    }, headers={'Authorization': f"Bearer {tokens['access_token']}"})
    assert response.status == 200
    assert session(pgsql, tokens['session_id']) is None
    assert session(pgsql, other['session_id']) is not None
    assert (await refresh(service_client, tokens)).status == 401
    assert (await refresh(service_client, other)).status == 200


async def test_multiple_logins_create_independent_sessions(service_client, pgsql):
    email = 'multiple-sessions@example.invalid'
    user_id = insert_user(pgsql, email, 'correct-password')
    tokens = []
    for _ in range(2):
        response = await service_client.post('/v1/auth/login', json={
            'email': email, 'password': 'correct-password',
        })
        assert response.status == 200
        tokens.append(response.json())
    assert tokens[0]['session_id'] != tokens[1]['session_id']
    assert all(session(pgsql, token['session_id'])[0] == user_id for token in tokens)
    logout = await service_client.post('/v1/auth/logout', json={}, headers={
        'Authorization': f"Bearer {tokens[0]['access_token']}",
    })
    assert logout.status == 200
    assert (await refresh(service_client, tokens[0])).status == 401
    assert (await refresh(service_client, tokens[1])).status == 200


async def test_tokens_and_hashes_are_not_logged(service_client, pgsql):
    async with service_client.capture_logs() as capture:
        _, tokens = await login(service_client, pgsql)
        response = await refresh(service_client, tokens)
        assert response.status == 200
        rotated = response.json()
        await service_client.get('/v1/auth/me', headers={
            'Authorization': f"Bearer {rotated['access_token']}",
        })
    logs = json.dumps(capture.select())
    for pair in [tokens, rotated]:
        for secret in [pair['access_token'], pair['refresh_token'],
                       hashlib.sha256(pair['refresh_token'].encode()).hexdigest()]:
            assert secret not in logs


async def test_concurrent_refresh_has_exactly_one_winner(service_client, pgsql):
    _, tokens = await login(service_client, pgsql)
    responses = await asyncio.gather(*(refresh(service_client, tokens) for _ in range(2)))
    assert sorted(r.status for r in responses) == [200, 401]
    winner = next(r.json() for r in responses if r.status == 200)
    assert (await refresh(service_client, winner)).status == 200


@pytest.mark.parametrize('body', [{}, {'session_id': 'invalid', 'refresh_token': 'x'},
                                     {'session_id': str(uuid.uuid4()), 'refresh_token': 42}])
async def test_refresh_invalid_request(service_client, body):
    response = await service_client.post('/v1/auth/refresh', json=body)
    assert response.status == 400


async def test_missing_session_and_unauthenticated_logout(service_client):
    response = await refresh(service_client, {
        'session_id': str(uuid.uuid4()), 'refresh_token': '0' * 64,
    })
    assert response.status == 401
    assert (await service_client.post('/v1/auth/logout', json={})).status == 401
