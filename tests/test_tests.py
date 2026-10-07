import asyncio
import copy
import uuid
from pathlib import Path

import pytest

from test_profile import auth_headers, create_user
from test_university_admins import university, membership


def setup(pgsql, role='teacher'):
    cursor = pgsql['db_1'].cursor()
    user = create_user(cursor)
    uni = university(cursor, 'Quiz University')
    membership(cursor, user, uni, role=role)
    return cursor, user, uni


def document(uni, status='public'):
    return {'university_id': uni, 'name': 'Столицы', 'description': '',
            'time_to_complete': 60, 'status': status,
            'questions': [{'text': 'Столица?', 'type': 'single',
                           'image_id': None, 'answers': [
                               {'text': 'Москва', 'is_correct': True, 'image_id': None},
                               {'text': 'Париж', 'is_correct': False, 'image_id': None}]}]}


async def save(client, user, body, test=None):
    if test:
        return await client.put('/v1/tests/' + test, json=body, headers=auth_headers(user))
    return await client.post('/v1/tests', json=body, headers=auth_headers(user))


@pytest.mark.parametrize('role', ['teacher', 'admin'])
@pytest.mark.parametrize('status', ['public', 'private'])
async def test_test_draft_ready_roundtrip(service_client, pgsql, role, status):
    cursor, user, uni = setup(pgsql, role)
    draft = {'university_id': uni, 'time_to_complete': 60, 'status': 'draft', 'questions': [{'text': 'Начало'}]}
    created = await save(service_client, user, draft)
    assert created.status == 201
    test = created.json()['test_id']
    loaded = await service_client.get('/v1/tests/' + test, headers=auth_headers(user))
    assert loaded.status == 200
    assert loaded.json()['test']['questions'][0]['answers'] == []
    assert loaded.json()['test']['time_to_complete'] == 60
    body = document(uni, status)
    body['revision'] = 1
    body['questions'].append(copy.deepcopy(body['questions'][0]))
    updated = await save(service_client, user, body, test)
    assert updated.status == 200 and updated.json()['revision'] == 2
    loaded = (await service_client.get('/v1/tests/' + test, headers=auth_headers(user))).json()['test']
    assert all('time_seconds' not in q for q in loaded['questions'])
    assert loaded['questions'][0]['answers'][0]['is_correct'] is True
    assert 'image_key' not in loaded['questions'][0]
    listing = (await service_client.get('/v1/tests', headers=auth_headers(user))).json()
    assert listing['tests'][0]['id'] == test
    assert (await save(service_client, user, body, test)).status == 409
    cursor.execute('SELECT count(*) FROM test.questions WHERE test_id=%s', (test,))
    assert cursor.fetchone()[0] == 2


@pytest.mark.parametrize('mutation', ['empty_name', 'no_questions', 'no_answers', 'no_correct', 'two_single', 'empty_answer', 'zero_time', 'unknown_type'])
async def test_test_ready_validation(service_client, pgsql, mutation):
    _, user, uni = setup(pgsql)
    body = document(uni)
    if mutation == 'empty_name': body['name'] = '\u00a0\u2003'
    if mutation == 'no_questions': body['questions'] = []
    if mutation == 'no_answers': body['questions'][0]['answers'] = []
    if mutation == 'no_correct': body['questions'][0]['answers'][0]['is_correct'] = False
    if mutation == 'two_single': body['questions'][0]['answers'][1]['is_correct'] = True
    if mutation == 'empty_answer': body['questions'][0]['answers'][1]['text'] = ' '
    if mutation == 'zero_time': body['time_to_complete'] = 0
    if mutation == 'unknown_type': body['questions'][0]['type'] = 'unexpected'
    response = await save(service_client, user, body)
    assert response.status == 400
    assert response.json()['details']


async def test_test_access_and_images(service_client, pgsql):
    cursor, user, uni = setup(pgsql)
    other = create_user(cursor)
    membership(cursor, other, uni, role='student')
    assert (await save(service_client, other, document(uni))).status == 403
    assert (await service_client.post('/v1/tests', json=document(uni))).status == 401
    image_id = str(uuid.uuid4())
    cursor.execute("INSERT INTO media.images(id,owner_id,storage_key,content_type,size_bytes,width,height,status,ready_at) VALUES(%s,%s,%s,'image/png',10,1,1,'ready',NOW())", (image_id, user, 'test/' + image_id))
    body = document(uni)
    body['questions'][0]['answers'][0].update(text='', image_id=image_id)
    body['status'] = 'draft'
    created = await save(service_client, user, body)
    assert created.status == 201
    test = created.json()['test_id']
    membership(cursor, other, uni, role='teacher')
    assert (await service_client.get('/v1/tests/' + test, headers=auth_headers(other))).status == 404
    assert (await service_client.get('/v1/tests', headers=auth_headers(other))).json()['tests'] == []
    assert (await save(service_client, other, body)).status == 400
    body.update(revision=1, name='Не должно сохраниться')
    body['questions'][0]['image_id'] = str(uuid.uuid4())
    assert (await save(service_client, user, body, test)).status == 400
    stored = (await service_client.get('/v1/tests/' + test, headers=auth_headers(user))).json()['test']
    assert stored['revision'] == 1 and stored['name'] == 'Столицы'
    cursor.execute("UPDATE education.memberships SET status='inactive' WHERE user_id=%s", (user,))
    assert (await save(service_client, user, body, test)).status == 403
    assert (await service_client.get('/v1/tests/' + test, headers=auth_headers(user))).status == 404


