import test from 'node:test';
import assert from 'node:assert/strict';
import { createApi, createController, ApiError, renderView } from '../app.js';

const university = '00000000-0000-4000-8000-000000000001';
const firstFaculty = { id: '00000000-0000-4000-8000-000000000002', name: 'Компьютерные науки' };
const secondFaculty = { id: '00000000-0000-4000-8000-000000000003', name: 'Экономика' };
const firstProgram = { id: '00000000-0000-4000-8000-000000000004', name: 'Информатика' };
const secondProgram = { id: '00000000-0000-4000-8000-000000000005', name: 'Экономика' };
const createdId = '00000000-0000-4000-8000-000000000006';

function setup(api, scope = 'university', storage) {
  const controller = createController({ api, storage });
  Object.assign(controller.state, {
    screen: 'profile',
    token: 'jwt',
    profile: {
      university_position: [
        {
          role: 'admin',
          admin_scope: scope,
          status: 'active',
          university_id: university,
          university_name: 'ВШЭ',
        },
      ],
    },
  });
  controller.openUniversity(university);
  return controller;
}

function fixtureApi(calls = []) {
  return async (path, data, token) => {
    calls.push({ path, data, token });
    if (path.startsWith('education/faculties?'))
      return { success: true, faculties: [firstFaculty, secondFaculty] };
    if (path.startsWith('education/programs?'))
      return {
        success: true,
        programs: path.includes(secondFaculty.id) ? [secondProgram] : [firstProgram],
      };
    if (data)
      return {
        success: true,
        [path.endsWith('faculties')
          ? 'faculty_id'
          : path.endsWith('programs')
            ? 'program_id'
            : 'group_id']: createdId,
      };
    return { success: true, [path.split('/')[1].split('?')[0]]: [] };
  };
}

for (const kind of ['faculties', 'programs', 'groups']) {
  test(`${kind} quick action submits the existing authenticated JSON contract and opens its list`, async () => {
    const calls = [];
    const mock = fixtureApi();
    const controller = setup(
      createApi(async (url, options) => {
        const data = options.body ? JSON.parse(options.body) : undefined;
        calls.push({ url, options, data });
        return { ok: true, json: async () => mock(url.slice('/v1/'.length), data) };
      }),
    );
    assert.equal(
      (renderView(controller.state).match(/data-action="education-create"/g) || []).length,
      3,
    );
    await controller.openEducationCreation(kind);
    assert.equal(calls.length, kind === 'faculties' ? 0 : 1);
    controller.editEducationName('  Новое название  ');
    if (kind !== 'faculties') await controller.selectEducationCreationFaculty(firstFaculty.id);
    if (kind === 'groups') controller.selectEducationCreationProgram(firstProgram.id);
    assert.equal(await controller.createEducationItem(), true);
    const post = calls.at(-1);
    assert.equal(post.url, `/v1/education/${kind}`);
    assert.equal(post.options.method, 'POST');
    assert.equal(post.options.headers.Authorization, 'Bearer jwt');
    assert.equal(post.options.headers['Content-Type'], 'application/json');
    assert.deepEqual(post.data, {
      university_id: university,
      name: 'Новое название',
      ...(kind === 'programs' ? { faculty_ids: [firstFaculty.id] } : {}),
      ...(kind === 'groups' ? { program_id: firstProgram.id } : {}),
    });
    assert.equal(controller.state.educationCreationSaved.id, createdId);
    assert.equal(controller.state.educationCreationDirty, false);
    assert.match(renderView(controller.state), /Новое название/);
    assert.equal(await controller.createEducationItem(), false);
    await controller.openEducationCatalog(kind);
    assert.equal(controller.state.screen, 'education-catalog');
    assert.equal(controller.state.educationCreationDraft, null);
  });
}

test('group choices load by faculty, preserve the name and reset a previously selected program', async () => {
  const calls = [];
  const controller = setup(fixtureApi(calls));
  await controller.openEducationCreation('groups');
  assert.equal(calls.length, 1);
  assert.ok(!renderView(controller.state).includes('id="education-new-program"'));
  controller.editEducationName('ПИ-2026-01');
  await controller.selectEducationCreationFaculty(firstFaculty.id);
  assert.equal(calls.at(-1).path, `education/programs?facultet_id=${firstFaculty.id}`);
  controller.selectEducationCreationProgram(firstProgram.id);
  await controller.selectEducationCreationFaculty(secondFaculty.id);
  assert.equal(controller.state.educationCreationDraft.program_id, '');
  assert.equal(controller.state.educationCreationDraft.name, 'ПИ-2026-01');
  assert.deepEqual(controller.state.educationCreationPrograms, [secondProgram]);
  assert.equal(controller.selectEducationCreationProgram(firstProgram.id), false);
  assert.equal(await controller.createEducationItem(), false);
  controller.selectEducationCreationProgram(secondProgram.id);
  assert.equal(await controller.createEducationItem(), true);
  assert.equal(calls.at(-1).data.program_id, secondProgram.id);
});

