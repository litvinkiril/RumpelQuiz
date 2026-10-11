import test from 'node:test';
import assert from 'node:assert/strict';
import { createApi, createController, ApiError, renderView } from '../app.js';

const university = '00000000-0000-4000-8000-000000000001';
const faculty = { id: '00000000-0000-4000-8000-000000000002', name: 'Математика' };
const facultyResponse = { success: true, faculties: [faculty] };
const createdResponse = { success: true, user_id: '00000000-0000-4000-8000-000000000003' };

function setup(api, scope = 'university', storage) {
  const controller = createController({ api, storage });
  Object.assign(controller.state, {
    token: 'token',
    screen: 'profile',
    profile: {
      university_position: [
        {
          role: 'admin',
          status: 'active',
          university_id: university,
          university_name: 'Учебный университет',
          admin_scope: scope,
        },
      ],
    },
  });
  controller.openUniversity(university);
  return controller;
}
function fill(controller) {
  for (const [field, value] of Object.entries({
    first_name: ' Анна ',
    last_name: ' Иванова ',
    middle_name: ' Сергеевна ',
    email: ' anna@example.com ',
    description: ' Учится на втором курсе. ',
  }))
    controller.editNewUser(field, value);
}

test('university page opens a complete form without loading faculties for a student', async () => {
  const controller = setup(() => assert.fail('No request is needed before choosing admin'));
  assert.match(renderView(controller.state), /data-action="create-user"/);
  assert.equal(await controller.openUserCreation(), true);
  const view = renderView(controller.state);
  for (const field of [
    'first_name',
    'last_name',
    'middle_name',
    'email',
    'new-user-password',
    'role',
    'description',
  ])
    assert.ok(view.includes(`name="${field}"`));
  for (const role of ['student', 'teacher', 'admin']) assert.ok(view.includes(`value="${role}"`));
  assert.ok(!view.includes('name="facultet_id"'));
  assert.equal(controller.state.newUserDraft.role, 'student');
});

for (const role of ['student', 'teacher', 'admin']) {
  test(`creates ${role} using existing API, university and faculty contract`, async () => {
    const requests = [];
    const controller = setup(
      createApi(async (url, options) => {
        requests.push({ url, options });
        return {
          ok: true,
          json: async () =>
            url.startsWith('/v1/education/faculties?') ? facultyResponse : createdResponse,
        };
      }),
    );
    await controller.openUserCreation();
    fill(controller);
    await controller.selectNewUserRole(role);
    if (role === 'admin') controller.editNewUser('facultet_id', faculty.id);
    const password = '  пароль  ';
    controller.editNewUser('password', password);
    assert.ok(!JSON.stringify(controller.state).includes(password));
    assert.equal(await controller.createUser(password), true);
    const request = requests.at(-1);
    assert.equal(request.url, '/v1/user/create');
    assert.equal(request.options.method, 'POST');
    assert.equal(request.options.headers.Authorization, 'Bearer token');
    assert.deepEqual(JSON.parse(request.options.body), {
      university,
      first_name: 'Анна',
      last_name: 'Иванова',
      middle_name: 'Сергеевна',
      email: 'anna@example.com',
      password,
      description: 'Учится на втором курсе.',
      role,
      ...(role === 'admin' ? { facultet_id: faculty.id } : {}),
    });
    assert.equal(controller.state.newUserDraft, null);
    assert.equal(controller.state.userCreationDirty, false);
    const view = renderView(controller.state);
    assert.match(view, /Пользователь создан/);
    assert.match(view, /Иванова Анна Сергеевна/);
    assert.ok(!view.includes('data-form="user-create"'));
    assert.ok(!JSON.stringify(controller.state).includes(password));
    await controller.openUserCreation();
    assert.equal(controller.state.newUserDraft.email, '');
    assert.equal(controller.state.createdUser, null);
  });
}

