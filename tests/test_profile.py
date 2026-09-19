import uuid

import pytest

from test_jwt_auth import make_access_token


def auth_headers(user_id):
    return {'Authorization': f'Bearer {make_access_token(user_id)}'}


def create_user(cursor, verified=True):
    user_id = str(uuid.uuid4())
    cursor.execute(
        "INSERT INTO auth.users (id, email, password_hash, email_verified) "
        "VALUES (%s, %s, 'unused', %s)",
        (user_id, f'{user_id}@example.invalid', verified),
    )
    return user_id


async def test_profile_requires_token(service_client):
    response = await service_client.get('/v1/user/profile')
    assert response.status == 401


@pytest.mark.parametrize(
    'state,status,error',
    [
        ('missing_user', 404, 'user_not_found'),
        ('unverified', 403, 'email_not_verified'),
        ('missing_profile', 404, 'user_profile_not_found'),
    ],
)
async def test_profile_errors(service_client, pgsql, state, status, error):
    cursor = pgsql['db_1'].cursor()
    user_id = (
        str(uuid.uuid4()) if state == 'missing_user'
        else create_user(cursor, verified=state != 'unverified')
    )
    response = await service_client.get(
        '/v1/user/profile', headers=auth_headers(user_id),
    )
    assert response.status == status
    assert response.json() == {'success': False, 'error': error}


async def test_profile_without_memberships(service_client, pgsql):
    cursor = pgsql['db_1'].cursor()
    user_id = create_user(cursor)
    cursor.execute('INSERT INTO users.profiles (user_id) VALUES (%s)', (user_id,))
    response = await service_client.get(
        '/v1/user/profile', headers=auth_headers(user_id),
    )
    assert response.status == 200
    assert response.json() == {
        'success': True,
        'email': f'{user_id}@example.invalid',
        'first_name': None, 'last_name': None,
        'middle_name': None, 'avatar_url': None,
        'university_position': [],
    }


async def test_profile_memberships_and_groups(service_client, pgsql):
    cursor = pgsql['db_1'].cursor()
    user_id = create_user(cursor)
    cursor.execute(
        "INSERT INTO users.profiles "
        "(user_id, first_name, last_name, middle_name, avatar_url) "
        "VALUES (%s, 'Ivan', 'Ivanov', 'Ivanovich', '/avatar.png')",
        (user_id,),
    )
    # Two students with groups check that all membership IDs reach the query.
    # A third student has no group; teacher/admin roles must also have null.
    cases = [
        ('University A', 'student', 'active', 'A-1'),
        ('University B', 'student', 'active', 'B-2'),
        ('University C', 'student', 'active', None),
        ('University A', 'teacher', 'active', None),
        ('University B', 'admin', 'active', None),
        ('University C', 'teacher', 'inactive', None),
    ]
    universities = {}
    expected = []
    for name, role, status, group in cases:
        if name not in universities:
            cursor.execute(
                'INSERT INTO education.universities (name) VALUES (%s) RETURNING id',
                (name,),
            )
            universities[name] = str(cursor.fetchone()[0])
        university_id = universities[name]
        cursor.execute(
            'INSERT INTO education.memberships (user_id, university_id, role, status, admin_scope) '
            'VALUES (%s, %s, %s, %s, %s) RETURNING id',
            (user_id, university_id, role, status, 'university' if role == 'admin' else None),
        )
        membership_id = str(cursor.fetchone()[0])
        group_id = None
        if group:
            cursor.execute(
                'INSERT INTO education.study_groups (university_id, name) '
                'VALUES (%s, %s) RETURNING id', (university_id, group),
            )
            group_id = str(cursor.fetchone()[0])
            cursor.execute(
                'INSERT INTO education.student_groups '
                '(membership_id, university_id, group_id) VALUES (%s, %s, %s)',
                (membership_id, university_id, group_id),
            )
        if status == 'active':
            expected.append({
                'university_id': university_id, 'university_name': name,
                'role': role, 'group_id': group_id, 'group_name': group,
                'admin_scope': 'university' if role == 'admin' else None,
            })

    other_user = create_user(cursor)
    cursor.execute(
        "INSERT INTO education.memberships (user_id, university_id, role, admin_scope) "
        "VALUES (%s, %s, 'admin', 'university')", (other_user, universities['University C']),
    )
    response = await service_client.get(
        '/v1/user/profile', headers=auth_headers(user_id),
    )
    assert response.status == 200
    body = response.json()
    positions = body.pop('university_position')
    assert body == {
        'success': True, 'email': f'{user_id}@example.invalid',
        'first_name': 'Ivan', 'last_name': 'Ivanov',
        'middle_name': 'Ivanovich', 'avatar_url': '/avatar.png',
    }
    key = lambda item: (item['university_name'], item['role'])
    assert sorted(positions, key=key) == sorted(expected, key=key)
