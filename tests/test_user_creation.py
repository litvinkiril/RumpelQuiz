import asyncio
from pathlib import Path
import uuid

import pytest

from test_profile import auth_headers, create_user
from test_university_admins import membership, university
from test_university_structure import faculty


ENDPOINT = '/v1/user/create'


@pytest.fixture
def creation_data(pgsql):
    cursor = pgsql['db_1'].cursor()
    uni = university(cursor, 'University')
    actor = create_user(cursor)
    membership(cursor, actor, uni)
    payload = {
        'email': 'created@example.invalid', 'password': 'Strong-password-123',
        'first_name': 'Иван', 'last_name': 'Иванов',
        'university': uni, 'role': 'student',
    }
    return cursor, actor, payload


def assert_not_created(cursor, email):
    cursor.execute('SELECT id FROM auth.users WHERE email = %s', (email,))
    assert cursor.fetchall() == []


async def test_user_creation_requires_authentication(service_client):
    response = await service_client.post(ENDPOINT, json={})
    assert response.status == 401


@pytest.mark.parametrize('role,with_faculty', [
    ('student', False), ('teacher', False), ('admin', True),
])
async def test_user_creation_persists_profile_and_membership(
    service_client, creation_data, role, with_faculty,
):
    cursor, actor, payload = creation_data
    payload.update(role=role, middle_name='Иванович', description='Преподаватель математики')
    faculty_id = faculty(cursor, payload['university']) if with_faculty else None
    if faculty_id:
        payload['facultet_id'] = faculty_id

    response = await service_client.post(ENDPOINT, json=payload, headers=auth_headers(actor))
    assert response.status == 201, response.text
    body = response.json()
    user_id = str(uuid.UUID(body['user_id']))
    assert body == {'success': True, 'user_id': user_id}

    cursor.execute(
        'SELECT email, email_verified, password_hash FROM auth.users WHERE id = %s',
        (user_id,),
    )
    email, verified, password_hash = cursor.fetchone()
    assert (email, verified) == (payload['email'], True)
    assert password_hash.startswith('$2b$')
    assert password_hash != payload['password']
    scope = 'faculties' if role == 'admin' else None
    cursor.execute(
        'SELECT id, university_id, role, status, admin_scope '
        'FROM education.memberships WHERE user_id = %s', (user_id,),
    )
    member_id, uni, stored_role, status, stored_scope = cursor.fetchone()
    assert (str(uni), stored_role, status, stored_scope) == (
        payload['university'], role, 'active', scope,
    )
    cursor.execute(
        'SELECT faculty_id, university_id FROM education.admin_faculties '
        'WHERE membership_id = %s', (str(member_id),),
    )
    assert [(str(f), str(u)) for f, u in cursor.fetchall()] == (
        [(faculty_id, payload['university'])] if with_faculty else []
    )

    # Exercise password verification and the existing profile API as a real client.
    login = await service_client.post('/v1/auth/login', json={
        'email': payload['email'], 'password': payload['password'],
    })
    assert login.status == 200, login.text
    headers = {'Authorization': f"Bearer {login.json()['access_token']}"}
    profile = await service_client.get('/v1/user/profile', headers=headers)
    assert profile.status == 200, profile.text
    assert profile.json() == {
        'success': True, 'email': payload['email'],
        'first_name': payload['first_name'], 'last_name': payload['last_name'],
        'middle_name': payload['middle_name'], 'description': payload['description'],
        'avatar_url': None,
        'university_position': [{
            'university_id': payload['university'], 'university_name': 'University',
            'role': role, 'admin_scope': scope, 'group_id': None, 'group_name': None,
        }],
    }


@pytest.mark.parametrize('optional', [{}, {
    'middle_name': None, 'description': None, 'facultet_id': None,
}, {'middle_name': '', 'description': ''}])
async def test_user_creation_optional_fields(service_client, creation_data, optional):
    cursor, actor, payload = creation_data
    payload.update(optional)
    response = await service_client.post(ENDPOINT, json=payload, headers=auth_headers(actor))
    assert response.status == 201, response.text
    cursor.execute(
        'SELECT middle_name, description, avatar_url FROM users.profiles WHERE user_id = %s',
        (response.json()['user_id'],),
    )
    assert cursor.fetchone() == (optional.get('middle_name'), optional.get('description'), None)


