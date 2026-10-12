import uuid

import pytest

from test_profile import auth_headers, create_user
from test_university_admins import membership, university
from test_university_structure import faculty, program


@pytest.fixture
def creation_context(pgsql):
    cursor = pgsql['db_1'].cursor()
    target = university(cursor, 'Университет')
    actor = create_user(cursor)
    membership(cursor, actor, target)
    return cursor, target, auth_headers(actor)


async def test_education_creation_chain_is_visible_in_catalog(service_client, creation_context):
    _, target, headers = creation_context
    faculty_response = await service_client.post(
        '/v1/education/faculties', headers=headers,
        json={'university_id': target, 'name': 'Компьютерные науки'},
    )
    assert faculty_response.status == 201
    faculty_id = faculty_response.json()['faculty_id']
    assert faculty_response.json() == {'success': True, 'faculty_id': faculty_id}

    program_response = await service_client.post(
        '/v1/education/programs', headers=headers,
        json={'university_id': target, 'name': 'Программная инженерия',
              'faculty_ids': [faculty_id]},
    )
    assert program_response.status == 201
    program_id = program_response.json()['program_id']
    assert program_response.json() == {'success': True, 'program_id': program_id}

    group_response = await service_client.post(
        '/v1/education/groups', headers=headers,
        json={'university_id': target, 'name': 'ПИ-2026-01', 'program_id': program_id},
    )
    assert group_response.status == 201
    group_id = group_response.json()['group_id']
    assert group_response.json() == {'success': True, 'group_id': group_id}

    for resource, filters, expected in [
        ('faculties', {'university_id': target},
         [{'id': faculty_id, 'name': 'Компьютерные науки'}]),
        ('programs', {'facultet_id': faculty_id},
         [{'id': program_id, 'name': 'Программная инженерия'}]),
        ('groups', {'university_id': target},
         [{'id': group_id, 'name': 'ПИ-2026-01'}]),
        ('groups', {'facultet_id': faculty_id},
         [{'id': group_id, 'name': 'ПИ-2026-01'}]),
        ('groups', {'programm_id': program_id},
         [{'id': group_id, 'name': 'ПИ-2026-01'}]),
    ]:
        response = await service_client.get(f'/v1/education/{resource}', params=filters)
        assert response.status == 200
        assert response.json() == {'success': True, resource: expected}


@pytest.mark.parametrize('kind,id_field,duplicate_error', [
    ('faculties', 'faculty_id', 'faculty_already_exists'),
    ('programs', 'program_id', 'program_already_exists'),
    ('groups', 'group_id', 'group_already_exists'),
])
async def test_education_creation_duplicate_names(
    service_client, creation_context, kind, id_field, duplicate_error,
):
    cursor, target, headers = creation_context
    payload = {'university_id': target, 'name': 'Название'}
    if kind != 'faculties':
        parent = faculty(cursor, target)
        if kind == 'programs':
            payload['faculty_ids'] = [parent]
        else:
            payload['program_id'] = program(cursor, target, parent)

    created = await service_client.post(f'/v1/education/{kind}', headers=headers, json=payload)
    assert created.status == 201
    uuid.UUID(created.json()[id_field])
    duplicate = await service_client.post(f'/v1/education/{kind}', headers=headers, json=payload)
    assert duplicate.status == 409
    assert duplicate.json() == {'success': False, 'error': duplicate_error}
    items = await service_client.get(f'/v1/education/{kind}', params={'university_id': target})
    assert len(items.json()[kind]) == 1


@pytest.mark.parametrize('kind,parent_case,expected_error', [
    ('programs', 'missing', 'invalid_request'),
    ('programs', 'empty', 'invalid_request'),
    ('programs', 'foreign', 'invalid_faculty'),
    ('groups', 'missing', 'invalid_request'),
    ('groups', 'empty', 'invalid_request'),
    ('groups', 'foreign', 'invalid_program'),
])
async def test_education_creation_requires_parent_in_university(
    service_client, creation_context, kind, parent_case, expected_error,
):
    cursor, target, headers = creation_context
    payload = {'university_id': target, 'name': 'Название'}
    if parent_case == 'empty':
        payload.update({'faculty_ids': []} if kind == 'programs' else {'program_id': ''})
    elif parent_case == 'foreign':
        other = university(cursor, 'Другой университет')
        other_faculty = faculty(cursor, other)
        payload.update(
            {'faculty_ids': [other_faculty]} if kind == 'programs'
            else {'program_id': program(cursor, other, other_faculty)},
        )
    response = await service_client.post(f'/v1/education/{kind}', headers=headers, json=payload)
    assert response.status == 400
    assert response.json() == {'success': False, 'error': expected_error}
    items = await service_client.get(f'/v1/education/{kind}', params={'university_id': target})
    assert items.json()[kind] == []
