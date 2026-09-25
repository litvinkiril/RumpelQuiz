import uuid

import pytest

from test_profile import auth_headers, create_user
from test_university_admins import membership, university
from test_university_structure import faculty, program


def endpoint(uni):
    return f'/v1/education/universities/{uni}/people'


def person(cursor, uni, role='student', first='Кирилл', last='Литвин', middle='Андреевич'):
    user = create_user(cursor)
    member = membership(cursor, user, uni, role=role)
    cursor.execute('INSERT INTO users.profiles (user_id, first_name, last_name, middle_name) '
                   'VALUES (%s, %s, %s, %s)', (user, first, last, middle))
    return user, member


@pytest.mark.parametrize('role', ['student', 'teacher', 'admin'])
async def test_people_active_members_can_search(service_client, pgsql, role):
    cursor = pgsql['db_1'].cursor()
    uni = university(cursor, 'Target')
    user, _ = person(cursor, uni, role)
    response = await service_client.get(endpoint(uni), params={'q': '  АНДРЕЕВ\u00a0кир\tлит  '}, headers=auth_headers(user))
    assert response.status == 200
    assert response.headers['Cache-Control'] == 'no-store'
    assert response.json() == {'success': True, 'has_more': False, 'next_offset': None, 'people': [{
        'user_id': user, 'first_name': 'Кирилл', 'last_name': 'Литвин', 'middle_name': 'Андреевич',
        'avatar_url': None, 'email': f'{user}@example.invalid', 'roles': [role],
        'student_details': {'group': None, 'faculties': []} if role == 'student' else None,
    }]}


async def test_people_auth_and_access(service_client, pgsql):
    cursor = pgsql['db_1'].cursor()
    uni = university(cursor, 'Target')
    user = create_user(cursor)
    assert (await service_client.get(endpoint(uni), params={'q': 'Кирилл'})).status == 401
    for target in (uni, str(uuid.uuid4())):
        response = await service_client.get(endpoint(target), params={'q': 'Кирилл'}, headers=auth_headers(user))
        assert response.status == 403
    member = membership(cursor, user, uni, status='inactive')
    assert (await service_client.get(endpoint(uni), params={'q': 'Кирилл'}, headers=auth_headers(user))).status == 403
    cursor.execute("UPDATE education.memberships SET status='active' WHERE id=%s", (member,))
    assert (await service_client.get(endpoint(uni), params={'q': 'Кирилл'}, headers=auth_headers(user))).status == 200


@pytest.mark.parametrize('params', [
    {}, {'q': ''}, {'q': '  '}, {'q': 'Я'}, {'q': 'Я' * 101},
    {'q': 'Иван', 'limit': '0'}, {'q': 'Иван', 'limit': '51'},
    {'q': 'Иван', 'limit': '1x'}, {'q': 'Иван', 'offset': '-1'},
    {'q': 'Иван', 'offset': '10001'}, {'q': 'Иван', 'offset': '999999999999999'},
])
async def test_people_invalid_params(service_client, params):
    response = await service_client.get(endpoint(uuid.uuid4()), params=params, headers=auth_headers(uuid.uuid4()))
    assert response.status == 400
    assert response.json()['error'] == 'invalid_search_params'


async def test_people_invalid_university(service_client):
    response = await service_client.get(endpoint('bad'), params={'q': 'Иван'}, headers=auth_headers(uuid.uuid4()))
    assert response.status == 400
    assert response.json()['error'] == 'invalid_university_id'


async def test_people_scoping_and_shared_faculties(service_client, pgsql):
    cursor = pgsql['db_1'].cursor()
    uni, other = university(cursor, 'Target'), university(cursor, 'Other')
    user, member = person(cursor, uni)
    membership(cursor, user, uni, role='teacher')
    membership(cursor, user, uni, role='admin', status='inactive')
    membership(cursor, user, other, role='admin')
    person(cursor, other)
    inactive, inactive_member = person(cursor, uni)
    cursor.execute("UPDATE education.memberships SET status='inactive' WHERE id=%s", (inactive_member,))
    fkn, fen = faculty(cursor, uni, 'ФКН'), faculty(cursor, uni, 'ФЭН')
    course = program(cursor, uni, fkn)
    cursor.execute('INSERT INTO education.faculty_programs VALUES (%s,%s,%s)', (fen, course, uni))
    cursor.execute("INSERT INTO education.study_groups (university_id, program_id, name) VALUES (%s,%s,'ПИ-24') RETURNING id", (uni, course))
    group = str(cursor.fetchone()[0])
    cursor.execute('INSERT INTO education.student_groups (membership_id, university_id, group_id) VALUES (%s,%s,%s)', (member, uni, group))
    response = await service_client.get(endpoint(uni), params={'q': 'Кирилл'}, headers=auth_headers(user))
    assert response.status == 200
    people = response.json()['people']
    assert len(people) == 1
    assert people[0]['user_id'] == user
    assert people[0]['roles'] == ['student', 'teacher']
    assert people[0]['student_details'] == {'group': {'id': group, 'name': 'ПИ-24'},
                                          'faculties': [{'id': fkn, 'name': 'ФКН'}, {'id': fen, 'name': 'ФЭН'}]}


async def test_people_pagination_and_literal_search(service_client, pgsql):
    cursor = pgsql['db_1'].cursor()
    uni = university(cursor, 'Target')
    users = [person(cursor, uni, first='Иван', last='Одинаковый', middle=None)[0] for _ in range(3)]
    headers = auth_headers(users[0])
    first = (await service_client.get(endpoint(uni), params={'q': 'Иван', 'limit': '2'}, headers=headers)).json()
    assert first['has_more'] is True and first['next_offset'] == 2
    second = (await service_client.get(endpoint(uni), params={'q': 'Иван', 'limit': '2', 'offset': '2'}, headers=headers)).json()
    assert second['has_more'] is False and second['next_offset'] is None
    assert [p['user_id'] for p in first['people'] + second['people']] == sorted(users)
    for query in ('Иван Несуществующий', '%_', "' OR 1=1 --"):
        result = (await service_client.get(endpoint(uni), params={'q': query}, headers=headers)).json()
        assert result['people'] == []
    literal, _ = person(cursor, uni, first='Имя%_', last=None, middle=None)
    result = (await service_client.get(endpoint(uni), params={'q': '%_'}, headers=headers)).json()
    assert [p['user_id'] for p in result['people']] == [literal]


async def test_people_pagination_never_advertises_invalid_offset(service_client, pgsql):
    cursor = pgsql['db_1'].cursor()
    uni = university(cursor, 'Target')
    requester = create_user(cursor)
    membership(cursor, requester, uni)
    cursor.execute(
        "WITH new_users AS (INSERT INTO auth.users (email, password_hash, email_verified) "
        "SELECT 'person' || n || '@example.invalid', 'unused', true FROM generate_series(1,10002) n RETURNING id), "
        "profiles AS (INSERT INTO users.profiles (user_id, first_name) SELECT id, 'Иван' FROM new_users) "
        "INSERT INTO education.memberships (user_id, university_id, role) SELECT id, %s, 'student' FROM new_users",
        (uni,),
    )
    response = await service_client.get(endpoint(uni), params={'q': 'Иван', 'limit': '1', 'offset': '10000'}, headers=auth_headers(requester))
    assert response.status == 200
    assert len(response.json()['people']) == 1
    assert response.json()['has_more'] is False
    assert response.json()['next_offset'] is None