async def test_user_creation_accepts_maximum_email_and_password_lengths(
    service_client, creation_data,
):
    cursor, actor, payload = creation_data
    payload.update(email='a' * 242 + '@example.com', password='я' * 36)
    assert len(payload['email']) == 254
    assert len(payload['password'].encode()) == 72
    response = await service_client.post(ENDPOINT, json=payload, headers=auth_headers(actor))
    assert response.status == 201, response.text
    login = await service_client.post('/v1/auth/login', json={
        'email': payload['email'], 'password': payload['password'],
    })
    assert login.status == 200, login.text


@pytest.mark.parametrize('field', [
    'email', 'password', 'first_name', 'last_name', 'university', 'role',
])
@pytest.mark.parametrize('value', ['missing', None, 123, [], {}])
async def test_user_creation_rejects_missing_or_wrong_required_fields(
    service_client, creation_data, field, value,
):
    cursor, actor, payload = creation_data
    if value == 'missing':
        del payload[field]
    else:
        payload[field] = value
    response = await service_client.post(ENDPOINT, json=payload, headers=auth_headers(actor))
    assert response.status == 400, response.text
    assert response.json() == {'success': False, 'error': 'invalid_request'}
    assert_not_created(cursor, 'created@example.invalid')


@pytest.mark.parametrize('field,value', [
    ('email', ''), ('email', 'missing-at'), ('email', 'a b@example.invalid'),
    ('email', 'a\n@example.invalid'), ('email', 'a\0@example.invalid'),
    ('email', 'x' * 250 + '@test.invalid'),
    ('password', ''), ('password', 'x' * 73), ('password', 'pass\0word'),
    ('password', 'я' * 37),
    ('first_name', ''), ('last_name', ''),
    ('first_name', ' \t\n'), ('last_name', ' \t\n'),
    ('first_name', 'Иван\0'), ('last_name', 'Иванов\0'),
    ('middle_name', 'Иванович\0'), ('description', 'Описание\0'),
    ('role', 'superadmin'), ('role', ''),
    ('university', 'not-a-uuid'), ('university', 'g' * 36),
    ('facultet_id', 'not-a-uuid'), ('facultet_id', 123),
    ('middle_name', 123), ('description', []),
])
async def test_user_creation_rejects_invalid_values(
    service_client, creation_data, field, value,
):
    cursor, actor, payload = creation_data
    payload[field] = value
    response = await service_client.post(ENDPOINT, json=payload, headers=auth_headers(actor))
    assert response.status == 400, response.text
    assert response.json() == {'success': False, 'error': 'invalid_request'}
    assert_not_created(cursor, 'created@example.invalid')


@pytest.mark.parametrize('payload', [None, [], 'text', 123])
async def test_user_creation_rejects_non_object_json(service_client, payload):
    response = await service_client.post(
        ENDPOINT, json=payload, headers=auth_headers(uuid.uuid4()),
    )
    assert response.status == 400


@pytest.mark.parametrize('role', ['student', 'teacher'])
async def test_user_creation_faculty_requires_admin_role(service_client, creation_data, role):
    cursor, actor, payload = creation_data
    payload.update(role=role, facultet_id=faculty(cursor, payload['university']))
    response = await service_client.post(ENDPOINT, json=payload, headers=auth_headers(actor))
    assert response.status == 400
    assert response.json() == {'success': False, 'error': 'invalid_request'}
    assert_not_created(cursor, payload['email'])


@pytest.mark.parametrize('faculty_value', ['missing', None])
async def test_user_creation_admin_requires_faculty(
    service_client, creation_data, faculty_value,
):
    cursor, actor, payload = creation_data
    payload['role'] = 'admin'
    if faculty_value != 'missing':
        payload['facultet_id'] = faculty_value

    response = await service_client.post(
        ENDPOINT, json=payload, headers=auth_headers(actor),
    )

    assert response.status == 400
    assert response.json() == {'success': False, 'error': 'invalid_faculty'}
    assert_not_created(cursor, payload['email'])
    cursor.execute('SELECT count(*) FROM users.profiles')
    assert cursor.fetchone()[0] == 0
    cursor.execute('SELECT count(*) FROM education.memberships')
    assert cursor.fetchone()[0] == 1  # Only the requesting administrator.
    cursor.execute('SELECT count(*) FROM education.admin_faculties')
    assert cursor.fetchone()[0] == 0


