import asyncio
import json
import uuid
from contextlib import AsyncExitStack

import aiohttp
import pytest

from test_game import game, read, action, join
from test_quizzes import setup, document, save
from test_profile import auth_headers, create_user
from test_university_admins import university, membership


async def history(client, user, quiz):
    return await client.get(f'/v1/quizzes/{quiz}/sessions', headers=auth_headers(user))


async def results(client, user, sid):
    return await client.get(f'/v1/game/sessions/{sid}/results', headers=auth_headers(user))


async def test_history_filters_orders_and_reports_metadata(service_client, pgsql):
    c, host, uni, quiz, sid, created = await game(service_client, pgsql)
    assert (await history(service_client, host, quiz)).json()['sessions'] == []
    assert (await results(service_client, host, sid)).status == 409
    student = create_user(c)
    membership(c, student, uni, role='student')
    await join(service_client, student, created.json()['session']['join_code'])
    await action(service_client, host, sid, 'next', {'expected_question_id': None})
    assert (await history(service_client, host, quiz)).json()['sessions'] == []
    await action(service_client, host, sid, 'close', {})
    second = str(uuid.uuid4())
    await service_client.post('/v1/game/sessions', json={
        'quiz_id': quiz, 'session_id': second, 'name': 'Отменённая'}, headers=auth_headers(host))
    await action(service_client, host, second, 'close', {})
    c.execute("UPDATE game.sessions SET created_at=NOW()-interval '1 day' WHERE id=%s", (sid,))
    c.execute("INSERT INTO users.profiles(user_id,last_name,first_name,middle_name) VALUES(%s,' Иванов ',' Иван ',' Иванович ')", (host,))
    response = await history(service_client, host, quiz)
    assert response.status == 200
    assert response.headers['Cache-Control'] == 'no-store'
    sessions = response.json()['sessions']
    assert [s['session_id'] for s in sessions] == [second, sid]
    assert [s['status'] for s in sessions] == ['cancelled', 'finished']
    assert sessions[1]['participants_count'] == 1
    assert sessions[1]['name'] == 'Практика 3'
    assert sessions[1]['host_name'] == 'Иванов Иван Иванович'
    assert isinstance(sessions[1]['created_at_ms'], int)
    assert sessions[1]['question_count'] == 2
    assert (await results(service_client, host, second)).json()['results'] == []


async def test_results_exact_sets_and_identical_visibility(service_client, pgsql):
    c, host, uni = setup(pgsql)
    body = document(uni)
    body['questions'].append({**body['questions'][0], 'type': 'multy', 'answers': [
        {'text': 'A', 'is_correct': True}, {'text': 'B', 'is_correct': True},
        {'text': 'C', 'is_correct': False},
    ]})
    quiz = (await save(service_client, host, body)).json()['quiz_id']
    sid = str(uuid.uuid4())
    created = await service_client.post('/v1/game/sessions', json={
        'quiz_id': quiz, 'session_id': sid, 'name': 'Баллы'}, headers=auth_headers(host))
    students = [create_user(c) for _ in range(5)]
    for student in students:
        membership(c, student, uni, role='student')
        await join(service_client, student, created.json()['session']['join_code'])
    await action(service_client, host, sid, 'next', {'expected_question_id': None})
    q = (await read(service_client, host, sid)).json()['current_question']
    for i, student in enumerate(students[:4]):
        assert (await action(service_client, student, sid, 'answers', {
            'question_id': q['id'], 'answer_ids': [q['answers'][0 if i < 3 else 1]['id']],
        })).status == 200
    await action(service_client, host, sid, 'next', {'expected_question_id': q['id']})
    q = (await read(service_client, host, sid)).json()['current_question']
    for student, choices in zip(students, ([0, 1], [0], [0, 1, 2], [2])):
        assert (await action(service_client, student, sid, 'answers', {
            'question_id': q['id'], 'answer_ids': [q['answers'][i]['id'] for i in choices],
        })).status == 200
    assert (await results(service_client, host, sid)).json()['error'] == 'session_not_finished'
    assert (await read(service_client, students[0], sid)).json()['results'] == []
    await action(service_client, host, sid, 'close', {})
    response = await results(service_client, host, sid)
    assert response.status == 200
    expected = response.json()['results']
    assert [r['score'] for r in expected] == [2, 1, 1, 0, 0]
    by_user = {r['user_id']: r for r in expected}
    for student, score in zip(students, [2, 1, 1, 0, 0]):
        assert by_user[student]['score'] == by_user[student]['correct_count'] == score
        assert by_user[student]['question_count'] == 2
        assert by_user[student]['answered_count'] == (0 if student == students[-1] else 2)
        assert (await results(service_client, student, sid)).json()['results'] == expected
        assert (await read(service_client, student, sid)).json()['results'] == expected
    await action(service_client, host, sid, 'close', {})
    assert (await results(service_client, host, sid)).json()['results'] == expected