async def test_test_concurrent_revision(service_client, pgsql):
    _, user, uni = setup(pgsql)
    body = document(uni, 'draft')
    test = (await save(service_client, user, body)).json()['test_id']
    body['revision'] = 1
    replies = await asyncio.gather(save(service_client, user, body, test), save(service_client, user, body, test))
    assert sorted(r.status for r in replies) == [200, 409]


@pytest.mark.parametrize('status', ['draft', 'public'])
async def test_published_test_cannot_change(service_client, pgsql, status):
    _, user, uni = setup(pgsql)
    body = document(uni)
    test = (await save(service_client, user, body)).json()['test_id']
    before = (await service_client.get('/v1/tests/' + test, headers=auth_headers(user))).json()
    body.update(revision=1, status=status, name='Изменённое название')
    body['questions'][0]['answers'][0]['text'] = 'Изменённый ответ'
    response = await save(service_client, user, body, test)
    assert response.status == 409
    assert response.json()['error'] == 'test_published'
    after = (await service_client.get('/v1/tests/' + test, headers=auth_headers(user))).json()
    assert after == before


async def test_concurrent_publication_preserves_questions(service_client, pgsql):
    _, user, uni = setup(pgsql)
    body = document(uni, 'draft')
    test = (await save(service_client, user, body)).json()['test_id']
    body.update(revision=1, status='public')
    replies = await asyncio.gather(save(service_client, user, body, test), save(service_client, user, body, test))
    assert sorted(r.status for r in replies) == [200, 409]
    rejected = next(r for r in replies if r.status == 409)
    assert rejected.json()['error'] == 'test_published'
    body.update(revision=2, status='draft')
    assert (await save(service_client, user, body, test)).json()['error'] == 'test_published'


async def test_test_multy_and_empty_draft(service_client, pgsql):
    _, user, uni = setup(pgsql)
    assert (await save(service_client, user, {'university_id': uni, 'time_to_complete': 60, 'status': 'draft'})).status == 201
    body = document(uni)
    body['questions'][0]['type'] = 'multy'
    body['questions'][0]['answers'][1]['is_correct'] = True
    assert (await save(service_client, user, body)).status == 201
    body['revision'] = 1
    assert (await save(service_client, user, body)).status == 400


@pytest.mark.parametrize('status', ['public', 'private'])
async def test_test_migration_is_repeatable_and_preserves_answers(service_client, pgsql, status):
    cursor, user, uni = setup(pgsql)
    test_id = (await save(service_client, user, document(uni, status))).json()['test_id']
    before = (await service_client.get('/v1/tests/' + test_id, headers=auth_headers(user))).json()
    root = Path(__file__).parents[1] / 'postgresql'
    migration = (root / 'migrations/007_tests.sql').read_text()
    embedded = (root / 'schemas/db_1.sql').read_text().split('-- BEGIN TEST SCHEMA\n')[1].split('-- END TEST SCHEMA')[0]
    assert migration.strip() == embedded.strip()
    # The manually prepared development schema has these tables but no
    # unconditional student/test uniqueness needed by start/resume.
    cursor.execute('DROP INDEX test.test_progress_test_student_key')
    cursor.execute(migration)
    cursor.execute(migration)
    after = (await service_client.get('/v1/tests/' + test_id, headers=auth_headers(user))).json()
    assert after == before
    student = create_user(cursor)
    membership(cursor, student, uni, role='student')
    started = await service_client.post(f'/v1/tests/{test_id}/start', json={}, headers=auth_headers(student))
    resumed = await service_client.post(f'/v1/tests/{test_id}/start', json={}, headers=auth_headers(student))
    assert started.status == resumed.status == 200
    assert started.json()['attempt'] == resumed.json()['attempt']


@pytest.mark.parametrize('mutation,field', [
    ('missing_time', 'time_to_complete'), ('question_time', 'questions[0].time_seconds'),
    ('bool_time', 'time_to_complete'), ('huge_time', 'time_to_complete'),
    ('wrong_status', 'status'), ('bad_uuid', 'university_id'),
    ('nil_image', 'questions[0].image_id'), ('too_many_questions', 'questions'),
    ('too_many_answers', 'questions[0].answers'), ('wrong_correct_type', 'questions[0].answers[0].is_correct'),
])
async def test_test_request_errors_are_precise(service_client, pgsql, mutation, field):
    _, user, uni = setup(pgsql)
    body = document(uni)
    if mutation == 'missing_time': body.pop('time_to_complete')
    if mutation == 'question_time': body['questions'][0]['time_seconds'] = None
    if mutation == 'bool_time': body['time_to_complete'] = True
    if mutation == 'huge_time': body['time_to_complete'] = 2147483648
    if mutation == 'wrong_status': body['status'] = 'ready'
    if mutation == 'bad_uuid': body['university_id'] = 'bad'
    if mutation == 'nil_image': body['questions'][0]['image_id'] = str(uuid.UUID(int=0))
    if mutation == 'too_many_questions': body['questions'] *= 101
    if mutation == 'too_many_answers': body['questions'][0]['answers'] *= 11
    if mutation == 'wrong_correct_type': body['questions'][0]['answers'][0]['is_correct'] = 1
    response = await save(service_client, user, body)
    assert response.status == 400 and response.json()['details'][0]['field'] == field