test('faculties load lazily, cache per university, refresh and clear when leaving education', async () => {
  const calls = [];
  const controller = setup(async (path) => {
    calls.push(path);
    return facultyResponse;
  });
  await controller.openUserCreation();
  await controller.selectNewUserRole('admin');
  assert.equal(calls.length, 1);
  const url = new URL(calls[0], 'https://example.test');
  assert.equal(url.pathname, '/education/faculties');
  assert.equal(url.searchParams.get('university_id'), university);
  controller.editNewUser('facultet_id', faculty.id);
  await controller.selectNewUserRole('teacher');
  assert.equal(controller.state.newUserDraft.facultet_id, '');
  await controller.selectNewUserRole('admin');
  assert.equal(calls.length, 1);
  await controller.loadUserFaculties(true);
  assert.equal(calls.length, 2);
  controller.navigate('university');
  await controller.openUserCreation();
  await controller.selectNewUserRole('admin');
  assert.equal(calls.length, 2);
  controller.navigate('profile');
  assert.deepEqual(controller.state.facultyCache, {});
  assert.equal(controller.state.newUserDraft, null);
  controller.openUniversity(university);
  await controller.openUserCreation();
  await controller.selectNewUserRole('admin');
  assert.equal(calls.length, 3);
});

test('faculty administrators can only create administrators and must choose a loaded faculty', async () => {
  const calls = [];
  const controller = setup(async (path) => {
    calls.push(path);
    return path === 'user/create' ? createdResponse : facultyResponse;
  }, 'faculties');
  await controller.openUserCreation();
  assert.equal(controller.state.newUserDraft.role, 'admin');
  const view = renderView(controller.state);
  assert.ok(!view.includes('value="student"'));
  assert.ok(!view.includes('value="teacher"'));
  assert.equal(await controller.selectNewUserRole('student'), false);
  fill(controller);
  assert.equal(await controller.createUser('password'), false);
  assert.match(controller.state.error, /Выберите факультет/);
  controller.editNewUser('facultet_id', 'foreign-or-stale-id');
  assert.equal(await controller.createUser('password'), false);
  assert.equal(calls.length, 1);
  controller.editNewUser('facultet_id', faculty.id);
  assert.equal(await controller.createUser('password'), true);
  assert.equal(calls.length, 2);
});

test('empty or malformed faculty responses and failures allow retry without losing the draft', async () => {
  let response;
  const controller = setup(async () => {
    if (response instanceof Error) throw response;
    return response;
  });
  await controller.openUserCreation();
  fill(controller);
  response = new ApiError('Нет связи', 503);
  assert.equal(await controller.selectNewUserRole('admin'), false);
  assert.match(renderView(controller.state), /Загрузить факультеты/);
  assert.equal(controller.state.newUserDraft.first_name, ' Анна ');
  response = { success: true, faculties: [null] };
  assert.equal(await controller.loadUserFaculties(true), false);
  assert.deepEqual(controller.state.facultyCache, {});
  response = { success: true, faculties: [] };
  assert.equal(await controller.loadUserFaculties(true), true);
  assert.match(renderView(controller.state), /пока нет факультетов/);
  assert.match(renderView(controller.state), /type="submit"\s+class="primary"\s+disabled/);
  response = facultyResponse;
  await controller.loadUserFaculties(true);
  assert.deepEqual(controller.state.userFaculties, [faculty]);
});

test('creation errors preserve entered fields, show contextual messages and allow resubmission', async () => {
  let error;
  const controller = setup(async () => {
    if (error) throw error;
    return createdResponse;
  });
  await controller.openUserCreation();
  fill(controller);
  for (const [code, status, message] of [
    ['email_already_exists', 409, /такой почтой уже существует/],
    ['university_access_denied', 403, /нет прав создавать/],
    ['invalid_faculty', 400, /Выберите факультет из списка/],
  ]) {
    error = new ApiError('Backend error', status, code);
    assert.equal(await controller.createUser('password'), false);
    assert.match(controller.state.error, message);
    assert.equal(controller.state.newUserDraft.description, ' Учится на втором курсе. ');
    assert.equal(controller.state.screen, 'user-create');
    assert.equal(controller.state.userCreationDirty, true);
  }
  error = null;
  assert.equal(await controller.createUser('password'), true);
});