async def test_history_and_results_access_validation(service_client, pgsql):
    c, host, uni, quiz, sid, created = await game(service_client, pgsql)
    await action(service_client, host, sid, 'close', {})
    for path in (f'/v1/quizzes/{quiz}/sessions', f'/v1/game/sessions/{sid}/results'):
        assert (await service_client.get(path)).status == 401
    for bad in ('bad', str(uuid.UUID(int=0))):
        assert (await history(service_client, host, bad)).status == 400
        assert (await results(service_client, host, bad)).status == 400
    assert (await history(service_client, host, str(uuid.uuid4()))).status == 404
    assert (await results(service_client, host, str(uuid.uuid4()))).status == 404
    for role in ('teacher', 'admin', 'student'):
        user = create_user(c)
        membership(c, user, uni, role=role)
        expected_status = 404 if role == 'student' else 200
        assert (await history(service_client, user, quiz)).status == expected_status
        assert (await results(service_client, user, sid)).status == expected_status
        c.execute("UPDATE education.memberships SET status='inactive' WHERE user_id=%s", (user,))
        assert (await history(service_client, user, quiz)).status == 404
        assert (await results(service_client, user, sid)).status == 404
    outsider = create_user(c)
    membership(c, outsider, university(c, 'Другой вуз'), role='admin')
    assert (await history(service_client, outsider, quiz)).status == 404
    assert (await results(service_client, outsider, sid)).status == 404
    participant = create_user(c)
    membership(c, participant, uni, role='student')
    c.execute('INSERT INTO game.participants(session_id,user_id) VALUES(%s,%s)', (sid, participant))
    assert (await results(service_client, participant, sid)).status == 200
    c.execute("UPDATE education.memberships SET status='inactive' WHERE user_id=%s", (participant,))
    assert (await results(service_client, participant, sid)).status == 404


async def event(response):
    frame = await asyncio.wait_for(response.content.readuntil(b'\n\n'), timeout=5)
    lines = frame.decode().splitlines()
    return lines[0].removeprefix('event: '), json.loads(lines[1].removeprefix('data: '))


@pytest.mark.parametrize('started', [False, True])
async def test_sse_results_are_last_event_then_eof(service_client, service_baseurl, pgsql, started):
    c, host, uni, quiz, sid, created = await game(service_client, pgsql)
    student = create_user(c)
    membership(c, student, uni, role='student')
    await join(service_client, student, created.json()['session']['join_code'])
    if started:
        await action(service_client, host, sid, 'next', {'expected_question_id': None})
        q = (await read(service_client, student, sid)).json()['current_question']
        await action(service_client, student, sid, 'answers', {
            'question_id': q['id'], 'answer_ids': [q['answers'][0]['id']],
        })
    url = service_baseurl.rstrip('/') + f'/v1/game/sessions/{sid}/events'
    async with aiohttp.ClientSession(timeout=aiohttp.ClientTimeout(total=15)) as client, AsyncExitStack() as stack:
        streams = []
        for user in (host, student):
            stream = await stack.enter_async_context(client.get(url, headers=auth_headers(user)))
            assert stream.status == 200
            assert (await event(stream))[0] == 'snapshot'
            streams.append(stream)
        await action(service_client, host, sid, 'close', {})
        expected = (await results(service_client, host, sid)).json()['results']
        assert expected[0]['score'] == (1 if started else 0)
        for stream in streams:
            kind, payload = await event(stream)
            assert kind == 'results'
            assert payload['results'] == expected
            assert payload['session']['status'] == ('finished' if started else 'cancelled')
            assert await asyncio.wait_for(stream.content.read(), timeout=5) == b''
        # A reconnect after a lost final event receives the same results and EOF.
        async with client.get(url, headers=auth_headers(student)) as stream:
            kind, payload = await event(stream)
            assert kind == 'results' and payload['results'] == expected
            assert await asyncio.wait_for(stream.content.read(), timeout=5) == b''
