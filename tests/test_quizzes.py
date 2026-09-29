import asyncio
import copy
import uuid

import pytest

from test_profile import auth_headers, create_user
from test_university_admins import university, membership


def setup(pgsql, role='teacher'):
    cursor = pgsql['db_1'].cursor()
    user = create_user(cursor)
    uni = university(cursor, 'Quiz University')
    membership(cursor, user, uni, role=role)
    return cursor, user, uni


def document(uni, status='ready'):
    return {'university_id': uni, 'name': 'Столицы', 'description': '',
            'default_time_seconds': 60, 'status': status,
            'questions': [{'text': 'Столица?', 'type': 'single', 'time_seconds': None,
                           'image_id': None, 'answers': [
                               {'text': 'Москва', 'is_correct': True, 'image_id': None},
                               {'text': 'Париж', 'is_correct': False, 'image_id': None}]}]}


async def save(client, user, body, quiz=None):
    if quiz:
        return await client.put('/v1/quizzes/' + quiz, json=body, headers=auth_headers(user))
    return await client.post('/v1/quizzes', json=body, headers=auth_headers(user))


@pytest.mark.parametrize('role', ['teacher', 'admin'])
async def test_quiz_draft_ready_roundtrip(service_client, pgsql, role):
    cursor, user, uni = setup(pgsql, role)
    draft = {'university_id': uni, 'status': 'draft', 'questions': [{'text': 'Начало'}]}
    created = await save(service_client, user, draft)
    assert created.status == 201
    quiz = created.json()['quiz_id']
    loaded = await service_client.get('/v1/quizzes/' + quiz, headers=auth_headers(user))
    assert loaded.status == 200
    assert loaded.json()['quiz']['questions'][0]['answers'] == []
    assert loaded.json()['quiz']['default_time_seconds'] == 60
    body = document(uni)
    body['revision'] = 1
    body['questions'].append(copy.deepcopy(body['questions'][0]))
    body['questions'][1]['time_seconds'] = 90
    updated = await save(service_client, user, body, quiz)
    assert updated.status == 200 and updated.json()['revision'] == 2
    loaded = (await service_client.get('/v1/quizzes/' + quiz, headers=auth_headers(user))).json()['quiz']
    assert [q['time_seconds'] for q in loaded['questions']] == [None, 90]
    assert loaded['questions'][0]['answers'][0]['is_correct'] is True
    assert 'image_key' not in loaded['questions'][0]
    listing = (await service_client.get('/v1/quizzes', headers=auth_headers(user))).json()
    assert listing['quizzes'][0]['id'] == quiz
    assert (await save(service_client, user, body, quiz)).status == 409
    cursor.execute('SELECT count(*) FROM quiz.questions WHERE quiz_id=%s', (quiz,))
    assert cursor.fetchone()[0] == 2


@pytest.mark.parametrize('mutation', ['empty_name', 'no_questions', 'no_answers', 'no_correct', 'two_single', 'empty_answer', 'zero_time', 'unknown_type'])
async def test_quiz_ready_validation(service_client, pgsql, mutation):
    _, user, uni = setup(pgsql)
    body = document(uni)
    if mutation == 'empty_name': body['name'] = '\u00a0\u2003'
    if mutation == 'no_questions': body['questions'] = []
    if mutation == 'no_answers': body['questions'][0]['answers'] = []
    if mutation == 'no_correct': body['questions'][0]['answers'][0]['is_correct'] = False
    if mutation == 'two_single': body['questions'][0]['answers'][1]['is_correct'] = True
    if mutation == 'empty_answer': body['questions'][0]['answers'][1]['text'] = ' '
    if mutation == 'zero_time': body['default_time_seconds'] = 0
    if mutation == 'unknown_type': body['questions'][0]['type'] = 'unexpected'
    response = await save(service_client, user, body)
    assert response.status == 400
    assert response.json()['details']


async def test_quiz_access_and_images(service_client, pgsql):
    cursor, user, uni = setup(pgsql)
    other = create_user(cursor)
    membership(cursor, other, uni, role='student')
    assert (await save(service_client, other, document(uni))).status == 403
    assert (await service_client.post('/v1/quizzes', json=document(uni))).status == 401
    image_id = str(uuid.uuid4())
    cursor.execute("INSERT INTO media.images(id,owner_id,storage_key,content_type,size_bytes,width,height,status,ready_at) VALUES(%s,%s,%s,'image/png',10,1,1,'ready',NOW())", (image_id, user, 'test/' + image_id))
    body = document(uni)
    body['questions'][0]['answers'][0].update(text='', image_id=image_id)
    created = await save(service_client, user, body)
    assert created.status == 201
    quiz = created.json()['quiz_id']
    membership(cursor, other, uni, role='teacher')
    assert (await service_client.get('/v1/quizzes/' + quiz, headers=auth_headers(other))).status == 404
    assert (await service_client.get('/v1/quizzes', headers=auth_headers(other))).json()['quizzes'] == []
    assert (await save(service_client, other, body)).status == 400
    body.update(revision=1, name='Не должно сохраниться')
    body['questions'][0]['image_id'] = str(uuid.uuid4())
    assert (await save(service_client, user, body, quiz)).status == 400
    stored = (await service_client.get('/v1/quizzes/' + quiz, headers=auth_headers(user))).json()['quiz']
    assert stored['revision'] == 1 and stored['name'] == 'Столицы'
    cursor.execute("UPDATE education.memberships SET status='inactive' WHERE user_id=%s", (user,))
    assert (await save(service_client, user, body, quiz)).status == 403
    assert (await service_client.get('/v1/quizzes/' + quiz, headers=auth_headers(user))).status == 404


async def test_quiz_concurrent_revision(service_client, pgsql):
    _, user, uni = setup(pgsql)
    body = document(uni)
    quiz = (await save(service_client, user, body)).json()['quiz_id']
    body['revision'] = 1
    replies = await asyncio.gather(save(service_client, user, body, quiz), save(service_client, user, body, quiz))
    assert sorted(r.status for r in replies) == [200, 409]


async def test_quiz_multy_and_empty_draft(service_client, pgsql):
    _, user, uni = setup(pgsql)
    assert (await save(service_client, user, {'university_id': uni, 'status': 'draft'})).status == 201
    body = document(uni)
    body['questions'][0]['type'] = 'multy'
    body['questions'][0]['answers'][1]['is_correct'] = True
    assert (await save(service_client, user, body)).status == 201
    body['revision'] = 1
    assert (await save(service_client, user, body)).status == 400


async def test_media_rejects_invalid_bytes(service_client, pgsql):
    import aiohttp
    _, user, _ = setup(pgsql)
    form = aiohttp.FormData()
    form.add_field('file', b'not an image', filename='fake.png', content_type='image/png')
    response = await service_client.post('/v1/media/images', data=form, headers=auth_headers(user))
    assert response.status == 415


async def test_media_valid_png_disabled_storage(service_client, pgsql):
    import aiohttp
    import base64
    _, user, _ = setup(pgsql)
    form = aiohttp.FormData()
    form.add_field('file', base64.b64decode('iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAwMCAO+aA1cAAAAASUVORK5CYII='), filename='pixel.png', content_type='image/png')
    response = await service_client.post('/v1/media/images', data=form, headers=auth_headers(user))
    assert response.status == 503
