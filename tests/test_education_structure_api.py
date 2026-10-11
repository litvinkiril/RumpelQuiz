import uuid

import pytest

from test_university_admins import university
from test_university_structure import faculty, program


FILTERS = [
    ('faculties', 'university_id'),
    ('programs', 'university_id'),
    ('programs', 'facultet_id'),
    ('groups', 'university_id'),
    ('groups', 'facultet_id'),
    ('groups', 'programm_id'),
]


def study_group(cursor, university_id, name, program_id=None):
    cursor.execute(
        'INSERT INTO education.study_groups (university_id, name, program_id) '
        'VALUES (%s, %s, %s) RETURNING id',
        (university_id, name, program_id),
    )
    return {'id': str(cursor.fetchone()[0]), 'name': name}


@pytest.fixture
def structure(pgsql):
    cursor = pgsql['db_1'].cursor()
    target = university(cursor, 'Target')
    other = university(cursor, 'Other')

    # Insert out of order to check that the API sorts by name.
    beta = faculty(cursor, target, 'Beta Faculty')
    alpha = faculty(cursor, target, 'Alpha Faculty')
    empty = faculty(cursor, target, 'Empty Faculty')
    foreign_faculty = faculty(cursor, other, 'Alpha Faculty')

    zeta_program = program(cursor, target, beta, 'Zeta Program')
    shared_program = program(cursor, target, alpha, 'Shared Program')
    alpha_program = program(cursor, target, alpha, 'Alpha Program')
    foreign_program = program(cursor, other, foreign_faculty, 'Alpha Program')
    cursor.execute(
        'INSERT INTO education.faculty_programs '
        '(faculty_id, program_id, university_id) VALUES (%s, %s, %s)',
        (beta, shared_program, target),
    )

    zeta_group = study_group(cursor, target, 'Zeta Group', zeta_program)
    delta_group = study_group(cursor, target, 'Delta Group', shared_program)
    alpha_group = study_group(cursor, target, 'Alpha Group', alpha_program)
    beta_group = study_group(cursor, target, 'Beta Group', shared_program)
    unassigned = study_group(cursor, target, 'Unassigned Group')
    study_group(cursor, other, 'Alpha Group', foreign_program)

    alpha_item = {'id': alpha_program, 'name': 'Alpha Program'}
    shared_item = {'id': shared_program, 'name': 'Shared Program'}
    zeta_item = {'id': zeta_program, 'name': 'Zeta Program'}

    return {
        'university': target,
        'alpha_faculty': alpha,
        'beta_faculty': beta,
        'shared_program': shared_program,
        'faculties': [
            {'id': alpha, 'name': 'Alpha Faculty'},
            {'id': beta, 'name': 'Beta Faculty'},
            {'id': empty, 'name': 'Empty Faculty'},
        ],
        'programs': [alpha_item, shared_item, zeta_item],
        'alpha_programs': [alpha_item, shared_item],
        'beta_programs': [shared_item, zeta_item],
        'groups': [alpha_group, beta_group, delta_group, unassigned, zeta_group],
        'alpha_groups': [alpha_group, beta_group, delta_group],
        'beta_groups': [beta_group, delta_group, zeta_group],
        'shared_groups': [beta_group, delta_group],
    }


@pytest.mark.parametrize('resource,filter_name,target_key,expected_key', [
    ('faculties', 'university_id', 'university', 'faculties'),
    ('programs', 'university_id', 'university', 'programs'),
    ('programs', 'facultet_id', 'alpha_faculty', 'alpha_programs'),
    ('programs', 'facultet_id', 'beta_faculty', 'beta_programs'),
    ('groups', 'university_id', 'university', 'groups'),
    ('groups', 'facultet_id', 'alpha_faculty', 'alpha_groups'),
    ('groups', 'facultet_id', 'beta_faculty', 'beta_groups'),
    ('groups', 'programm_id', 'shared_program', 'shared_groups'),
])
async def test_structure_lists_are_public_and_scoped(
    service_client, structure, resource, filter_name, target_key, expected_key,
):
    # No JWT or membership is required to view a university's structure.
    response = await service_client.get(
        f'/v1/education/{resource}',
        params={filter_name: structure[target_key]},
    )

    assert response.status == 200
    assert response.headers['Cache-Control'] == 'no-store'
    assert response.json() == {
        'success': True,
        resource: structure[expected_key],
    }


@pytest.mark.parametrize('resource,filter_name', FILTERS)
@pytest.mark.parametrize('target_exists', [False, True])
async def test_structure_empty_lists(
    service_client, pgsql, resource, filter_name, target_exists,
):
    target = str(uuid.uuid4())

    if target_exists:
        cursor = pgsql['db_1'].cursor()
        target = university(cursor, 'Empty University')
        if filter_name in ('facultet_id', 'programm_id'):
            university_id = target
            target = faculty(cursor, university_id)
            if filter_name == 'programm_id':
                target = program(cursor, university_id, target)

    response = await service_client.get(
        f'/v1/education/{resource}', params={filter_name: target},
    )

    assert response.status == 200
    assert response.json() == {'success': True, resource: []}


@pytest.mark.parametrize('resource,filter_name', FILTERS)
@pytest.mark.parametrize('value', ['', 'not-a-uuid', '1234', 'g' * 36])
async def test_structure_invalid_uuid(service_client, resource, filter_name, value):
    response = await service_client.get(
        f'/v1/education/{resource}', params={filter_name: value},
    )

    assert response.status == 400
    assert response.json() == {
        'success': False,
        'error': f'invalid_{filter_name}',
    }


@pytest.mark.parametrize('resource,filter_names', [
    ('faculties', []),
    ('faculties', ['facultet_id']),
    ('faculties', ['programm_id']),
    ('programs', []),
    ('programs', ['programm_id']),
    ('programs', ['university_id', 'facultet_id']),
    ('groups', []),
    ('groups', ['university_id', 'facultet_id']),
    ('groups', ['university_id', 'programm_id']),
    ('groups', ['facultet_id', 'programm_id']),
    ('groups', ['university_id', 'facultet_id', 'programm_id']),
])
async def test_structure_requires_one_supported_filter(
    service_client, resource, filter_names,
):
    response = await service_client.get(
        f'/v1/education/{resource}',
        params={name: str(uuid.uuid4()) for name in filter_names},
    )

    assert response.status == 400
    assert response.json() == {
        'success': False,
        'error': 'invalid_structure_filter',
    }


@pytest.mark.parametrize('resource,filter_name,target_key,expected_key,extra', [
    ('faculties', 'university_id', 'university', 'faculties',
     {'facultet_id': 'ignored', 'programm_id': 'ignored'}),
    ('programs', 'university_id', 'university', 'programs',
     {'programm_id': 'ignored'}),
    ('programs', 'facultet_id', 'alpha_faculty', 'alpha_programs',
     {'programm_id': 'ignored'}),
    ('groups', 'programm_id', 'shared_program', 'shared_groups',
     {'unused': 'ignored'}),
])
async def test_structure_ignores_unrelated_parameters(
    service_client, structure, resource, filter_name, target_key, expected_key, extra,
):
    response = await service_client.get(
        f'/v1/education/{resource}',
        params={filter_name: structure[target_key], **extra},
    )

    assert response.status == 200
    assert response.json() == {
        'success': True,
        resource: structure[expected_key],
    }
