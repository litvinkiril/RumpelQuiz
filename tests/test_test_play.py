import asyncio
import uuid

import pytest

from test_profile import auth_headers, create_user
from test_university_admins import membership, university
from test_tests import document, save, setup


async def prepare(client, pgsql, status='public', count=2):
    cursor, teacher, uni = setup(pgsql)
    student = create_user(cursor)
    membership(cursor, student, uni, role='student')
    body = document(uni, status)
    body['questions'] *= count
    test_id = (await save(client, teacher, body)).json()['test_id']
    return cursor, teacher, student, uni, test_id


async def start(client, student, test_id):
    return await client.post(f'/v1/tests/{test_id}/start', json={}, headers=auth_headers(student))


async def progress(client, student, test_id):
    return await client.get(f'/v1/tests/{test_id}/progress', headers=auth_headers(student))


async def answer(client, student, test_id, question, choices=None):
    return await client.post(f'/v1/tests/{test_id}/answers', json={
        'question_id': question['id'], 'answer_ids': choices if choices is not None else [question['answers'][0]['id']],
    }, headers=auth_headers(student))


@pytest.mark.parametrize('status', ['public', 'private'])
async def test_test_complete_resume_and_results(service_client, pgsql, status):
    cursor, teacher, student, uni, test_id = await prepare(service_client, pgsql, status)
    listing = (await service_client.get('/v1/tests/available', headers=auth_headers(student))).json()['tests']
    assert [t['id'] for t in listing] == ([test_id] if status == 'public' else [])
    state = (await start(service_client, student, test_id)).json()
    assert state['attempt']['status'] == 'in_progress'
    assert state['question_count'] == 2 and state['answered_count'] == 0
    assert state['attempt']['deadline_at_ms'] - state['attempt']['started_at_ms'] == 60000
    question = state['current_question']
    assert question['position'] == 0
    assert 'is_correct' not in question['answers'][0]
    assert 'time_seconds' not in question and 'image_key' not in question
    assert 'result' not in state
    next_state = (await answer(service_client, student, test_id, question)).json()
    assert next_state['current_question']['position'] == 1
    assert next_state['answered_count'] == 1
    resumed = (await start(service_client, student, test_id)).json()
    assert resumed['attempt'] == state['attempt']
    assert resumed['current_question'] == next_state['current_question']
    assert (await progress(service_client, student, test_id)).json()['current_question'] == next_state['current_question']
    listing = (await service_client.get('/v1/tests/available', headers=auth_headers(student))).json()['tests']
    assert listing[0]['id'] == test_id  # Private attempts become resumable from the student's list.
    finished = (await answer(service_client, student, test_id, resumed['current_question'])).json()
    assert finished['current_question'] is None and finished['attempt']['status'] == 'completed'
    assert finished['result']['score'] == 2 and finished['result']['answered_count'] == 2
    assert (await start(service_client, student, test_id)).json()['attempt'] == finished['attempt']
    report = await service_client.get(f'/v1/tests/{test_id}/results', headers=auth_headers(teacher))
    assert report.status == 200
    assert report.json()['results'][0]['student_id'] == student
    assert report.json()['results'][0]['score'] == 2
    assert (await service_client.get(f'/v1/tests/{test_id}/results', headers=auth_headers(student))).status == 404
    cursor.execute('SELECT count(*) FROM game.sessions')
    assert cursor.fetchone()[0] == 0
    cursor.execute('SELECT count(*) FROM test.test_in_process WHERE test_id=%s', (test_id,))
    assert cursor.fetchone()[0] == 1


async def test_test_concurrent_start_and_submission_replay(service_client, pgsql):
    cursor, _, student, _, test_id = await prepare(service_client, pgsql)
    starts = await asyncio.gather(*(start(service_client, student, test_id) for _ in range(3)))
    assert all(r.status == 200 for r in starts)
    assert len({r.json()['attempt']['id'] for r in starts}) == 1
    q = starts[0].json()['current_question']
    replies = await asyncio.gather(*(answer(service_client, student, test_id, q) for _ in range(3)))
    assert all(r.status == 200 for r in replies)
    assert all(r.json()['answered_count'] == 1 for r in replies)
    cursor.execute('SELECT count(*) FROM test.student_answers WHERE test_id=%s', (test_id,))
    assert cursor.fetchone()[0] == 1
    cursor.execute('SELECT submitted_at FROM test.student_answers WHERE test_id=%s', (test_id,))
    submitted_at = cursor.fetchone()[0]
    assert (await answer(service_client, student, test_id, q)).status == 200
    cursor.execute('SELECT submitted_at FROM test.student_answers WHERE test_id=%s', (test_id,))
    assert cursor.fetchone()[0] == submitted_at
    changed = await answer(service_client, student, test_id, q, [q['answers'][1]['id']])
    assert changed.status == 409 and changed.json()['error'] == 'test_answer_already_saved'


async def test_test_concurrent_different_answers_have_one_winner(service_client, pgsql):
    _, _, student, _, test_id = await prepare(service_client, pgsql)
    q = (await start(service_client, student, test_id)).json()['current_question']
    replies = await asyncio.gather(*(answer(service_client, student, test_id, q, [a['id']]) for a in q['answers']))
    assert sorted(r.status for r in replies) == [200, 409]
    assert (await progress(service_client, student, test_id)).json()['answered_count'] == 1