test('validation rejects blank names, invalid email and passwords beyond 72 UTF-8 bytes', async () => {
  let calls = 0;
  const controller = setup(async () => {
    calls++;
    return createdResponse;
  });
  await controller.openUserCreation();
  assert.equal(await controller.createUser('password'), false);
  fill(controller);
  controller.editNewUser('email', 'anna example.com');
  assert.equal(await controller.createUser('password'), false);
  controller.editNewUser('email', 'anna@example.com');
  assert.equal(await controller.createUser('я'.repeat(37)), false);
  assert.equal(await controller.createUser(''), false);
  assert.equal(await controller.createUser('pass\0word'), false);
  assert.equal(calls, 0);
  controller.editNewUser('middle_name', ' ');
  controller.editNewUser('description', ' ');
  assert.equal(await controller.createUser('я'.repeat(36)), true);
  assert.equal(calls, 1);
});

test('duplicate submits and late faculty or creation responses cannot affect another screen', async () => {
  let resolve,
    calls = 0;
  const controller = setup(() => {
    calls++;
    return new Promise((done) => {
      resolve = done;
    });
  });
  await controller.openUserCreation();
  const loading = controller.selectNewUserRole('admin');
  controller.navigate('university');
  resolve(facultyResponse);
  assert.equal(await loading, false);
  assert.deepEqual(controller.state.facultyCache, {});
  assert.equal(controller.state.newUserDraft, null);
  await controller.openUserCreation();
  fill(controller);
  const pending = controller.createUser('password');
  assert.equal(await controller.createUser('password'), false);
  assert.equal(calls, 2);
  controller.navigate('account');
  resolve(createdResponse);
  assert.equal(await pending, false);
  assert.equal(controller.state.screen, 'account');
  assert.equal(controller.state.createdUser, null);
});

test('expired authorization clears form and cache, and password never enters session storage', async () => {
  const writes = [];
  const controller = setup(
    async () => {
      throw new ApiError('Сессия истекла', 401);
    },
    'university',
    { getItem: () => null, setItem: (key, value) => writes.push(value) },
  );
  await controller.openUserCreation();
  fill(controller);
  controller.editNewUser('password', 'SECRET_PASSWORD');
  controller.state.facultyCache[university] = [faculty];
  assert.equal(await controller.createUser('SECRET_PASSWORD'), false);
  assert.equal(controller.state.screen, 'login');
  assert.equal(controller.state.newUserDraft, null);
  assert.deepEqual(controller.state.facultyCache, {});
  assert.ok(
    writes.every(
      (value) => !value.includes('SECRET_PASSWORD') && !value.includes('anna@example.com'),
    ),
  );
});

test('missing admin membership, inactive membership and unknown scope cannot open creation', async () => {
  const controller = setup(() => assert.fail('No API call expected'));
  controller.state.profile.university_position[0].status = 'inactive';
  assert.equal(await controller.openUserCreation(), false);
  controller.state.profile.university_position[0].status = 'active';
  controller.state.profile.university_position[0].role = 'teacher';
  assert.equal(await controller.openUserCreation(), false);
  controller.state.profile.university_position[0].role = 'admin';
  controller.state.selectedUniversity.adminScope = null;
  assert.equal(await controller.openUserCreation(), false);
  assert.match(controller.state.error, /Обновите профиль/);
});

test('names, descriptions and faculty names are escaped in the form and confirmation', async () => {
  const controller = setup(async (path) =>
    path === 'user/create'
      ? createdResponse
      : {
          success: true,
          faculties: [{ ...faculty, name: '<img src=x onerror=alert(1)>' }],
        },
  );
  await controller.openUserCreation();
  fill(controller);
  controller.editNewUser('first_name', '"><script>alert(1)</script>');
  controller.editNewUser('description', '</textarea><script>alert(1)</script>');
  await controller.selectNewUserRole('admin');
  let view = renderView(controller.state);
  assert.ok(!view.includes('<script>'));
  assert.ok(!view.includes('<img src=x'));
  controller.editNewUser('facultet_id', faculty.id);
  await controller.createUser('password');
  view = renderView(controller.state);
  assert.ok(!view.includes('<script>'));
  assert.match(view, /&lt;script&gt;/);
});