@pytest.mark.parametrize('access', [
    'student', 'teacher', 'inactive', 'other_university', 'none',
])
async def test_user_creation_requires_active_admin_in_target_university(
    service_client, creation_data, access,
):
    cursor, _, payload = creation_data
    actor = create_user(cursor)
    uni = payload['university']
    if access in ('student', 'teacher'):
        membership(cursor, actor, uni, role=access)
    elif access == 'inactive':
        membership(cursor, actor, uni, status='inactive')
    elif access == 'other_university':
        membership(cursor, actor, university(cursor, 'Other'))
    response = await service_client.post(ENDPOINT, json=payload, headers=auth_headers(actor))
    assert response.status == 403
    assert response.json() == {'success': False, 'error': 'university_access_denied'}
    assert_not_created(cursor, payload['email'])


async def test_user_creation_missing_university(service_client, creation_data):
    cursor, actor, payload = creation_data
    payload['university'] = str(uuid.uuid4())
    response = await service_client.post(ENDPOINT, json=payload, headers=auth_headers(actor))
    assert response.status == 404
    assert response.json() == {'success': False, 'error': 'university_not_found'}
    assert_not_created(cursor, payload['email'])


@pytest.mark.parametrize('target', ['missing', 'other_university'])
async def test_user_creation_rejects_invalid_faculty(service_client, creation_data, target):
    cursor, actor, payload = creation_data
    payload['role'] = 'admin'
    payload['facultet_id'] = (
        str(uuid.uuid4()) if target == 'missing'
        else faculty(cursor, university(cursor, 'Other'))
    )
    response = await service_client.post(ENDPOINT, json=payload, headers=auth_headers(actor))
    assert response.status == 400
    assert response.json() == {'success': False, 'error': 'invalid_faculty'}
    assert_not_created(cursor, payload['email'])


@pytest.mark.parametrize('target', ['own', 'unassigned', 'university', 'student', 'teacher'])
async def test_user_creation_respects_faculty_admin_scope(service_client, creation_data, target):
    cursor, _, payload = creation_data
    actor = create_user(cursor)
    member = membership(cursor, actor, payload['university'], scope='faculties')
    own = faculty(cursor, payload['university'], 'Own')
    cursor.execute(
        'INSERT INTO education.admin_faculties (membership_id, faculty_id, university_id) '
        'VALUES (%s, %s, %s)', (member, own, payload['university']),
    )
    payload['role'] = target if target in ('student', 'teacher') else 'admin'
    if target == 'own':
        payload['facultet_id'] = own
    elif target == 'unassigned':
        payload['facultet_id'] = faculty(cursor, payload['university'], 'Unassigned')
    response = await service_client.post(ENDPOINT, json=payload, headers=auth_headers(actor))
    if target == 'own':
        assert response.status == 201, response.text
    elif target == 'university':
        assert response.status == 400, response.text
        assert response.json() == {'success': False, 'error': 'invalid_faculty'}
        assert_not_created(cursor, payload['email'])
    else:
        assert response.status == 403, response.text
        assert response.json() == {'success': False, 'error': 'university_access_denied'}
        assert_not_created(cursor, payload['email'])


@pytest.mark.parametrize('verified', [True, False])
async def test_user_creation_does_not_overwrite_existing_account(
    service_client, creation_data, verified,
):
    cursor, actor, payload = creation_data
    existing = create_user(cursor, verified=verified)
    payload['email'] = f'{existing}@example.invalid'
    response = await service_client.post(ENDPOINT, json=payload, headers=auth_headers(actor))
    assert response.status == 409
    assert response.json() == {'success': False, 'error': 'email_already_exists'}
    cursor.execute('SELECT password_hash, email_verified FROM auth.users WHERE id = %s', (existing,))
    assert cursor.fetchone() == ('unused', verified)
    cursor.execute('SELECT user_id FROM users.profiles WHERE user_id = %s', (existing,))
    assert cursor.fetchall() == []
    cursor.execute('SELECT id FROM education.memberships WHERE user_id = %s', (existing,))
    assert cursor.fetchall() == []


