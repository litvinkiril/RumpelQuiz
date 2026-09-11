import asyncio
import base64
import hashlib
import hmac
import json
import re
import time
import uuid

import pytest

from test_jwt_auth import JWT_SECRET, make_access_token
from test_login import insert_user
from test_resend_code import insert_verification

ROUTES = ['register', 'login', 'verify-email', 'resend-code', 'refresh',
          'forgot-password/email-check', 'forgot-password/verify-code',
          'forgot-password/update-password']


@pytest.mark.parametrize('route', ROUTES)
@pytest.mark.parametrize('body', ['{broken', 'null', '[]'])
async def test_auth_rejects_invalid_json_shapes(service_client, route, body):
    response = await service_client.post('/v1/auth/' + route, data=body,
                                         headers={'Content-Type': 'application/json'})
    assert response.status == 400


@pytest.mark.parametrize('code', [None, 123456, '', '12345', '1234567', 'abcdef', '123\x0045'])
async def test_verification_rejects_invalid_code_input(service_client, code):
    response = await service_client.post('/v1/auth/verify-email', json={
        'verification_id': str(uuid.uuid4()), 'code': code,
    })
    assert response.status == 400
    assert response.json()['error'] == 'invalid_or_expired_code'


async def test_email_code_wrong_then_correct_then_replay(service_client, pgsql):
    user_id, verification_id = insert_verification(pgsql, 'code-retry@example.invalid')
    cursor = pgsql['db_1'].cursor()
    cursor.execute("UPDATE auth.verification_codes SET expires_at = NOW() + INTERVAL '5 minutes'")
    wrong = await service_client.post('/v1/auth/verify-email', json={
        'verification_id': verification_id, 'code': '222222',
    })
    assert wrong.status == 400
    cursor.execute('SELECT email_verified FROM auth.users WHERE id = %s', (user_id,))
    assert cursor.fetchone()[0] is False
    for expected in [200, 400]:
        response = await service_client.post('/v1/auth/verify-email', json={
            'verification_id': verification_id, 'code': '111111',
        })
        assert response.status == expected


async def test_email_code_consumed_atomically(service_client, pgsql):
    _, verification_id = insert_verification(pgsql, 'code-race@example.invalid')
    pgsql['db_1'].cursor().execute("UPDATE auth.verification_codes SET expires_at = NOW() + INTERVAL '5 minutes'")
    async def verify():
        return await service_client.post('/v1/auth/verify-email', json={
            'verification_id': verification_id, 'code': '111111',
        })
    responses = await asyncio.gather(verify(), verify())
    assert sorted(r.status for r in responses) == [200, 400]


async def test_resend_rejects_already_verified_user(service_client, pgsql):
    user_id, verification_id = insert_verification(pgsql, 'already-verified@example.invalid')
    pgsql['db_1'].cursor().execute('UPDATE auth.users SET email_verified = TRUE WHERE id = %s', (user_id,))
    response = await service_client.post('/v1/auth/resend-code', json={'verification_id': verification_id})
    assert response.status == 404


@pytest.mark.parametrize('header', ['Basic abc', 'Bearer', 'Bearer ', 'Bearer not.a.jwt', 'Bearer a.b.c.d'])
async def test_auth_rejects_bad_authorization(service_client, header):
    assert (await service_client.get('/v1/auth/me', headers={'Authorization': header})).status == 401


def sign_custom(header, payload, secret=JWT_SECRET):
    def b64(data):
        return base64.urlsafe_b64encode(json.dumps(data).encode()).rstrip(b'=')
    data = b64(header) + b'.' + b64(payload)
    signature = base64.urlsafe_b64encode(hmac.new(secret, data, hashlib.sha256).digest()).rstrip(b'=')
    return (data + b'.' + signature).decode()


@pytest.mark.parametrize('kind', ['algorithm', 'type', 'future', 'exp_before_iat',
                                  'missing_exp', 'wrong_sub', 'wrong_types', 'wrong_secret',
                                  'missing_user', 'missing_session', 'wrong_session',
                                  'wrong_user', 'inconsistent_subject'])
async def test_jwt_rejects_invalid_claims(service_client, kind):
    now = int(time.time())
    header = {'alg': 'HS256', 'typ': 'JWT'}
    payload = {'sub': str(uuid.uuid4()), 'iat': now, 'exp': now + 3600}
    payload.update(user_id=payload['sub'], session_id=str(uuid.uuid4()))
    secret = JWT_SECRET
    if kind == 'algorithm': header['alg'] = 'none'
    if kind == 'type': header['typ'] = 'OTHER'
    if kind == 'future': payload['iat'] = now + 120
    if kind == 'exp_before_iat': payload.update(iat=now + 30, exp=now + 10)
    if kind == 'missing_exp': del payload['exp']
    if kind == 'wrong_sub': payload['sub'] = 'not-a-uuid'
    if kind == 'wrong_types': payload['exp'] = str(now + 3600)
    if kind == 'wrong_secret': secret = b'another-secret'
    if kind == 'missing_user': del payload['user_id']
    if kind == 'missing_session': del payload['session_id']
    if kind == 'wrong_session': payload['session_id'] = 'not-a-uuid'
    if kind == 'wrong_user': payload.update(user_id='not-a-uuid', sub='not-a-uuid')
    if kind == 'inconsistent_subject': payload['sub'] = str(uuid.uuid4())
    token = sign_custom(header, payload, secret)
    assert (await service_client.get('/v1/auth/me', headers={'Authorization': 'Bearer ' + token})).status == 401


async def test_normal_mode_does_not_log_plaintext_codes(service_client, mockserver):
    @mockserver.handler('/v2/email/outbound-emails')
    def postbox(request):
        return mockserver.make_response('', 200)
    async with service_client.capture_logs() as capture:
        response = await service_client.post('/v1/auth/register', json={
            'email': 'no-console@example.invalid', 'password': 'password',
            'password_confirmation': 'password',
        })
    assert response.status == 201
    assert not any('[DEV AUTH CODE]' in log.get('text', '') for log in capture.select())
