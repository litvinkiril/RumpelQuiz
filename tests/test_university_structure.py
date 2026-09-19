from pathlib import Path

import psycopg2
import pytest

from test_profile import auth_headers, create_user
from test_university_admins import membership, university


def faculty(cursor, university_id, name='Faculty'):
    cursor.execute(
        'INSERT INTO education.faculties (university_id, name) VALUES (%s, %s) RETURNING id',
        (university_id, name),
    )
    return str(cursor.fetchone()[0])


def program(cursor, university_id, faculty_id, name='Program'):
    # Both rows must exist by the end of the statement/transaction.
    cursor.execute(
        'WITH p AS (INSERT INTO education.programs (university_id, name) '
        'VALUES (%s, %s) RETURNING id) '
        'INSERT INTO education.faculty_programs (faculty_id, program_id, university_id) '
        'SELECT %s, p.id, %s FROM p RETURNING program_id',
        (university_id, name, faculty_id, university_id),
    )
    return str(cursor.fetchone()[0])


def test_shared_program_and_group_university_boundaries(pgsql):
    cursor = pgsql['db_1'].cursor()
    uni = university(cursor, 'University')
    other = university(cursor, 'Other')
    first, second = faculty(cursor, uni, 'FKN'), faculty(cursor, uni, 'FEN')
    foreign = faculty(cursor, other)
    course = program(cursor, uni, first)
    cursor.execute(
        'INSERT INTO education.faculty_programs VALUES (%s, %s, %s)', (second, course, uni),
    )
    cursor.execute(
        "INSERT INTO education.study_groups (university_id, program_id, name) VALUES (%s, %s, 'Group')",
        (uni, course),
    )
    with pytest.raises(psycopg2.errors.ForeignKeyViolation):
        cursor.execute(
            'INSERT INTO education.faculty_programs VALUES (%s, %s, %s)', (foreign, course, uni),
        )
    with pytest.raises(psycopg2.errors.ForeignKeyViolation):
        cursor.execute(
            "INSERT INTO education.study_groups (university_id, program_id, name) VALUES (%s, %s, 'Bad')",
            (other, course),
        )
    with pytest.raises(psycopg2.errors.UniqueViolation):
        cursor.execute('INSERT INTO education.faculty_programs VALUES (%s, %s, %s)', (first, course, uni))


def test_program_requires_faculty_and_can_change_assignment(pgsql):
    cursor = pgsql['db_1'].cursor()
    uni = university(cursor, 'University')
    first, second = faculty(cursor, uni, 'First'), faculty(cursor, uni, 'Second')
    with pytest.raises(psycopg2.errors.CheckViolation):
        cursor.execute("INSERT INTO education.programs (university_id, name) VALUES (%s, 'Orphan')", (uni,))
    course = program(cursor, uni, first)
    with pytest.raises(psycopg2.errors.CheckViolation):
        cursor.execute('DELETE FROM education.faculty_programs WHERE program_id = %s', (course,))
    # Updating a link must actually keep NEW values, not silently preserve OLD.
    cursor.execute(
        'UPDATE education.faculty_programs SET faculty_id = %s WHERE program_id = %s',
        (second, course),
    )
    cursor.execute('SELECT faculty_id FROM education.faculty_programs WHERE program_id = %s', (course,))
    assert str(cursor.fetchone()[0]) == second
    cursor.execute('DELETE FROM education.programs WHERE id = %s', (course,))
    cursor.execute('SELECT COUNT(*) FROM education.faculty_programs WHERE program_id = %s', (course,))
    assert cursor.fetchone()[0] == 0


@pytest.mark.parametrize('role,scope', [
    ('admin', None), ('admin', 'everything'), ('student', 'university'), ('teacher', 'faculties'),
])
def test_scope_is_explicit_and_only_for_admins(pgsql, role, scope):
    cursor = pgsql['db_1'].cursor()
    uni = university(cursor, 'University')
    user = create_user(cursor)
    with pytest.raises(psycopg2.errors.CheckViolation):
        cursor.execute(
            'INSERT INTO education.memberships (user_id, university_id, role, admin_scope) VALUES (%s, %s, %s, %s)',
            (user, uni, role, scope),
        )


