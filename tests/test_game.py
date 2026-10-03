import asyncio
import copy
import uuid
from pathlib import Path

import pytest

from test_quizzes import setup, document, save
from test_profile import auth_headers, create_user
from test_university_admins import university, membership


async def game(client, pgsql, status='ready'):
    cursor, host, uni = setup(pgsql)
    body = document(uni, status)
    body['questions'].append(copy.deepcopy(body['questions'][0]))
    body['questions'][1]['type'] = 'multy'
    quiz = (await save(client, host, body)).json()['quiz_id']
    sid = str(uuid.uuid4())
    created = await client.post('/v1/game/sessions', json={'quiz_id': quiz, 'session_id': sid, 'name': 'Практика 3'}, headers=auth_headers(host))
    return cursor, host, uni, quiz, sid, created


async def read(client, user, sid):
    return await client.get('/v1/game/sessions/' + sid, headers=auth_headers(user))


async def action(client, user, sid, name, body):
    return await client.post('/v1/game/sessions/' + sid + '/' + name, json=body, headers=auth_headers(user))


async def join(client, user, code):
    return await client.post('/v1/game/sessions/join', json={'code': code}, headers=auth_headers(user))


async def test_game_lifecycle_and_retries(service_client, pgsql):
    c, host, uni, quiz, sid, created = await game(service_client, pgsql)
    assert created.status == 200, created.text
    room = created.json()['session']
    assert len(room['join_code']) == 6 and room['join_code'].isdigit()
    again = await service_client.post('/v1/game/sessions', json={'quiz_id': quiz, 'session_id': sid, 'name': 'Практика 3'}, headers=auth_headers(host))
    assert again.json() == created.json()
    assert (await read(service_client, host, sid)).json()['current_question'] is None
    student = create_user(c)
    membership(c, student, uni, role='student')
    assert (await join(service_client, student, room['join_code'])).status == 200
    assert (await join(service_client, student, room['join_code'])).status == 200
    assert (await read(service_client, host, sid)).json()['session']['participants_count'] == 1
    first = await action(service_client, host, sid, 'next', {'expected_question_id': None})
    assert first.status == 200, first.text
    qid = first.json()['question_id']
    stale = await action(service_client, host, sid, 'next', {'expected_question_id': None})
    assert stale.status == 409 and stale.json()['current_question_id'] == qid
    state = (await read(service_client, student, sid)).json()
    assert 'is_correct' not in str(state) and 'image_key' not in str(state)
    assert state['participants'] == [] and state['results'] == []
    q = state['current_question']
    answer = {'question_id': q['id'], 'answer_ids': [q['answers'][0]['id']]}
    sent = await action(service_client, student, sid, 'answers', answer)
    assert sent.status == 200, sent.text
    c.execute('SELECT submitted_at FROM game.submissions WHERE session_id=%s', (sid,))
    timestamp = c.fetchone()[0]
    assert (await action(service_client, student, sid, 'answers', answer)).status == 200
    assert (await read(service_client, student, sid)).json()['current_question']['submitted']
    answer2 = {**answer, 'answer_ids': [q['answers'][1]['id']]}
    assert (await action(service_client, student, sid, 'answers', answer2)).status == 409
    second = await action(service_client, host, sid, 'next', {'expected_question_id': qid})
    assert second.status == 200 and not second.json()['has_next']
    assert (await action(service_client, host, sid, 'next', {'expected_question_id': second.json()['question_id']})).json()['error'] == 'no_more_questions'
    closed = await action(service_client, host, sid, 'close', {})
    assert closed.status == 200 and closed.json()['session']['status'] == 'finished'
    assert (await action(service_client, host, sid, 'close', {})).json() == closed.json()
    assert (await action(service_client, student, sid, 'answers', answer)).status == 200
    c.execute('SELECT submitted_at FROM game.submissions WHERE session_id=%s', (sid,))
    assert c.fetchone()[0] == timestamp
    assert (await read(service_client, host, sid)).json()['results'][0]['answered_count'] == 1
    assert (await join(service_client, student, room['join_code'])).status == 404


async def test_game_access_and_validation(service_client, pgsql):
    c, host, uni, quiz, sid, created = await game(service_client, pgsql)
    stranger = create_user(c)
    other_uni = university(c, 'Other game university')
    membership(c, stranger, other_uni, role='student')
    assert (await join(service_client, stranger, created.json()['session']['join_code'])).status == 403
    assert (await read(service_client, stranger, sid)).status == 403
    assert (await action(service_client, stranger, sid, 'next', {'expected_question_id': None})).status == 403
    assert (await action(service_client, stranger, sid, 'close', {})).status == 403
    assert (await service_client.get('/v1/game/sessions/' + sid)).status == 401
    for body in ({}, [], {'expected_question_id': 123}, {'expected_question_id': str(uuid.UUID(int=0))}):
        assert (await action(service_client, host, sid, 'next', body)).status == 400
    assert (await action(service_client, host, sid, 'close', {'unexpected': True})).status == 400
    assert (await join(service_client, stranger, '<bad>')).status == 400
    assert (await read(service_client, host, 'bad-id')).status == 400
    closed = await action(service_client, host, sid, 'close', {})
    assert closed.json()['session']['status'] == 'cancelled'
    assert (await action(service_client, host, sid, 'next', {'expected_question_id': None})).status == 409


