from pathlib import Path
import uuid

import pytest

from test_profile import auth_headers, create_user
from test_university_admins import membership, university


ENDPOINT = '/v1/tests/search'
PARAMS = {'name': '', 'favourites': 'false', 'count_spend': '0'}


def setup_catalog(pgsql, role='teacher'):
    cursor = pgsql['db_1'].cursor()
    user = create_user(cursor)
    uni = university(cursor, 'Catalog University')
    membership(cursor, user, uni, role=role)
    return cursor, user, uni


def create_test(cursor, author, uni, title='Тест', question_count=0,
                created_at='2026-01-01T00:00:00Z', status='draft'):
    test_id = str(uuid.uuid4())
    cursor.execute(
        'INSERT INTO test.tests '
        '(id, author_id, university_id, name, time_to_complete, created_at, status) '
        'VALUES (%s, %s, %s, %s, 60, %s, %s)',
        (test_id, author, uni, title, created_at, status),
    )
    for position in range(question_count):
        cursor.execute(
            'INSERT INTO test.questions (test_id, text, position) VALUES (%s, %s, %s)',
            (test_id, f'Question {position}', position),
        )
    return test_id


def favourite(cursor, user, test_id):
    cursor.execute('INSERT INTO test.favourites (user_id, test_id) VALUES (%s, %s)',
                   (user, test_id))


async def search(client, user, **params):
    response = await client.get(
        ENDPOINT, params={**PARAMS, **params}, headers=auth_headers(user),
    )
    assert response.status == 200, response.text
    assert response.headers['Cache-Control'] == 'no-store'
    return response.json()['tests']


async def test_catalog_requires_authentication(service_client):
    response = await service_client.get(ENDPOINT, params=PARAMS)
    assert response.status == 401
    response = await service_client.get(
        ENDPOINT, params=PARAMS, headers={'Authorization': 'Bearer invalid'},
    )
    assert response.status == 401


@pytest.mark.parametrize('params', [
    {},
    {'name': '', 'favourites': 'false'},
    {'name': '', 'count_spend': '0'},
    {'favourites': 'false', 'count_spend': '0'},
    *[{**PARAMS, 'favourites': value} for value in ('', 'TRUE', 'False', '1', 'yes')],
    *[{**PARAMS, 'count_spend': value} for value in (
        '', '-1', '-0', '+1', ' 1', '1 ', '1x', '1.5', 'oops',
        '2147483648', '99999999999999999999999999', '١',
    )],
])
async def test_catalog_rejects_invalid_params(service_client, params):
    response = await service_client.get(
        ENDPOINT, params=params, headers=auth_headers(uuid.uuid4()),
    )
    assert response.status == 400
    assert response.json() == {'success': False, 'error': 'invalid_search_params'}
    assert response.headers['Cache-Control'] == 'no-store'


@pytest.mark.parametrize('role', ['teacher', 'admin'])
async def test_catalog_returns_metadata_for_active_authors(service_client, pgsql, role):
    cursor, user, uni = setup_catalog(pgsql, role)
    author = create_user(cursor)
    cursor.execute(
        'INSERT INTO users.profiles (user_id, first_name, last_name) VALUES (%s, %s, %s)',
        (author, 'Анна', 'Иванова'),
    )
    test_id = create_test(cursor, author, uni, title='Столицы', question_count=3)
    assert await search(service_client, user) == [{
        'test_id': test_id, 'creator_first_name': 'Анна', 'creator_last_name': 'Иванова',
        'title': 'Столицы', 'question_count': 3, 'is_favourite': False,
    }]


@pytest.mark.parametrize('with_profile', [False, True])
async def test_catalog_handles_missing_profile_names(service_client, pgsql, with_profile):
    cursor, user, uni = setup_catalog(pgsql)
    if with_profile:
        cursor.execute('INSERT INTO users.profiles (user_id) VALUES (%s)', (user,))
    test_id = create_test(cursor, user, uni)
    assert await search(service_client, user) == [{
        'test_id': test_id, 'creator_first_name': '', 'creator_last_name': '',
        'title': 'Тест', 'question_count': 0, 'is_favourite': False,
    }]


@pytest.mark.parametrize('access', ['student', 'inactive', 'other_university', 'none'])
async def test_catalog_excludes_tests_without_active_author_role(service_client, pgsql, access):
    cursor = pgsql['db_1'].cursor()
    user, author = create_user(cursor), create_user(cursor)
    uni = university(cursor, 'Target')
    test_id = create_test(cursor, author, uni)
    favourite(cursor, user, test_id)
    if access == 'student':
        membership(cursor, user, uni, role='student')
    elif access == 'inactive':
        membership(cursor, user, uni, role='teacher', status='inactive')
    elif access == 'other_university':
        membership(cursor, user, university(cursor, 'Other'), role='teacher')
    assert await search(service_client, user) == []
    assert await search(service_client, user, favourites='true') == []


async def test_catalog_scopes_each_university_and_does_not_duplicate_roles(service_client, pgsql):
    cursor, user, uni = setup_catalog(pgsql)
    membership(cursor, user, uni, role='admin')
    other = university(cursor, 'Other Author University')
    membership(cursor, user, other, role='admin')
    student_uni = university(cursor, 'Student University')
    membership(cursor, user, student_uni, role='student')
    inaccessible = university(cursor, 'No Membership University')
    expected = [create_test(cursor, user, target) for target in (uni, other)]
    for target in (student_uni, inaccessible):
        favourite(cursor, user, create_test(cursor, user, target))
    assert [row['test_id'] for row in await search(service_client, user)] == sorted(expected)