test('required names, faculties and programs reject invalid or foreign choices before POST', async () => {
  const calls = [];
  const controller = setup(fixtureApi(calls));
  await controller.openEducationCreation('programs');
  controller.editEducationName(' \n\t');
  assert.equal(await controller.createEducationItem(), false);
  controller.editEducationName('Программа');
  assert.equal(await controller.createEducationItem(), false);
  assert.equal(await controller.selectEducationCreationFaculty('foreign'), false);
  assert.equal(calls.filter((call) => call.data).length, 0);
  await controller.selectEducationCreationFaculty(firstFaculty.id);
  controller.editEducationName('Текст\0');
  assert.equal(await controller.createEducationItem(), false);
});

test('duplicate errors preserve the form and a successful faculty creation invalidates cached choices', async () => {
  let attempt = 0;
  const controller = setup(async () => {
    if (++attempt === 1) throw new ApiError('Conflict', 409, 'faculty_already_exists');
    return { success: true, faculty_id: createdId };
  });
  controller.state.facultyCache[university] = [firstFaculty];
  await controller.openEducationCreation('faculties');
  controller.editEducationName('Новый факультет');
  assert.equal(await controller.createEducationItem(), false);
  assert.match(controller.state.error, /уже есть в этом университете/);
  assert.equal(controller.state.educationCreationDraft.name, 'Новый факультет');
  assert.equal(controller.state.educationCreationDirty, true);
  assert.equal(await controller.createEducationItem(), true);
  assert.equal(Object.hasOwn(controller.state.facultyCache, university), false);
  await controller.openEducationCreation('faculties');
  assert.equal(controller.state.educationCreationDraft.name, '');
});

test('failed and empty dependent lists can retry while retaining the entered name', async () => {
  let facultiesAttempt = 0;
  let programsAttempt = 0;
  const controller = setup(async (path) => {
    if (path.includes('faculties?')) {
      if (++facultiesAttempt === 1) throw new ApiError('Нет сети');
      return { success: true, faculties: [firstFaculty] };
    }
    if (++programsAttempt === 1) return { success: true, programs: [{ name: 'Без id' }] };
    return { success: true, programs: [] };
  });
  assert.equal(await controller.openEducationCreation('groups'), false);
  controller.editEducationName('Новая группа');
  assert.equal(await controller.loadEducationCreationFaculties(true), true);
  assert.equal(await controller.selectEducationCreationFaculty(firstFaculty.id), false);
  assert.equal(await controller.loadEducationCreationPrograms(), true);
  assert.match(renderView(controller.state), /пока нет программ/);
  assert.equal(controller.state.educationCreationDraft.name, 'Новая группа');
  assert.equal(await controller.createEducationItem(), false);
});

test('faculty administrators cannot create faculties; missing or inactive admin access cannot create', async () => {
  const controller = setup(fixtureApi(), 'faculties');
  assert.equal(await controller.openEducationCreation('faculties'), false);
  assert.equal(await controller.openEducationCreation('programs'), true);
  controller.navigate('university');
  controller.state.profile.university_position[0].status = 'inactive';
  assert.equal(await controller.openEducationCreation('groups'), false);
  controller.state.profile.university_position[0].status = 'active';
  controller.state.profile.university_position[0].admin_scope = null;
  assert.equal(await controller.openEducationCreation('groups'), false);
});

test('duplicate submits and late creation replies cannot reopen a form after navigation', async () => {
  let resolve;
  const controller = setup(
    () =>
      new Promise((done) => {
        resolve = done;
      }),
  );
  await controller.openEducationCreation('faculties');
  controller.editEducationName('Факультет');
  const pending = controller.createEducationItem();
  assert.equal(await controller.createEducationItem(), false);
  controller.navigate('university');
  resolve({ success: true, faculty_id: createdId });
  await pending;
  assert.equal(controller.state.screen, 'university');
  assert.equal(controller.state.educationCreationDraft, null);
  assert.equal(controller.state.educationCreationSaved, null);
});

test('late program replies and expired authentication clear dependent creation state', async () => {
  let resolve;
  const controller = setup((path) =>
    path.includes('faculties?')
      ? Promise.resolve({ success: true, faculties: [firstFaculty] })
      : new Promise((done) => {
          resolve = done;
        }),
  );
  await controller.openEducationCreation('groups');
  const pending = controller.selectEducationCreationFaculty(firstFaculty.id);
  controller.navigate('profile');
  resolve({ success: true, programs: [firstProgram] });
  await pending;
  assert.equal(controller.state.educationCreationPrograms, null);
  const expired = setup(async () => {
    throw new ApiError('Войдите снова', 401);
  });
  await expired.openEducationCreation('programs');
  assert.equal(expired.state.screen, 'login');
  assert.equal(expired.state.educationCreationDraft, null);
});

test('creation data and option names are escaped and drafts are never stored in the session', async () => {
  let saved = '';
  const controller = setup(
    async () => ({
      success: true,
      faculties: [{ id: 'x" onclick="bad', name: '<script>bad</script>' }],
    }),
    'university',
    {
      getItem: () => '',
      setItem: (key, value) => {
        saved = value;
      },
    },
  );
  await controller.openEducationCreation('programs');
  controller.editEducationName('private-draft<svg onload="bad">');
  await controller.selectEducationCreationFaculty('x" onclick="bad');
  const view = renderView(controller.state);
  assert.ok(!view.includes('<script>'));
  assert.ok(!view.includes('<svg onload='));
  assert.ok(!view.includes(' onclick="'));
  assert.ok(!saved.includes('private-draft'));
});
