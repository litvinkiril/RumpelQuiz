import uuid

import pytest

from test_profile import auth_headers, create_user


def university(cursor, name):
    cursor.execute(
        'INSERT INTO education.universities (name) VALUES (%s) RETURNING id',
        (name,),
    )
    return str(cursor.fetchone()[0])


def membership(cursor, user_id, university_id, role='admin', status='active', scope='university'):
    cursor.execute(
        'INSERT INTO education.memberships (user_id, university_id, role, status, admin_scope) '
        'VALUES (%s, %s, %s, %s, %s) RETURNING id',
        (user_id, university_id, role, status, scope if role == 'admin' else None),
    )
    return str(cursor.fetchone()[0])


def endpoint(university_id):
    return f'/v1/education/universities/{university_id}/admins'


async def test_admins_require_token(service_client):
    response = await service_client.get(endpoint(uuid.uuid4()))
    assert response.status == 401


@pytest.mark.parametrize('university_id', ['not-a-uuid', '1234', 'g' * 36])
async def test_admins_reject_invalid_university_id(service_client, university_id):
    response = await service_client.get(
        endpoint(university_id), headers=auth_headers(uuid.uuid4()),
    )
    assert response.status == 400
    assert response.json() == {'success': False, 'error': 'invalid_university_id'}


@pytest.mark.parametrize('access', [
    'student', 'teacher', 'inactive', 'other_university', 'none', 'missing_university',
])
async def test_admins_deny_university_access(service_client, pgsql, access):
    cursor = pgsql['db_1'].cursor()
    user_id = create_user(cursor)
    target = university(cursor, 'Target')
    if access in ('student', 'teacher'):
        membership(cursor, user_id, target, role=access)
    elif access == 'inactive':
        membership(cursor, user_id, target, status='inactive')
    elif access == 'other_university':
        membership(cursor, user_id, university(cursor, 'Other'))
    elif access == 'missing_university':
        target = str(uuid.uuid4())

    response = await service_client.get(endpoint(target), headers=auth_headers(user_id))
    assert response.status == 403
    assert response.json() == {'success': False, 'error': 'university_access_denied'}
    assert response.headers['Cache-Control'] == 'no-store'


async def test_admins_return_only_active_contacts_in_requested_university(service_client, pgsql):
    cursor = pgsql['db_1'].cursor()
    target = university(cursor, 'Target')
    other = university(cursor, 'Other')
    requester = create_user(cursor)
    requester_membership = membership(cursor, requester, target)
    cursor.execute(
        'INSERT INTO users.profiles (user_id, first_name, last_name, middle_name, avatar_url) '
        "VALUES (%s, 'Иван', 'Иванов', 'Иванович', '/avatar.png')", (requester,),
    )
    # Another affiliation of the same person must never appear in this response.
    membership(cursor, requester, other, role='student')

    no_profile = create_user(cursor, verified=False)
    no_profile_membership = membership(cursor, no_profile, target)
    for role, status, university_id in [
        ('admin', 'inactive', target), ('teacher', 'active', target),
        ('student', 'active', target), ('admin', 'active', other),
    ]:
        membership(cursor, create_user(cursor), university_id, role, status)

    response = await service_client.get(endpoint(target), headers=auth_headers(requester))
    assert response.status == 200
    assert response.headers['Cache-Control'] == 'no-store'
    assert response.json() == {
        'success': True,
        'admins': [
            {
                'membership_id': requester_membership,
                'first_name': 'Иван', 'last_name': 'Иванов', 'middle_name': 'Иванович',
                'avatar_url': '/avatar.png', 'email': f'{requester}@example.invalid',
            },
            {
                'membership_id': no_profile_membership,
                'first_name': None, 'last_name': None, 'middle_name': None,
                'avatar_url': None, 'email': f'{no_profile}@example.invalid',
            },
        ],
    }

    # The same valid token must stop granting access after membership revocation.
    cursor.execute(
        "UPDATE education.memberships SET status = 'inactive' WHERE id = %s",
        (requester_membership,),
    )
    response = await service_client.get(endpoint(target), headers=auth_headers(requester))
    assert response.status == 403