def test_faculty_grants_enforce_role_scope_and_university(pgsql):
    cursor = pgsql['db_1'].cursor()
    uni = university(cursor, 'University')
    other = university(cursor, 'Other')
    first, second = faculty(cursor, uni, 'First'), faculty(cursor, uni, 'Second')
    foreign = faculty(cursor, other)
    member = membership(cursor, create_user(cursor), uni, scope='faculties')
    for target in (first, second):
        cursor.execute(
            'INSERT INTO education.admin_faculties (membership_id, faculty_id, university_id) VALUES (%s, %s, %s)',
            (member, target, uni),
        )
    for role, scope in [('student', None), ('admin', 'university')]:
        wrong_member = membership(cursor, create_user(cursor), uni, role=role, scope=scope)
        with pytest.raises(psycopg2.errors.ForeignKeyViolation):
            cursor.execute(
                'INSERT INTO education.admin_faculties (membership_id, faculty_id, university_id) VALUES (%s, %s, %s)',
                (wrong_member, first, uni),
            )
    with pytest.raises(psycopg2.errors.ForeignKeyViolation):
        cursor.execute(
            'INSERT INTO education.admin_faculties (membership_id, faculty_id, university_id) VALUES (%s, %s, %s)',
            (member, foreign, uni),
        )
    with pytest.raises(psycopg2.errors.ForeignKeyViolation):
        cursor.execute("UPDATE education.memberships SET admin_scope = 'university' WHERE id = %s", (member,))
    cursor.execute('DELETE FROM education.admin_faculties WHERE membership_id = %s', (member,))
    cursor.execute("UPDATE education.memberships SET admin_scope = 'university' WHERE id = %s", (member,))


async def test_profile_exposes_faculty_scope_without_other_university_data(service_client, pgsql):
    cursor = pgsql['db_1'].cursor()
    uni = university(cursor, 'University')
    user = create_user(cursor)
    cursor.execute('INSERT INTO users.profiles (user_id) VALUES (%s)', (user,))
    membership(cursor, user, uni, scope='faculties')
    response = await service_client.get('/v1/user/profile', headers=auth_headers(user))
    assert response.status == 200
    assert response.json()['university_position'] == [{
        'university_id': uni, 'university_name': 'University', 'role': 'admin',
        'admin_scope': 'faculties', 'group_id': None, 'group_name': None,
    }]


def test_migration_preserves_existing_memberships_and_is_repeatable(pgsql):
    migration = Path(__file__).parents[1] / 'postgresql/migrations/004_university_structure.sql'
    source = migration.read_text()
    body = source.split('-- BEGIN UNIVERSITY STRUCTURE')[1].split('-- END UNIVERSITY STRUCTURE')[0]
    schema = (migration.parents[1] / 'schemas/db_1.sql').read_text()
    assert body == schema.split('-- BEGIN UNIVERSITY STRUCTURE')[1].split('-- END UNIVERSITY STRUCTURE')[0]

    cursor = pgsql['db_1'].cursor()
    # Reconstruct the old schema only inside the disposable test DB transaction.
    cursor.execute('BEGIN')
    try:
        cursor.execute('DROP TABLE education.admin_faculties, education.faculty_programs, '
                       'education.programs, education.faculties CASCADE')
        cursor.execute('ALTER TABLE education.memberships DROP COLUMN admin_scope CASCADE')
        cursor.execute('ALTER TABLE education.study_groups DROP COLUMN program_id')
        uni = university(cursor, 'Legacy University')
        user = create_user(cursor)
        cursor.execute("INSERT INTO education.memberships (user_id, university_id, role) VALUES (%s, %s, 'admin') RETURNING id", (user, uni))
        member = str(cursor.fetchone()[0])
        cursor.execute("INSERT INTO education.study_groups (university_id, name) VALUES (%s, 'Legacy') RETURNING id", (uni,))
        group = str(cursor.fetchone()[0])
        cursor.execute(body)
        cursor.execute('SELECT admin_scope FROM education.memberships WHERE id = %s', (member,))
        assert cursor.fetchone()[0] == 'university'
        cursor.execute('SELECT program_id FROM education.study_groups WHERE id = %s', (group,))
        assert cursor.fetchone()[0] is None
        cursor.execute("UPDATE education.memberships SET admin_scope = 'faculties' WHERE id = %s", (member,))
        cursor.execute(body)
        cursor.execute('SELECT admin_scope FROM education.memberships WHERE id = %s', (member,))
        assert cursor.fetchone()[0] == 'faculties'
    finally:
        cursor.execute('ROLLBACK')
