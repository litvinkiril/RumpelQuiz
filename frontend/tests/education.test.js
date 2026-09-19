import test from 'node:test';
import assert from 'node:assert/strict';
import {createApi, createController, ApiError, renderView} from '../app.js';

const profile = {success: true, email: 'self@example.invalid', first_name: 'Иван', university_position: [
  {university_id: 'hse', university_name: 'ВШЭ', role: 'admin', group_id: null, group_name: null},
  {university_id: 'mirea', university_name: 'МИРЭА', role: 'student', group_id: null, group_name: null},
]};
const admin = {membership_id: 'member', first_name: 'Елена', last_name: 'Зыбина', middle_name: null,
  avatar_url: null, email: 'admin@example.invalid'};
const result = {success: true, admins: [admin]};
function setup(api = async () => result) {
  const controller = createController({api});
  Object.assign(controller.state, {token: 'jwt', profile, screen: 'profile'});
  return controller;
}

test('education requests use protected education URL', async () => {
  let request;
  const api = createApi(async (url, options) => { request = {url, ...options}; return {ok: true, json: async () => result}; });
  await api('education/universities/hse/admins', undefined, 'jwt');
  assert.equal(request.url, '/v1/education/universities/hse/admins');
  assert.equal(request.method, 'GET');
  assert.equal(request.headers.Authorization, 'Bearer jwt');
});

test('admin university opens local menu; student card cannot open admin menu', () => {
  const controller = setup(() => { throw new Error('No request expected'); });
  assert.equal(controller.openUniversity('mirea'), false);
  assert.equal(controller.openUniversity('unknown'), false);
  assert.equal(controller.openUniversity('hse'), true);
  assert.equal(controller.state.screen, 'university');
  assert.deepEqual(controller.state.selectedUniversity, {id: 'hse', name: 'ВШЭ'});
  const html = renderView(controller.state);
  assert.match(html, /Посмотреть администраторов/);
  assert.match(html, /Посмотреть преподавателей/);
  assert.match(html, /Посмотреть студентов/);
  assert.match(html, /Посмотреть группы/);
  assert.equal((html.match(/class="section-card" disabled/g) || []).length, 3);
  controller.navigate('profile');
  assert.equal(controller.state.profile, profile);
  assert.equal(controller.state.selectedUniversity, null);
  const cards = renderView(controller.state);
  assert.equal((cards.match(/data-action="university"/g) || []).length, 1);
});

test('list loads only for selected university and contact opens without a request', async () => {
  const calls = [];
  const controller = setup(async (...args) => { calls.push(args); return result; });
  controller.openUniversity('hse');
  await controller.openAdmins();
  assert.deepEqual(calls, [['education/universities/hse/admins', undefined, 'jwt']]);
  assert.equal(controller.state.screen, 'admins');
  assert.ok(!renderView(controller.state).includes(admin.email));
  controller.openAdmin('member');
  const html = renderView(controller.state);
  assert.match(html, /<dialog/);
  assert.match(html, /Елена/);
  assert.match(html, /mailto:admin%40example.invalid/);
  assert.equal(calls.length, 1);
  controller.closeAdmin();
  assert.equal(controller.state.selectedAdmin, null);
  controller.navigate('university');
  assert.equal(controller.state.admins, null);
});

test('late list response cannot restore private data after leaving', async () => {
  let resolve;
  const controller = setup(() => new Promise(r => { resolve = r; }));
  controller.openUniversity('hse');
  const pending = controller.openAdmins();
  controller.navigate('profile');
  resolve(result);
  await pending;
  assert.equal(controller.state.screen, 'profile');
  assert.equal(controller.state.admins, null);
  assert.equal(controller.state.selectedUniversity, null);
});

test('403 clears old contacts, preserves login, and offers a retry', async () => {
  let count = 0;
  const api = createApi(async () => ++count === 1
    ? {ok: true, json: async () => result}
    : {ok: false, status: 403, json: async () => ({error: 'university_access_denied'})});
  const controller = setup(api);
  controller.openUniversity('hse');
  await controller.openAdmins();
  controller.openAdmin('member');
  await controller.openAdmins();
  assert.equal(controller.state.token, 'jwt');
  assert.equal(controller.state.admins, null);
  assert.equal(controller.state.selectedAdmin, null);
  assert.match(controller.state.error, /Нет доступа/);
  assert.match(renderView(controller.state), /Попробовать снова/);
});

test('failed request can recover and empty or malformed lists are handled', async () => {
  let attempt = 0;
  const controller = setup(async () => {
    if (++attempt === 1) throw new ApiError('Нет сети');
    if (attempt === 2) return {success: true};
    return {success: true, admins: []};
  });
  controller.openUniversity('hse');
  assert.equal(await controller.openAdmins(), false);
  assert.equal(await controller.openAdmins(), false);
  assert.equal(await controller.openAdmins(), true);
  assert.match(renderView(controller.state), /пока нет активных администраторов/);
});

test('expired session clears all university data', async () => {
  const controller = setup(async () => { throw new ApiError('Войдите снова', 401); });
  controller.openUniversity('hse');
  await controller.openAdmins();
  assert.equal(controller.state.screen, 'login');
  assert.equal(controller.state.profile, null);
  assert.equal(controller.state.selectedUniversity, null);
  assert.equal(controller.state.admins, null);
});

test('list request refreshes an expired access token', async () => {
  const calls = [];
  const controller = setup(async (path, data, token) => {
    calls.push({path, token});
    if (path === 'refresh') return {access_token: 'new', refresh_token: 'rotated', session_id: 's'};
    if (token === 'jwt') throw new ApiError('Expired', 401);
    return result;
  });
  Object.assign(controller.state, {refreshToken: 'refresh', sessionId: 's'});
  controller.openUniversity('hse');
  await controller.openAdmins();
  assert.equal(controller.state.admins[0], admin);
  assert.equal(calls[1].path, 'refresh');
  assert.equal(calls[2].token, 'new');
});

test('contact data is escaped and unsafe avatar URLs do not render', () => {
  const unsafe = {...admin, first_name: '<script>alert(1)</script>', avatar_url: 'javascript:alert(1)', email: 'x" onmouseover="bad'};
  const html = renderView({screen: 'admins', selectedUniversity: {name: '<img src=x>'}, admins: [unsafe], selectedAdmin: unsafe});
  assert.ok(!html.includes('<script>'));
  assert.ok(!html.includes('<img'));
  assert.ok(!html.includes('href="javascript:'));
  assert.ok(!html.includes(' onmouseover="'));
  assert.match(html, /&lt;script&gt;/);
});