@pytest.mark.parametrize('operation', ['read', 'answer', 'teacher_results'])
async def test_test_expiry_is_persistent_and_cannot_restart(service_client, pgsql, operation):
    cursor, teacher, student, _, test_id = await prepare(service_client, pgsql)
    initial = (await start(service_client, student, test_id)).json()
    q = initial['current_question']
    cursor.execute("UPDATE test.test_in_process SET started_at=NOW()-interval '2 minutes',deadline_at=NOW()-interval '1 minute' WHERE test_id=%s", (test_id,))
    if operation == 'read':
        assert (await progress(service_client, student, test_id)).json()['attempt']['status'] == 'expired'
    elif operation == 'answer':
        response = await answer(service_client, student, test_id, q)
        assert response.status == 409 and response.json()['error'] == 'test_attempt_finished'
    else:
        results = (await service_client.get(f'/v1/tests/{test_id}/results', headers=auth_headers(teacher))).json()['results']
        assert results[0]['status'] == 'expired'
    cursor.execute('SELECT status FROM test.test_in_process WHERE test_id=%s', (test_id,))
    assert cursor.fetchone()[0] == 'expired'
    state = (await start(service_client, student, test_id)).json()
    assert state['attempt']['id'] == initial['attempt']['id']
    assert state['current_question'] is None and state['result']['score'] == 0
    cursor.execute('SELECT count(*) FROM test.student_answers WHERE test_id=%s', (test_id,))
    assert cursor.fetchone()[0] == 0


async def test_test_access_author_student_university_and_revocation(service_client, pgsql):
    cursor, teacher, student, uni, test_id = await prepare(service_client, pgsql, 'private')
    outsider = create_user(cursor)
    other_uni = university(cursor, 'Other university')
    membership(cursor, outsider, other_uni, role='student')
    assert (await start(service_client, outsider, test_id)).status == 403
    assert (await start(service_client, teacher, test_id)).status == 403
    assert (await service_client.post(f'/v1/tests/{test_id}/start', json={})).status == 401
    assert (await service_client.get(f'/v1/tests/{test_id}', headers=auth_headers(student))).status == 404
    assert (await progress(service_client, student, test_id)).json()['error'] == 'test_not_started'
    assert (await start(service_client, student, test_id)).status == 200
    cursor.execute("UPDATE education.memberships SET status='inactive' WHERE user_id=%s", (student,))
    assert (await progress(service_client, student, test_id)).status == 403
    assert (await service_client.get('/v1/tests/available', headers=auth_headers(student))).json()['tests'] == []
    membership(cursor, outsider, uni, role='teacher')
    assert (await service_client.get(f'/v1/tests/{test_id}/results', headers=auth_headers(outsider))).status == 404
    draft = (await save(service_client, teacher, document(uni, 'draft'))).json()['test_id']
    assert (await start(service_client, student, draft)).status == 404


@pytest.mark.parametrize('bad', ['empty', 'duplicate', 'two_single', 'foreign', 'future'])
async def test_test_invalid_answer_does_not_advance(service_client, pgsql, bad):
    cursor, _, student, _, test_id = await prepare(service_client, pgsql)
    state = (await start(service_client, student, test_id)).json()
    q = state['current_question']
    choices = [q['answers'][0]['id']]
    if bad == 'empty': choices = []
    if bad == 'duplicate': choices *= 2
    if bad == 'two_single': choices = [a['id'] for a in q['answers']]
    if bad == 'foreign': choices = [str(uuid.uuid4())]
    if bad == 'future':
        cursor.execute('SELECT id FROM test.questions WHERE test_id=%s AND position=1', (test_id,))
        q = dict(q, id=str(cursor.fetchone()[0]))
    response = await answer(service_client, student, test_id, q, choices)
    assert response.status == (409 if bad == 'future' else 400)
    stored = (await progress(service_client, student, test_id)).json()
    assert stored['attempt'] == state['attempt'] and stored['current_question'] == state['current_question']
    assert stored['answered_count'] == 0


@pytest.mark.parametrize('choices,score', [([0], 0), ([0, 1], 1), ([0, 1, 2], 0), ([2], 0)])
async def test_test_multiple_answers_score_exact_set(service_client, pgsql, choices, score):
    cursor, teacher, uni = setup(pgsql)
    student = create_user(cursor)
    membership(cursor, student, uni, role='student')
    body = document(uni)
    body['questions'][0]['type'] = 'multy'
    body['questions'][0]['answers'][1]['is_correct'] = True
    body['questions'][0]['answers'].append({'text': 'Лондон', 'is_correct': False})
    test_id = (await save(service_client, teacher, body)).json()['test_id']
    q = (await start(service_client, student, test_id)).json()['current_question']
    picked = [q['answers'][i]['id'] for i in choices]
    response = await answer(service_client, student, test_id, q, picked)
    assert response.status == 200 and response.json()['result']['score'] == score
    assert (await answer(service_client, student, test_id, q, list(reversed(picked)))).status == 200


async def test_test_student_progress_is_independent(service_client, pgsql):
    cursor, _, student, uni, test_id = await prepare(service_client, pgsql)
    other = create_user(cursor)
    membership(cursor, other, uni, role='student')
    first = (await start(service_client, student, test_id)).json()
    second = (await start(service_client, other, test_id)).json()
    assert first['attempt']['id'] != second['attempt']['id']
    await answer(service_client, student, test_id, first['current_question'])
    assert (await progress(service_client, other, test_id)).json()['answered_count'] == 0
    assert (await progress(service_client, student, test_id)).json()['answered_count'] == 1
