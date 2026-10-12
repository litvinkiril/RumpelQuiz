import test from 'node:test';
import assert from 'node:assert/strict';
import { createApi, createController, ApiError, renderView } from '../app.js';

const universityId = '00000000-0000-4000-8000-000000000001';
const faculty = { id: '00000000-0000-4000-8000-000000000002', name: 'Компьютерные науки' };
const otherFaculty = { id: '00000000-0000-4000-8000-000000000003', name: 'Экономика' };
const programs = [{ id: 'program', name: 'Прикладная информатика' }];
const groups = [{ id: 'group', name: 'ИКБО-01-24' }];

function setup(api) {
  const controller = createController({ api });
  Object.assign(controller.state, {
    token: 'jwt',
    screen: 'profile',
    profile: {
      university_position: [
        {
          university_id: universityId,
          university_name: 'ВШЭ',
          role: 'admin',
          admin_scope: 'university',
        },
      ],
    },
  });
  controller.openUniversity(universityId);
  return controller;
}

for (const [kind, items] of [
  ['faculties', [faculty]],
  ['programs', programs],
  ['groups', groups],
]) {
  test(`${kind} card loads the complete selected university list with the existing GET endpoint`, async () => {
    const calls = [];
    const controller = setup(
      createApi(async (url, options) => {
        calls.push({ url, options });
        return { ok: true, json: async () => ({ success: true, [kind]: items }) };
      }),
    );
    assert.equal(
      (renderView(controller.state).match(/data-action="education-catalog"/g) || []).length,
      3,
    );
    assert.equal(await controller.openEducationCatalog(kind), true);
    assert.equal(controller.state.screen, 'education-catalog');
    assert.equal(calls.length, 1);
    assert.equal(calls[0].url, `/v1/education/${kind}?university_id=${universityId}`);
    assert.equal(calls[0].options.method, 'GET');
    assert.equal(calls[0].options.headers.Authorization, 'Bearer jwt');
    assert.deepEqual(controller.state.educationCatalogItems, items);
    assert.ok(renderView(controller.state).includes(items[0].name));
    assert.equal(
      renderView(controller.state).includes('data-action="education-faculty-picker"'),
      kind !== 'faculties',
    );
  });
}

for (const [kind, items] of [
  ['programs', programs],
  ['groups', groups],
]) {
  test(`${kind} loads faculty choices on demand, filters with facultet_id only, and resets to the university`, async () => {
    const calls = [];
    const controller = setup(async (path) => {
      calls.push(path);
      return path.startsWith('education/faculties?')
        ? { success: true, faculties: [faculty, otherFaculty] }
        : { success: true, [kind]: items };
    });
    await controller.openEducationCatalog(kind);
    assert.equal(calls.length, 1);
    await controller.toggleEducationFacultyPicker();
    assert.equal(calls[1], `education/faculties?university_id=${universityId}`);
    assert.equal(controller.state.educationFacultyPickerOpen, true);
    await controller.toggleEducationFacultyPicker();
    await controller.toggleEducationFacultyPicker();
    assert.equal(calls.length, 2);
    assert.equal(await controller.selectEducationFaculty('foreign-faculty'), false);
    assert.equal(calls.length, 2);
    await controller.selectEducationFaculty(faculty.id);
    assert.equal(calls[2], `education/${kind}?facultet_id=${faculty.id}`);
    assert.equal(controller.state.educationFacultyPickerOpen, false);
    assert.ok(renderView(controller.state).includes(faculty.name));
    await controller.selectEducationFaculty('');
    assert.equal(calls[3], `education/${kind}?university_id=${universityId}`);
    assert.equal(controller.state.educationCatalogFacultyId, '');
  });
}

test('faculty list is reused by the picker and a different section starts with all faculties', async () => {
  const calls = [];
  const controller = setup(async (path) => {
    calls.push(path);
    const kind = path.split('/')[1].split('?')[0];
    return { success: true, [kind]: kind === 'faculties' ? [faculty] : programs };
  });
  await controller.openEducationCatalog('faculties');
  controller.navigate('university');
  await controller.openEducationCatalog('programs');
  await controller.toggleEducationFacultyPicker();
  assert.equal(calls.length, 2);
  await controller.selectEducationFaculty(faculty.id);
  controller.navigate('university');
  await controller.openEducationCatalog('groups');
  assert.equal(controller.state.educationCatalogFacultyId, '');
  assert.equal(calls.at(-1), `education/groups?university_id=${universityId}`);
});

