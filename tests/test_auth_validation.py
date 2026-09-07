import pytest

from test_login import insert_user


@pytest.mark.parametrize('route', ['register', 'login'])
@pytest.mark.parametrize('payload', [{}, {'email': 123, 'password': 'password'}])
async def test_auth_missing_or_wrong_fields(service_client, route, payload):
    response = await service_client.post('/v1/auth/' + route, json=payload)
    assert response.status == 400
    assert response.json()['success'] is False


@pytest.mark.parametrize('password', ['', 'x' * 73, 'pass\0word'])
async def test_registration_rejects_unsupported_password(service_client, password):
    response = await service_client.post('/v1/auth/register', json={
        'email': 'invalid-password@example.invalid', 'password': password,
        'password_confirmation': password,
    })
    assert response.status == 400


@pytest.mark.parametrize('suffix', ['x', '\0suffix'])
async def test_login_does_not_accept_truncated_password(service_client, pgsql, suffix):
    password = 'x' * 72
    email = 'truncated@example.invalid'
    insert_user(pgsql, email, password)
    response = await service_client.post('/v1/auth/login', json={
        'email': email, 'password': password + suffix,
    })
    assert response.status == 401