async def test_game_draft_and_revoked_host(service_client, pgsql):
    c, host, uni, quiz, sid, response = await game(service_client, pgsql, 'draft')
    assert response.status == 409 and response.json()['error'] == 'quiz_not_ready'
    c, host, uni, quiz, sid, response = await game(service_client, pgsql)
    c.execute("UPDATE education.memberships SET status='inactive' WHERE user_id=%s", (host,))
    assert (await read(service_client, host, sid)).status == 403
    assert (await action(service_client, host, sid, 'close', {})).status == 403


async def test_game_concurrent_create_next_and_answer(service_client, pgsql):
    c, host, uni, quiz, sid, created = await game(service_client, pgsql)
    new_id = str(uuid.uuid4())
    replies = await asyncio.gather(*[service_client.post('/v1/game/sessions', json={'quiz_id': quiz, 'session_id': new_id, 'name': 'Практика 3'}, headers=auth_headers(host)) for _ in range(2)])
    assert [r.status for r in replies] == [200, 200]
    assert replies[0].json() == replies[1].json()
    replies = await asyncio.gather(*[action(service_client, host, sid, 'next', {'expected_question_id': None}) for _ in range(2)])
    assert sorted(r.status for r in replies) == [200, 409]
    student = create_user(c); membership(c, student, uni, role='student')
    await join(service_client, student, created.json()['session']['join_code'])
    q = (await read(service_client, student, sid)).json()['current_question']
    replies = await asyncio.gather(*[action(service_client, student, sid, 'answers', {'question_id': q['id'], 'answer_ids': [a['id']]}) for a in q['answers']])
    assert sorted(r.status for r in replies) == [200, 409]
    c.execute('SELECT count(*) FROM game.submissions WHERE session_id=%s', (sid,))
    assert c.fetchone()[0] == 1


async def test_game_answer_deadline_and_membership(service_client, pgsql):
    c, host, uni, quiz, sid, created = await game(service_client, pgsql)
    student = create_user(c); membership(c, student, uni, role='student')
    await join(service_client, student, created.json()['session']['join_code'])
    await action(service_client, host, sid, 'next', {'expected_question_id': None})
    q = (await read(service_client, student, sid)).json()['current_question']
    assert (await action(service_client, student, sid, 'answers', {'question_id': q['id'], 'answer_ids': [a['id'] for a in q['answers']]})).status == 400
    assert (await action(service_client, student, sid, 'answers', {'question_id': q['id'], 'answer_ids': [str(uuid.uuid4())]})).status == 400
    c.execute("UPDATE game.session_questions SET opened_at=NOW()-interval '2 minutes',deadline_at=NOW()-interval '1 minute' WHERE session_id=%s", (sid,))
    answer = {'question_id': q['id'], 'answer_ids': [q['answers'][0]['id']]}
    assert (await action(service_client, student, sid, 'answers', answer)).json()['error'] == 'question_closed'
    assert not (await read(service_client, student, sid)).json()['current_question']['accepting_answers']
    c.execute("UPDATE education.memberships SET status='inactive' WHERE user_id=%s", (student,))
    assert (await action(service_client, student, sid, 'answers', answer)).status == 403


def test_game_schema_matches_fresh_database():
    root = Path(__file__).parents[1] / 'postgresql'
    embedded = (root / 'schemas/db_1.sql').read_text().split('-- BEGIN GAME SCHEMA\n')[1].split('-- END GAME SCHEMA')[0]
    assert embedded.strip() == (root / 'game_schema.sql').read_text().strip()


async def test_game_session_name_validation(service_client, pgsql):
    c, host, uni, quiz, sid, created = await game(service_client, pgsql)
    assert created.json()['session']['name'] == 'Практика 3'
    assert (await read(service_client, host, sid)).json()['session']['name'] == 'Практика 3'
    for value in (None, 1, [], {}, '', '  ', '\u00a0\u2003', 'я' * 201, 'a\0b'):
        response = await service_client.post('/v1/game/sessions', json={
            'quiz_id': quiz, 'session_id': str(uuid.uuid4()), 'name': value,
        }, headers=auth_headers(host))
        assert response.status == 400, response.text
    missing = await service_client.post('/v1/game/sessions', json={
        'quiz_id': quiz, 'session_id': str(uuid.uuid4()),
    }, headers=auth_headers(host))
    assert missing.status == 400
    conflict = await service_client.post('/v1/game/sessions', json={
        'quiz_id': quiz, 'session_id': sid, 'name': 'Другая сессия',
    }, headers=auth_headers(host))
    assert conflict.status == 409
    for name in ('Практика 3', 'я' * 200, '😀' * 200):
        response = await service_client.post('/v1/game/sessions', json={
            'quiz_id': quiz, 'session_id': str(uuid.uuid4()), 'name': '\u00a0  ' + name + '  ',
        }, headers=auth_headers(host))
        assert response.status == 200, response.text
        assert response.json()['session']['name'] == name


async def test_game_session_name_migration(service_client, pgsql):
    c, host, uni, quiz, sid, created = await game(service_client, pgsql)
    c.execute('ALTER TABLE game.sessions DROP COLUMN name')
    migration = (Path(__file__).parents[1] / 'postgresql/migrations/006_game_session_names.sql').read_text()
    c.execute(migration)
    c.execute(migration)
    c.execute('SELECT name FROM game.sessions WHERE id=%s', (sid,))
    assert c.fetchone()[0] == 'Сессия ' + sid
