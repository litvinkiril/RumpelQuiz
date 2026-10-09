import asyncio
import uuid

import pytest

from test_profile import auth_headers, create_user
from test_search_quizzes import create_quiz, search_quizzes, setup_quiz_catalog
from test_university_admins import membership, university


def endpoint(quiz_id):
    return f'/v1/quizzes/{quiz_id}/favourite'


async def change(client, user, quiz_id, method):
    response = await getattr(client, method)(endpoint(quiz_id), headers=auth_headers(user))
    assert response.status == 200, response.text
    assert response.json() == {'success': True}
    assert response.headers['Cache-Control'] == 'no-store'


@pytest.mark.parametrize('method', ['post', 'delete'])
async def test_quiz_favourites_require_authentication(service_client, method):
    response = await getattr(service_client, method)(endpoint(uuid.uuid4()))
    assert response.status == 401


@pytest.mark.parametrize('method', ['post', 'delete'])
@pytest.mark.parametrize('quiz_id', ['bad', '123', 'g' * 36, str(uuid.UUID(int=0))])
async def test_quiz_favourites_reject_invalid_quiz_id(service_client, method, quiz_id):
    response = await getattr(service_client, method)(
        endpoint(quiz_id), headers=auth_headers(uuid.uuid4()),
    )
    assert response.status == 400
    assert response.json() == {'success': False, 'error': 'invalid_quiz_id'}


@pytest.mark.parametrize('role', ['teacher', 'admin'])
async def test_quiz_favourites_add_remove_and_repeat(service_client, pgsql, role):
    cursor, user, uni = setup_quiz_catalog(pgsql, role)
    quiz_id = create_quiz(cursor, user, uni)
    assert await search_quizzes(service_client, user, favourites='true') == []
    await change(service_client, user, quiz_id, 'post')
    cursor.execute('SELECT created_at FROM quiz.favourites WHERE user_id=%s AND quiz_id=%s',
                   (user, quiz_id))
    created_at = cursor.fetchone()[0]
    await change(service_client, user, quiz_id, 'post')
    cursor.execute('SELECT created_at FROM quiz.favourites WHERE user_id=%s AND quiz_id=%s',
                   (user, quiz_id))
    assert cursor.fetchall() == [(created_at,)]
    rows = await search_quizzes(service_client, user, favourites='true')
    assert [row['quiz_id'] for row in rows] == [quiz_id]
    assert rows[0]['is_favourite'] is True
    await change(service_client, user, quiz_id, 'delete')
    await change(service_client, user, quiz_id, 'delete')
    assert await search_quizzes(service_client, user, favourites='true') == []
    assert (await search_quizzes(service_client, user))[0]['is_favourite'] is False


async def test_quiz_favourites_add_missing_quiz(service_client, pgsql):
    _, user, _ = setup_quiz_catalog(pgsql)
    response = await service_client.post(endpoint(uuid.uuid4()), headers=auth_headers(user))
    assert response.status == 404
    assert response.json() == {'success': False, 'error': 'quiz_not_found'}


@pytest.mark.parametrize('access', ['student', 'inactive', 'other_university', 'none'])
async def test_quiz_favourites_add_requires_catalog_access(service_client, pgsql, access):
    cursor = pgsql['db_1'].cursor()
    user, author = create_user(cursor), create_user(cursor)
    uni = university(cursor, 'Target')
    quiz_id = create_quiz(cursor, author, uni)
    if access == 'student':
        membership(cursor, user, uni, role='student')
    elif access == 'inactive':
        membership(cursor, user, uni, role='teacher', status='inactive')
    elif access == 'other_university':
        membership(cursor, user, university(cursor, 'Other'), role='teacher')
    response = await service_client.post(endpoint(quiz_id), headers=auth_headers(user))
    assert response.status == 404
    assert response.json() == {'success': False, 'error': 'quiz_not_found'}
    cursor.execute('SELECT count(*) FROM quiz.favourites WHERE user_id=%s', (user,))
    assert cursor.fetchone()[0] == 0


async def test_quiz_favourites_concurrent_add_creates_one_record(service_client, pgsql):
    cursor, user, uni = setup_quiz_catalog(pgsql)
    quiz_id = create_quiz(cursor, user, uni)
    await asyncio.gather(*(change(service_client, user, quiz_id, 'post') for _ in range(5)))
    cursor.execute('SELECT count(*) FROM quiz.favourites WHERE user_id=%s AND quiz_id=%s',
                   (user, quiz_id))
    assert cursor.fetchone()[0] == 1


async def test_quiz_favourites_delete_only_own_record_even_after_access_revoked(service_client, pgsql):
    cursor, user, uni = setup_quiz_catalog(pgsql)
    other = create_user(cursor)
    membership(cursor, other, uni, role='teacher')
    quiz_id = create_quiz(cursor, other, uni)
    await change(service_client, user, quiz_id, 'post')
    await change(service_client, other, quiz_id, 'post')
    cursor.execute("UPDATE education.memberships SET status='inactive' WHERE user_id=%s", (user,))
    await change(service_client, user, quiz_id, 'delete')
    cursor.execute('SELECT user_id FROM quiz.favourites WHERE quiz_id=%s', (quiz_id,))
    assert [str(row[0]) for row in cursor.fetchall()] == [other]
    assert [row['quiz_id'] for row in await search_quizzes(service_client, other, favourites='true')] == [quiz_id]


async def test_quiz_favourites_delete_missing_record_is_successful(service_client, pgsql):
    cursor = pgsql['db_1'].cursor()
    user = create_user(cursor)
    await change(service_client, user, uuid.uuid4(), 'delete')


async def test_quiz_favourites_are_independent_of_test_favourites(service_client, pgsql):
    from test_search_tests import create_test, search

    cursor, user, uni = setup_quiz_catalog(pgsql)
    shared_id = create_test(cursor, user, uni)
    quiz_id = create_quiz(cursor, user, uni)
    # Even equal IDs in the two tables must refer to independent bookmarks.
    cursor.execute('UPDATE quiz.quizzes SET id=%s WHERE id=%s', (shared_id, quiz_id))
    await change(service_client, user, shared_id, 'post')
    assert (await search(service_client, user))[0]['is_favourite'] is False
    response = await service_client.post(
        f'/v1/tests/{shared_id}/favourite', headers=auth_headers(user),
    )
    assert response.status == 200
    await change(service_client, user, shared_id, 'delete')
    assert await search_quizzes(service_client, user, favourites='true') == []
    assert (await search(service_client, user))[0]['is_favourite'] is True
    await change(service_client, user, shared_id, 'post')
    response = await service_client.delete(
        f'/v1/tests/{shared_id}/favourite', headers=auth_headers(user),
    )
    assert response.status == 200
    assert (await search_quizzes(service_client, user))[0]['is_favourite'] is True