async def test_catalog_rechecks_revoked_membership(service_client, pgsql):
    cursor, user, uni = setup_catalog(pgsql)
    create_test(cursor, user, uni)
    assert len(await search(service_client, user)) == 1
    cursor.execute("UPDATE education.memberships SET status='inactive' WHERE user_id=%s",
                   (user,))
    assert await search(service_client, user) == []


async def test_catalog_favourites_belong_to_requester_and_combine_with_name(service_client, pgsql):
    cursor, user, uni = setup_catalog(pgsql)
    other = create_user(cursor)
    membership(cursor, other, uni, role='teacher')
    own = create_test(cursor, user, uni, title='Математика')
    other_only = create_test(cursor, other, uni, title='Математика 2')
    different = create_test(cursor, user, uni, title='История')
    favourite(cursor, user, own)
    favourite(cursor, user, different)
    favourite(cursor, other, other_only)
    all_rows = await search(service_client, user)
    assert {row['test_id']: row['is_favourite'] for row in all_rows} == {
        own: True, other_only: False, different: True,
    }
    assert {row['test_id'] for row in await search(service_client, user, favourites='true')} == {
        own, different,
    }
    rows = await search(service_client, user, favourites='true', name='МАТЕМ')
    assert [row['test_id'] for row in rows] == [own]
    rows = await search(service_client, other, favourites='true')
    assert [row['test_id'] for row in rows] == [other_only]


@pytest.mark.parametrize('title,query', [
    ('МАТЕМАТИКА', 'тЕмА'), ('ЁЖИК', 'ёж'), ('Mixed CASE', 'xed ca'),
    ('Проценты %_ и символы', '%_'), ("O'Reilly", "'Rei"),
])
async def test_catalog_searches_case_insensitive_literal_substrings(service_client, pgsql, title, query):
    cursor, user, uni = setup_catalog(pgsql)
    matching = create_test(cursor, user, uni, title=title)
    create_test(cursor, user, uni, title='Не подходит')
    assert [row['test_id'] for row in await search(service_client, user, name=query)] == [matching]
    assert await search(service_client, user, name="' OR 1=1 --") == []
    assert await search(service_client, user, name='несуществующее название') == []


async def test_catalog_paginates_ten_rows_with_stable_order(service_client, pgsql):
    cursor, user, uni = setup_catalog(pgsql)
    old = [create_test(cursor, user, uni) for _ in range(12)]
    newer = [create_test(cursor, user, uni, created_at='2026-01-02T00:00:00Z')
             for _ in range(11)]
    expected = sorted(newer) + sorted(old)
    for offset, size in ((0, 10), (10, 10), (20, 3), (23, 0)):
        rows = await search(service_client, user, count_spend=str(offset))
        assert len(rows) == size
        assert [row['test_id'] for row in rows] == expected[offset:offset + 10]
    assert await search(service_client, user, count_spend='2147483647') == []
    assert [row['test_id'] for row in await search(service_client, user, count_spend='0001')] == expected[1:11]


async def test_catalog_filters_before_pagination(service_client, pgsql):
    cursor, user, uni = setup_catalog(pgsql)
    matching = [create_test(cursor, user, uni, title='Нужный') for _ in range(12)]
    for test_id in matching:
        favourite(cursor, user, test_id)
    for _ in range(12):
        create_test(cursor, user, uni, title='Нужный', created_at='2026-01-02T00:00:00Z')
        favourite(cursor, user, create_test(cursor, user, uni, title='Другой',
                                          created_at='2026-01-03T00:00:00Z'))
    rows = await search(service_client, user, name='Нужный', favourites='true', count_spend='10')
    assert [row['test_id'] for row in rows] == sorted(matching)[10:]


async def test_catalog_favourites_migration_is_repeatable(pgsql, service_source_dir):
    cursor, user, uni = setup_catalog(pgsql)
    test_id = create_test(cursor, user, uni)
    favourite(cursor, user, test_id)
    cursor.execute('SELECT created_at FROM test.favourites WHERE user_id=%s AND test_id=%s',
                   (user, test_id))
    created_at = cursor.fetchone()[0]
    migration = (Path(service_source_dir) / 'postgresql/migrations/008_test_favourites.sql').read_text()
    for _ in range(2):
        cursor.execute(migration)
        cursor.execute('SELECT user_id, test_id, created_at FROM test.favourites')
        rows = cursor.fetchall()
        assert len(rows) == 1
        assert (str(rows[0][0]), str(rows[0][1]), rows[0][2]) == (user, test_id, created_at)


@pytest.mark.parametrize('deleted', ['user', 'test'])
async def test_catalog_favourites_follow_deleted_entities(pgsql, deleted):
    cursor, user, uni = setup_catalog(pgsql)
    requester = create_user(cursor)
    test_id = create_test(cursor, user, uni)
    favourite(cursor, requester, test_id)
    if deleted == 'user':
        cursor.execute('DELETE FROM auth.users WHERE id=%s', (requester,))
    else:
        cursor.execute('DELETE FROM test.tests WHERE id=%s', (test_id,))
    cursor.execute('SELECT count(*) FROM test.favourites WHERE user_id=%s AND test_id=%s',
                   (requester, test_id))
    assert cursor.fetchone()[0] == 0