async def test_user_creation_concurrent_duplicate_has_one_winner(service_client, creation_data):
    cursor, actor, payload = creation_data
    responses = await asyncio.gather(*(
        service_client.post(ENDPOINT, json=payload, headers=auth_headers(actor))
        for _ in range(2)
    ))
    assert sorted(r.status for r in responses) == [201, 409]
    loser = next(r for r in responses if r.status == 409)
    assert loser.json() == {'success': False, 'error': 'email_already_exists'}
    cursor.execute(
        'SELECT u.id, p.user_id, m.user_id FROM auth.users u '
        'JOIN users.profiles p ON p.user_id = u.id '
        'JOIN education.memberships m ON m.user_id = u.id WHERE u.email = %s',
        (payload['email'],),
    )
    rows = cursor.fetchall()
    assert len(rows) == 1
    assert rows[0][0] == rows[0][1] == rows[0][2]


async def test_user_creation_revoked_admin_cannot_reuse_token(service_client, creation_data):
    cursor, actor, payload = creation_data
    headers = auth_headers(actor)
    cursor.execute("UPDATE education.memberships SET status = 'inactive' WHERE user_id = %s", (actor,))
    response = await service_client.post(ENDPOINT, json=payload, headers=headers)
    assert response.status == 403
    assert_not_created(cursor, payload['email'])


@pytest.mark.parametrize('table', ['memberships', 'admin_faculties'])
async def test_user_creation_rolls_back_all_records_on_assignment_failure(
    service_client, creation_data, table,
):
    cursor, actor, payload = creation_data
    payload.update(role='admin', facultet_id=faculty(cursor, payload['university']))
    # Both table names come from the fixed parameter list above.
    cursor.execute(f'''
        CREATE FUNCTION education.reject_test_membership() RETURNS trigger
        LANGUAGE plpgsql AS $$ BEGIN RAISE EXCEPTION 'Test membership failure'; END $$;
        CREATE TRIGGER reject_test_membership BEFORE INSERT ON education.{table}
        FOR EACH ROW EXECUTE FUNCTION education.reject_test_membership();
    ''')
    try:
        response = await service_client.post(ENDPOINT, json=payload, headers=auth_headers(actor))
        assert response.status == 500
        assert_not_created(cursor, payload['email'])
        cursor.execute('SELECT count(*) FROM users.profiles')
        assert cursor.fetchone()[0] == 0
        cursor.execute('SELECT count(*) FROM education.memberships')
        assert cursor.fetchone()[0] == 1  # Only the requesting administrator.
        cursor.execute('SELECT count(*) FROM education.admin_faculties')
        assert cursor.fetchone()[0] == 0
    finally:
        cursor.execute(f'DROP TRIGGER reject_test_membership ON education.{table}')
        cursor.execute('DROP FUNCTION education.reject_test_membership()')


def test_profile_description_migration_preserves_data_and_is_repeatable(pgsql):
    cursor = pgsql['db_1'].cursor()
    user = create_user(cursor)
    cursor.execute('INSERT INTO users.profiles (user_id, first_name) VALUES (%s, %s)', (user, 'Иван'))
    migration = (
        Path(__file__).parents[1] / 'postgresql/migrations/010_profile_description.sql'
    ).read_text()
    cursor.execute('BEGIN')
    try:
        cursor.execute('ALTER TABLE users.profiles DROP COLUMN description')
        cursor.execute(migration)
        cursor.execute('SELECT first_name, description FROM users.profiles WHERE user_id = %s', (user,))
        assert cursor.fetchone() == ('Иван', None)
        cursor.execute('UPDATE users.profiles SET description = %s WHERE user_id = %s', ('Описание', user))
        cursor.execute(migration)
        cursor.execute('SELECT first_name, description FROM users.profiles WHERE user_id = %s', (user,))
        assert cursor.fetchone() == ('Иван', 'Описание')
    finally:
        cursor.execute('ROLLBACK')
