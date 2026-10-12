import uuid

import pytest

from test_education_structure_api import structure, study_group
from test_university_admins import university
from test_university_structure import faculty, program


@pytest.mark.parametrize('resource', ['programs', 'groups'])
async def test_public_search(service_client, structure, resource):
    response = await service_client.get(
        f'/v1/education/search/{resource}',
        params={'university_id': structure['university'], 'name': '  sHaReD  ' if resource == 'programs' else 'DELt'},
    )
    assert response.status == 200
    assert response.headers['Cache-Control'] == 'no-store'
    expected = {
        'id': structure['shared_program'] if resource == 'programs' else structure['groups'][2]['id'],
        'name': 'Shared Program' if resource == 'programs' else 'Delta Group',
        'facultet_names': ['Alpha Faculty', 'Beta Faculty'],
    }
    if resource == 'groups':
        expected['program_name'] = 'Shared Program'
    assert response.json() == {'success': True, resource: [expected]}


async def test_unassigned_group(service_client, structure):
    response = await service_client.get(
        '/v1/education/search/groups',
        params={'university_id': structure['university'], 'name': 'unassigned'},
    )
    assert response.status == 200
    assert response.json() == {
        'success': True,
        'groups': [{
            **structure['groups'][3],
            'facultet_names': [],
            'program_name': None,
        }],
    }


@pytest.mark.parametrize('resource', ['programs', 'groups'])
@pytest.mark.parametrize('params,error', [
    ({'name': 'x'}, 'invalid_university_id'),
    ({'university_id': 'bad', 'name': 'x'}, 'invalid_university_id'),
    ({'university_id': str(uuid.uuid4())}, 'invalid_name'),
    ({'university_id': str(uuid.uuid4()), 'name': ''}, 'invalid_name'),
    ({'university_id': str(uuid.uuid4()), 'name': ' \t\r\n'}, 'invalid_name'),
])
async def test_invalid_search(service_client, resource, params, error):
    response = await service_client.get(f'/v1/education/search/{resource}', params=params)
    assert response.status == 400
    assert response.json() == {'success': False, 'error': error}


@pytest.mark.parametrize('resource', ['programs', 'groups'])
@pytest.mark.parametrize('name', ['Alpha', 'missing'])
async def test_search_university_scope(service_client, structure, resource, name):
    response = await service_client.get(
        f'/v1/education/search/{resource}',
        params={'university_id': structure['university'], 'name': name},
    )
    assert response.status == 200
    items = response.json()[resource]
    assert response.json()['success'] is True
    assert len(items) == (1 if name == 'Alpha' else 0)
    if items:
        assert items[0]['id'] == structure[resource][0]['id']


@pytest.mark.parametrize('resource', ['programs', 'groups'])
async def test_unknown_university(service_client, resource):
    response = await service_client.get(
        f'/v1/education/search/{resource}',
        params={'university_id': str(uuid.uuid4()), 'name': 'a'},
    )
    assert response.status == 200
    assert response.json() == {'success': True, resource: []}


@pytest.mark.parametrize('resource', ['programs', 'groups'])
@pytest.mark.parametrize('count', [30, 31])
async def test_search_limit(service_client, pgsql, resource, count):
    cursor = pgsql['db_1'].cursor()
    uni = university(cursor, 'Search limit')
    fac = faculty(cursor, uni)
    shared = program(cursor, uni, fac, 'Shared')
    for index in range(count):
        if resource == 'programs':
            program(cursor, uni, fac, f'Match {index:02d}')
        else:
            study_group(cursor, uni, f'Match {index:02d}', shared)
    response = await service_client.get(
        f'/v1/education/search/{resource}',
        params={'university_id': uni, 'name': 'match'},
    )
    assert response.status == 200
    if count == 31:
        assert response.json() == {
            'success': False,
            'error': 'too_many_results',
            'message': 'Введите более точное название',
            resource: [],
        }
    else:
        assert response.json()['success'] is True
        assert len(response.json()[resource]) == 30
        assert [item['name'] for item in response.json()[resource]] == [
            f'Match {index:02d}' for index in range(count)
        ]


@pytest.mark.parametrize('resource', ['programs', 'groups'])
@pytest.mark.parametrize('needle', ['%', '_', '!', "x' OR TRUE --", 'инФорМ'])
async def test_search_literal_input(service_client, pgsql, resource, needle):
    cursor = pgsql['db_1'].cursor()
    uni = university(cursor, 'Literal search')
    fac = faculty(cursor, uni)
    course = program(cursor, uni, fac, f'Прикладная информатика {needle}')
    group = study_group(cursor, uni, f'Группа информатики {needle}', course)
    program(cursor, uni, fac, 'Unrelated')
    study_group(cursor, uni, 'Unrelated', course)
    response = await service_client.get(
        f'/v1/education/search/{resource}',
        params={'university_id': uni, 'name': needle},
    )
    assert response.status == 200
    assert response.json()['success'] is True
    assert [item['id'] for item in response.json()[resource]] == [
        course if resource == 'programs' else group['id'],
    ]


@pytest.mark.parametrize('resource', ['programs', 'groups'])
async def test_same_names_keep_distinct_ids(service_client, pgsql, resource):
    cursor = pgsql['db_1'].cursor()
    uni = university(cursor, 'Duplicates')
    ids = []
    for index in range(2):
        fac = faculty(cursor, uni, f'Faculty {index}')
        course = program(cursor, uni, fac, 'Same Program')
        group = study_group(cursor, uni, 'Same Group', course)
        ids.append(course if resource == 'programs' else group['id'])
    response = await service_client.get(
        f'/v1/education/search/{resource}',
        params={'university_id': uni, 'name': 'Same'},
    )
    assert response.status == 200
    assert {item['id'] for item in response.json()[resource]} == set(ids)
    assert {tuple(item['facultet_names']) for item in response.json()[resource]} == {
        ('Faculty 0',), ('Faculty 1',),
    }
