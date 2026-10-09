import asyncio
import uuid

import pytest

from test_profile import auth_headers, create_user
from test_search_tests import create_test, search, setup_catalog
from test_university_admins import membership, university


def endpoint(test_id):
    return f'/v1/tests/{test_id}/favourite'


async def change(client, user, test_id, method):
    response = await getattr(client, method)(endpoint(test_id), headers=auth_headers(user))
    assert response.status == 200, response.text
    assert response.json() == {'success': True}
    assert response.headers['Cache-Control'] == 'no-store'


@pytest.mark.parametrize('method', ['post', 'delete'])
async def test_favourites_require_authentication(service_client, method):
    response = await getattr(service_client, method)(endpoint(uuid.uuid4()))
    assert response.status == 401


@pytest.mark.parametrize('method', ['post', 'delete'])
@pytest.mark.parametrize('test_id', ['bad', '123', 'g' * 36, str(uuid.UUID(int=0))])
async def test_favourites_reject_invalid_test_id(service_client, method, test_id):
    response = await getattr(service_client, method)(
        endpoint(test_id), headers=auth_headers(uuid.uuid4()),
    )
    assert response.status == 400
    assert response.json() == {'success': False, 'error': 'invalid_test_id'}


@pytest.mark.parametrize('role', ['teacher', 'admin'])
async def test_favourites_add_remove_and_repeat(service_client, pgsql, role):
    cursor, user, uni = setup_catalog(pgsql, role)
    test_id = create_test(cursor, user, uni)
    assert await search(service_client, user, favourites='true') == []
    await change(service_client, user, test_id, 'post')
    cursor.execute('SELECT created_at FROM test.favourites WHERE user_id=%s AND test_id=%s',
                   (user, test_id))
    created_at = cursor.fetchone()[0]
    await change(service_client, user, test_id, 'post')
    cursor.execute('SELECT created_at FROM test.favourites WHERE user_id=%s AND test_id=%s',
                   (user, test_id))
    assert cursor.fetchall() == [(created_at,)]
    rows = await search(service_client, user, favourites='true')
    assert [row['test_id'] for row in rows] == [test_id]
    assert rows[0]['is_favourite'] is True
    await change(service_client, user, test_id, 'delete')
    await change(service_client, user, test_id, 'delete')
    assert await search(service_client, user, favourites='true') == []
    assert (await search(service_client, user))[0]['is_favourite'] is False


async def test_favourites_add_missing_test(service_client, pgsql):
    _, user, _ = setup_catalog(pgsql)
    response = await service_client.post(endpoint(uuid.uuid4()), headers=auth_headers(user))
    assert response.status == 404
    assert response.json() == {'success': False, 'error': 'test_not_found'}


@pytest.mark.parametrize('access', ['student', 'inactive', 'other_university', 'none'])
async def test_favourites_add_requires_catalog_access(service_client, pgsql, access):
    cursor = pgsql['db_1'].cursor()
    user, author = create_user(cursor), create_user(cursor)
    uni = university(cursor, 'Target')
    test_id = create_test(cursor, author, uni)
    if access == 'student':
        membership(cursor, user, uni, role='student')
    elif access == 'inactive':
        membership(cursor, user, uni, role='teacher', status='inactive')
    elif access == 'other_university':
        membership(cursor, user, university(cursor, 'Other'), role='teacher')
    response = await service_client.post(endpoint(test_id), headers=auth_headers(user))
    assert response.status == 404
    assert response.json() == {'success': False, 'error': 'test_not_found'}
    cursor.execute('SELECT count(*) FROM test.favourites WHERE user_id=%s', (user,))
    assert cursor.fetchone()[0] == 0


async def test_favourites_concurrent_add_creates_one_record(service_client, pgsql):
    cursor, user, uni = setup_catalog(pgsql)
    test_id = create_test(cursor, user, uni)
    await asyncio.gather(*(change(service_client, user, test_id, 'post') for _ in range(5)))
    cursor.execute('SELECT count(*) FROM test.favourites WHERE user_id=%s AND test_id=%s',
                   (user, test_id))
    assert cursor.fetchone()[0] == 1


async def test_favourites_delete_only_own_record_even_after_access_revoked(service_client, pgsql):
    cursor, user, uni = setup_catalog(pgsql)
    other = create_user(cursor)
    membership(cursor, other, uni, role='teacher')
    test_id = create_test(cursor, other, uni)
    await change(service_client, user, test_id, 'post')
    await change(service_client, other, test_id, 'post')
    cursor.execute("UPDATE education.memberships SET status='inactive' WHERE user_id=%s", (user,))
    await change(service_client, user, test_id, 'delete')
    cursor.execute('SELECT user_id FROM test.favourites WHERE test_id=%s', (test_id,))
    assert [str(row[0]) for row in cursor.fetchall()] == [other]
    assert [row['test_id'] for row in await search(service_client, other, favourites='true')] == [test_id]


async def test_favourites_delete_missing_record_is_successful(service_client, pgsql):
    cursor = pgsql['db_1'].cursor()
    user = create_user(cursor)
    await change(service_client, user, uuid.uuid4(), 'delete')
