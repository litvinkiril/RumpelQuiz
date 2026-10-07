import asyncio
import json
import uuid

import aiohttp

from test_game import game, join, read, action
from test_profile import auth_headers, create_user
from test_university_admins import membership


async def next_presence(stream):
    async def receive():
        while True:
            frame = (await stream.content.readuntil(b'\n\n')).decode()
            if frame.startswith('event: presence\n'):
                return json.loads(frame.split('data: ', 1)[1])
    return await asyncio.wait_for(receive(), 15)


async def test_presence_access_and_validation(service_client, pgsql):
    c, host, uni, quiz, sid, created = await game(service_client, pgsql)
    student = create_user(c)
    membership(c, student, uni, role='student')
    body = {'client_id': str(uuid.uuid4()), 'sequence': 1, 'online': True}
    assert (await action(service_client, student, sid, 'presence', body)).status == 403
    await join(service_client, student, created.json()['session']['join_code'])
    assert (await action(service_client, host, sid, 'presence', body)).status == 403
    for invalid in ({}, {**body, 'client_id': 'bad'}, {**body, 'sequence': -1},
                    {**body, 'sequence': 1.5}, {**body, 'online': 'true'}):
        assert (await action(service_client, student, sid, 'presence', invalid)).status == 400
    assert (await action(service_client, student, sid, 'presence', body)).status == 200
    assert (await read(service_client, host, sid)).json()['presence'] == [{'user_id': student, 'online': True}]
    assert 'presence' not in (await read(service_client, student, sid)).json()
    c.execute("UPDATE education.memberships SET status='inactive' WHERE user_id=%s", (student,))
    assert (await action(service_client, student, sid, 'presence', body)).status == 403
    c.execute("UPDATE education.memberships SET status='active' WHERE user_id=%s", (student,))
    await action(service_client, host, sid, 'close', {})
    assert (await action(service_client, student, sid, 'presence', body)).status == 409
    assert (await read(service_client, host, sid)).json()['presence'] == []


async def test_departure_and_return_reach_host_sse(service_client, service_baseurl, pgsql):
    c, host, uni, quiz, sid, created = await game(service_client, pgsql)
    student = create_user(c)
    membership(c, student, uni, role='student')
    await join(service_client, student, created.json()['session']['join_code'])
    body = {'client_id': str(uuid.uuid4()), 'sequence': 1, 'online': True}
    await action(service_client, student, sid, 'presence', body)
    url = service_baseurl.rstrip('/') + f'/v1/game/sessions/{sid}/events'
    async with aiohttp.ClientSession(timeout=aiohttp.ClientTimeout(total=30)) as client:
        async with client.get(url, headers=auth_headers(host)) as stream:
            initial = (await stream.content.readuntil(b'\n\n')).decode()
            assert initial.startswith('event: snapshot\n')
            assert json.loads(initial.split('data: ', 1)[1])['presence'][0]['online'] is True
            await action(service_client, student, sid, 'presence', {**body, 'sequence': 2, 'online': False})
            assert await next_presence(stream) == [{'user_id': student, 'online': False}]
            # An old heartbeat arriving late cannot restore a departed participant.
            await action(service_client, student, sid, 'presence', body)
            assert (await read(service_client, host, sid)).json()['presence'][0]['online'] is False
            await action(service_client, student, sid, 'presence', {**body, 'sequence': 3})
            assert await next_presence(stream) == [{'user_id': student, 'online': True}]
