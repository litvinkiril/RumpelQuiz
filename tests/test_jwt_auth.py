import base64
import hashlib
import hmac
import json
import time
import uuid


JWT_SECRET = b'testsuite-jwt-secret-that-is-at-least-32-characters'


def encode_base64url(value):
    return base64.urlsafe_b64encode(value).rstrip(b'=')


def make_access_token(user_id, expires_in=3600):
    now = int(time.time())
    header = encode_base64url(
        json.dumps(
            {'alg': 'HS256', 'typ': 'JWT'},
            separators=(',', ':'),
        ).encode(),
    )
    payload = encode_base64url(
        json.dumps(
            {'sub': str(user_id), 'iat': now, 'exp': now + expires_in},
            separators=(',', ':'),
        ).encode(),
    )
    signing_input = header + b'.' + payload
    signature = encode_base64url(
        hmac.new(JWT_SECRET, signing_input, hashlib.sha256).digest(),
    )
    return b'.'.join((header, payload, signature)).decode()


async def test_protected_endpoint_requires_token(service_client):
    response = await service_client.get('/v1/auth/me')

    assert response.status == 401


async def test_protected_endpoint_exposes_authenticated_user(service_client):
    user_id = uuid.uuid4()
    response = await service_client.get(
        '/v1/auth/me',
        headers={'Authorization': f'Bearer {make_access_token(user_id)}'},
    )

    assert response.status == 200
    assert response.json() == {'user_id': str(user_id)}


async def test_protected_endpoint_rejects_expired_token(service_client):
    token = make_access_token(uuid.uuid4(), expires_in=-1)
    response = await service_client.get(
        '/v1/auth/me',
        headers={'Authorization': f'Bearer {token}'},
    )

    assert response.status == 401


async def test_verify_email_returns_usable_access_token(
    service_client,
    pgsql,
):
    verification_id = uuid.uuid4()
    code = '123456'
    cursor = pgsql['db_1'].cursor()
    cursor.execute(
        '''
        INSERT INTO auth.users (email, password_hash)
        VALUES (%s, 'unused-test-password-hash')
        RETURNING id::text
        ''',
        ('jwt-verification@example.invalid',),
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
        VALUES (%s, %s, crypt(%s, gen_salt('bf')), NOW() + INTERVAL '5 minutes')
        ''',
        (str(verification_id), user_id, code),
    )

    response = await service_client.post(
        '/v1/auth/verify-email',
        json={
            'verification_id': str(verification_id),
            'code': code,
        },
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

    cursor.execute(
        'SELECT email_verified FROM auth.users WHERE id = %s',
        (user_id,),
    )
    assert cursor.fetchone()[0] is True
    cursor.execute(
        'SELECT COUNT(*) FROM auth.verification_codes WHERE id = %s',
        (str(verification_id),),
    )
    assert cursor.fetchone()[0] == 0
