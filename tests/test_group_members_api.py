import uuid

import pytest

from test_education_structure_api import study_group
from test_profile import create_user
from test_university_admins import membership, university


def endpoint(group_id):
    return f'/v1/education/groups/{group_id}/members'


def add_member(cursor, university_id, group_id, role, status='active', profile=None):
    user_id = create_user(cursor)
    membership_id = membership(cursor, user_id, university_id, role, status)
    table = 'teacher_groups' if role == 'teacher' else 'student_groups'
    cursor.execute(
        f'INSERT INTO education.{table} (membership_id, university_id, group_id) '
        'VALUES (%s, %s, %s)',
        (membership_id, university_id, group_id),
    )
    fields = ('first_name', 'last_name', 'middle_name', 'avatar_url')
    if profile is not None:
        cursor.execute(
            'INSERT INTO users.profiles '
            '(user_id, first_name, last_name, middle_name, avatar_url) '
            'VALUES (%s, %s, %s, %s, %s)',
            (user_id, *(profile.get(field) for field in fields)),
        )
    return {'id': user_id, **{field: (profile or {}).get(field) for field in fields}}


async def test_group_members_public_and_scoped(service_client, pgsql):
    cursor = pgsql['db_1'].cursor()
    target = university(cursor, 'Target')
    other = university(cursor, 'Other')
    group = study_group(cursor, target, 'Target Group')['id']
    sibling = study_group(cursor, target, 'Sibling Group')['id']
    foreign = study_group(cursor, other, 'Foreign Group')['id']

    # Insert out of order; each array must be sorted by name, nulls last.
    teacher_b = add_member(cursor, target, group, 'teacher', profile={
        'first_name': 'Пётр', 'last_name': 'Белов',
        'middle_name': 'Иванович', 'avatar_url': '/teacher.png',
    })
    teacher_a = add_member(cursor, target, group, 'teacher', profile={
        'first_name': 'Анна', 'last_name': 'Алексеева',
    })
    teacher_null = add_member(cursor, target, group, 'teacher')
    student_b = add_member(cursor, target, group, 'student', profile={
        'first_name': 'Иван', 'last_name': 'Белов', 'avatar_url': '/student.png',
    })
    student_a = add_member(cursor, target, group, 'student', profile={
        'first_name': 'Анна', 'last_name': 'Алексеева',
    })
    student_null = add_member(cursor, target, group, 'student')
    for role in ('teacher', 'student'):
        add_member(cursor, target, group, role, status='inactive')
        add_member(cursor, target, sibling, role)
        add_member(cursor, other, foreign, role)
        # A university membership alone does not make someone a group member.
        membership(cursor, create_user(cursor), target, role)

    # A teacher assigned to several groups must appear once in each group.
    cursor.execute(
        'INSERT INTO education.teacher_groups (membership_id, university_id, group_id) '
        "SELECT id, university_id, %s FROM education.memberships "
        "WHERE user_id = %s AND role = 'teacher'",
        (sibling, teacher_a['id']),
    )

    # No JWT, requester ID or university membership is required.
    response = await service_client.get(endpoint(group))
    assert response.status == 200
    assert response.headers['Cache-Control'] == 'no-store'
    assert response.json() == {
        'success': True,
        'teachers': [teacher_a, teacher_b, teacher_null],
        'students': [student_a, student_b, student_null],
    }


@pytest.mark.parametrize('state', ['empty', 'inactive_only', 'teachers_only', 'students_only'])
async def test_group_members_empty_arrays(service_client, pgsql, state):
    cursor = pgsql['db_1'].cursor()
    target = university(cursor, 'Target')
    group = study_group(cursor, target, 'Group')['id']
    expected = {'success': True, 'teachers': [], 'students': []}
    if state == 'inactive_only':
        for role in ('teacher', 'student'):
            add_member(cursor, target, group, role, status='inactive')
    elif state in ('teachers_only', 'students_only'):
        key = state.removesuffix('_only')
        role = 'teacher' if key == 'teachers' else 'student'
        expected[key] = [add_member(cursor, target, group, role)]

    response = await service_client.get(endpoint(group))
    assert response.status == 200
    assert response.json() == expected


async def test_group_members_missing_group(service_client):
    response = await service_client.get(endpoint(uuid.uuid4()))
    assert response.status == 404
    assert response.json() == {'success': False, 'error': 'group_not_found'}


@pytest.mark.parametrize('group_id', ['not-a-uuid', '1234', 'g' * 36])
async def test_group_members_invalid_id(service_client, group_id):
    response = await service_client.get(endpoint(group_id))
    assert response.status == 400
    assert response.json() == {'success': False, 'error': 'invalid_group_id'}


async def test_group_members_same_user_with_both_roles(service_client, pgsql):
    cursor = pgsql['db_1'].cursor()
    target = university(cursor, 'Target')
    group = study_group(cursor, target, 'Group')['id']
    person = add_member(cursor, target, group, 'teacher')
    student_membership = membership(cursor, person['id'], target, 'student')
    cursor.execute(
        'INSERT INTO education.student_groups (membership_id, university_id, group_id) '
        'VALUES (%s, %s, %s)',
        (student_membership, target, group),
    )
    response = await service_client.get(endpoint(group))
    assert response.status == 200
    assert response.json() == {
        'success': True, 'teachers': [person], 'students': [person],
    }