test('faculty loading failure preserves the displayed list and can be retried', async () => {
  let attempts = 0;
  const controller = setup(async (path) => {
    if (path.startsWith('education/programs?')) return { success: true, programs };
    if (++attempts === 1) throw new ApiError('Нет сети');
    return { success: true, faculties: [faculty] };
  });
  await controller.openEducationCatalog('programs');
  assert.equal(await controller.toggleEducationFacultyPicker(), false);
  assert.deepEqual(controller.state.educationCatalogItems, programs);
  assert.match(renderView(controller.state), /Загрузить факультеты ещё раз/);
  assert.ok(renderView(controller.state).includes(programs[0].name));
  assert.equal(await controller.loadEducationCatalogFaculties(), true);
  assert.deepEqual(controller.state.facultyCache[universityId], [faculty]);
});

test('failed and malformed lists can retry without losing the selected faculty', async () => {
  let attempts = 0;
  const controller = setup(async (path) => {
    if (path.includes('faculties?')) return { success: true, faculties: [faculty] };
    if (path.includes('university_id=')) return { success: true, groups };
    if (++attempts === 1) throw new ApiError('Нет сети');
    if (attempts === 2) return { success: true, groups: [{ name: 'Без id' }] };
    return { success: true, groups: [] };
  });
  await controller.openEducationCatalog('groups');
  await controller.toggleEducationFacultyPicker();
  assert.equal(await controller.selectEducationFaculty(faculty.id), false);
  assert.equal(controller.state.educationCatalogFacultyId, faculty.id);
  assert.equal(controller.state.educationCatalogItems, null);
  assert.match(renderView(controller.state), /Загрузить список ещё раз/);
  assert.equal(await controller.loadEducationCatalog(), false);
  assert.match(controller.state.error, /Не удалось загрузить группы/);
  assert.equal(await controller.loadEducationCatalog(), true);
  assert.match(
    renderView(controller.state).replace(/\s+/g, ' '),
    /На выбранном факультете пока нет групп/,
  );
});

for (const screen of ['university', 'profile', 'account', 'quiz-section']) {
  test(`leaving for ${screen} cancels a pending list and clears catalog state`, async () => {
    let resolve;
    const controller = setup(
      () =>
        new Promise((done) => {
          resolve = done;
        }),
    );
    const pending = controller.openEducationCatalog('groups');
    assert.equal(await controller.openEducationCatalog('programs'), false);
    controller.navigate(screen);
    resolve({ success: true, groups });
    await pending;
    assert.equal(controller.state.screen, screen);
    assert.equal(controller.state.educationCatalogItems, null);
    assert.equal(controller.state.educationCatalogKind, '');
  });
}

test('late faculty choices do not leak into another university and expired login clears the catalog', async () => {
  let resolve;
  const controller = setup((path) =>
    path.includes('programs?')
      ? Promise.resolve({ success: true, programs })
      : new Promise((done) => {
          resolve = done;
        }),
  );
  await controller.openEducationCatalog('programs');
  const pending = controller.toggleEducationFacultyPicker();
  controller.navigate('profile');
  controller.state.profile.university_position[0].university_id = 'other';
  controller.openUniversity('other');
  resolve({ success: true, faculties: [faculty] });
  await pending;
  assert.deepEqual(controller.state.facultyCache, {});
  assert.equal(controller.state.educationFacultyPickerOpen, false);
  const expired = setup(async () => {
    throw new ApiError('Войдите снова', 401);
  });
  await expired.openEducationCatalog('programs');
  assert.equal(expired.state.screen, 'login');
  assert.equal(expired.state.educationCatalogItems, null);
  assert.equal(expired.state.educationCatalogKind, '');
});

test('empty faculty choices and unsafe names are rendered as text', async () => {
  const controller = setup(async (path) =>
    path.includes('faculties?')
      ? { success: true, faculties: [] }
      : { success: true, programs: [{ id: 'program', name: '<img src=x onerror="alert(1)">' }] },
  );
  await controller.openEducationCatalog('programs');
  await controller.toggleEducationFacultyPicker();
  const view = renderView(controller.state);
  assert.match(view, /В этом университете пока нет факультетов/);
  assert.ok(!view.includes('<img'));
  assert.match(view, /&lt;img/);
  controller.state.facultyCache[universityId] = [
    { id: 'x" onclick="bad', name: '<script>bad</script>' },
  ];
  const picker = renderView(controller.state);
  assert.ok(!picker.includes('<script>'));
  assert.ok(!picker.includes(' onclick="'));
});
