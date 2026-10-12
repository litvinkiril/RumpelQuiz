import test from 'node:test';
import assert from 'node:assert/strict';
import { createApi, createController, ApiError, renderView } from '../app.js';
import { bindEvents } from '../assets/ui/events.js';

const universityId = '00000000-0000-4000-8000-000000000001';
const program = { id: 'program', name: 'Информатика', facultet_names: ['ФКН', 'Математика'] };
const group = { ...program, id: 'group', name: 'ЭК-2023-01', program_name: program.name };

function setup(api) {
  const controller = createController({ api });
  Object.assign(controller.state, {
    token: 'jwt',
    screen: 'profile',
    profile: {
      university_position: [{ university_id: universityId, university_name: 'ВШЭ', role: 'admin' }],
    },
  });
  controller.openUniversity(universityId);
  return controller;
}

for (const [kind, item] of [
  ['groups', group],
  ['programs', program],
]) {
  test(`${kind} search uses the public GET endpoint scoped to the selected university`, async () => {
    let request;
    const controller = setup(
      createApi(async (url, options) => {
        request = { url: new URL(url, 'http://localhost'), options };
        return { ok: true, json: async () => ({ success: true, [kind]: [item] }) };
      }),
    );
    assert.equal(await controller.searchEducation('  Инфо %_!&+  ', kind), true);
    assert.equal(request.url.pathname, `/v1/education/search/${kind}`);
    assert.equal(request.url.searchParams.get('university_id'), universityId);
    assert.equal(request.url.searchParams.get('name'), 'Инфо %_!&+');
    assert.equal(request.options.method, 'GET');
    assert.equal(request.options.headers.Authorization, undefined);
    assert.deepEqual(controller.state.educationSearchItems, [item]);
    const view = renderView(controller.state);
    assert.ok(view.includes(item.name));
    assert.ok(view.includes('ФКН, Математика'));
    assert.match(view, /Найдено: 1/);
    assert.equal(view.includes('Программа:'), kind === 'groups');
  });
}

test('typing waits 300 ms and Enter cancels the queued duplicate request', async (t) => {
  t.mock.timers.enable({ apis: ['setTimeout'] });
  const calls = [];
  const controller = setup(async (path) => {
    calls.push(path);
    return { success: true, groups: [group] };
  });
  controller.editEducationSearchQuery('Э');
  t.mock.timers.tick(200);
  controller.editEducationSearchQuery('ЭК');
  t.mock.timers.tick(299);
  assert.equal(calls.length, 0);
  t.mock.timers.tick(1);
  await Promise.resolve();
  assert.equal(calls.length, 1);
  controller.editEducationSearchQuery('ЭК-2023');
  assert.equal(await controller.searchEducation(), true);
  t.mock.timers.tick(300);
  assert.equal(calls.length, 2);
});

test('empty and whitespace input clears results and sends no request', async () => {
  let calls = 0;
  const controller = setup(async () => {
    ++calls;
    return { success: true, groups: [group] };
  });
  await controller.searchEducation('ЭК');
  await controller.searchEducation(' \t ');
  assert.equal(calls, 1);
  assert.equal(controller.state.educationSearchItems, null);
  assert.equal(controller.state.educationSearchError, '');
  assert.match(renderView(controller.state), /aria-busy="false"\s+hidden/);
});

test('switching type repeats the current query and keeps the selected tab', async () => {
  const calls = [];
  const controller = setup(async (path) => {
    calls.push(path);
    return path.includes('/programs?')
      ? { success: true, programs: [program] }
      : { success: true, groups: [group] };
  });
  await controller.searchEducation('Инфо');
  await controller.selectEducationSearchKind('programs');
  assert.equal(calls.length, 2);
  assert.match(calls[1], /^education\/search\/programs\?/);
  assert.equal(controller.state.educationSearchQuery, 'Инфо');
  assert.match(renderView(controller.state), /value="programs"\s+checked/);
  assert.match(renderView(controller.state), /placeholder="Введите название программы…"/);
});

test('loading keeps input and navigation available; stale successes and failures cannot replace new results', async () => {
  const pending = [];
  const controller = setup(
    () => new Promise((resolve, reject) => pending.push({ resolve, reject })),
  );
  const old = controller.searchEducation('старый');
  assert.equal(controller.state.busy, false);
  assert.match(renderView(controller.state), /Ищем…/);
  const latest = controller.searchEducation('новый', 'programs');
  pending[1].resolve({ success: true, programs: [program] });
  await latest;
  pending[0].reject(new ApiError('Ошибка старого запроса'));
  await old;
  assert.deepEqual(controller.state.educationSearchItems, [program]);
  assert.equal(controller.state.educationSearchError, '');
  const stale = controller.searchEducation('старый');
  const current = controller.searchEducation('новый');
  pending[2].resolve({ success: true, programs: [] });
  await stale;
  assert.equal(controller.state.educationSearchLoading, true);
  pending[3].resolve({ success: true, programs: [program] });
  await current;
  assert.deepEqual(controller.state.educationSearchItems, [program]);
});

test('clearing input ignores an already pending response', async () => {
  let resolve;
  const controller = setup(
    () =>
      new Promise((done) => {
        resolve = done;
      }),
  );
  const pending = controller.searchEducation('ЭК');
  controller.editEducationSearchQuery('');
  resolve({ success: true, groups: [group] });
  await pending;
  assert.equal(controller.state.educationSearchItems, null);
  assert.equal(controller.state.educationSearchLoading, false);
});

for (const screen of ['profile', 'account', 'quiz-section', 'test-section', 'university']) {
  test(`leaving for ${screen} cancels queued and pending searches`, async (t) => {
    t.mock.timers.enable({ apis: ['setTimeout'] });
    let resolve;
    let calls = 0;
    const controller = setup(() => {
      ++calls;
      return new Promise((done) => {
        resolve = done;
      });
    });
    const pending = controller.searchEducation('ЭК');
    controller.navigate(screen);
    resolve({ success: true, groups: [group] });
    await pending;
    assert.equal(controller.state.educationSearchItems, null);
    assert.equal(controller.state.educationSearchQuery, '');
    controller.state.screen = 'profile';
    controller.openUniversity(universityId);
    controller.editEducationSearchQuery('ЭК');
    controller.navigate(screen);
    t.mock.timers.tick(300);
    assert.equal(calls, 1);
  });
}

test('returning to the same university cannot revive a previous search', async () => {
  let resolve;
  const controller = setup(
    () =>
      new Promise((done) => {
        resolve = done;
      }),
  );
  const pending = controller.searchEducation('ЭК');
  controller.navigate('profile');
  controller.openUniversity(universityId);
  resolve({ success: true, groups: [group] });
  await pending;
  assert.equal(controller.state.educationSearchItems, null);
});

test('empty, too many, network failure and malformed responses can recover', async () => {
  const results = [
    { success: true, groups: [] },
    { success: false, error: 'too_many_results', message: '<script>bad</script>', groups: [] },
    new ApiError('Нет сети'),
    { success: true, groups: [{ ...group, facultet_names: null }] },
    { success: true, groups: [group] },
  ];
  const controller = setup(async () => {
    const result = results.shift();
    if (result instanceof Error) throw result;
    return result;
  });
  await controller.searchEducation('ЭК');
  assert.match(renderView(controller.state), /Ничего не найдено/);
  for (const message of [
    'Введите более точное название',
    'Нет сети',
    'Не удалось загрузить результаты',
  ]) {
    assert.equal(await controller.searchEducation(), false);
    assert.ok(renderView(controller.state).includes(message));
    assert.match(renderView(controller.state), /data-action="education-search-retry"/);
    assert.equal(controller.state.educationSearchItems, null);
    assert.equal(controller.state.educationSearchLoading, false);
  }
  assert.equal(await controller.searchEducation(), true);
  assert.equal(controller.state.educationSearchError, '');
});

test('duplicate names remain distinct; unassigned groups and unsafe values render safely', async () => {
  const controller = setup(async () => ({
    success: true,
    groups: [
      {
        ...group,
        name: '<img src=x onerror="bad">',
        program_name: '<script>bad</script>',
        facultet_names: ['<b>Факультет</b>'],
      },
      {
        ...group,
        id: 'other',
        name: '<img src=x onerror="bad">',
        program_name: null,
        facultet_names: [],
      },
    ],
  }));
  await controller.searchEducation('x" onfocus="bad');
  const view = renderView(controller.state);
  assert.match(view, /Найдено: 2/);
  assert.match(view, /Программа: Не указана/);
  assert.match(view, /Факультеты:\s+Не указаны/);
  assert.ok(!view.includes('<img'));
  assert.ok(!view.includes('<script>'));
  assert.ok(!view.includes(' onfocus="'));
  assert.ok(view.includes('&lt;b&gt;Факультет&lt;/b&gt;'));
});

test('search input, tabs, Enter, suggested queries and retry are wired to the controller', async () => {
  const listeners = {};
  const calls = [];
  const controller = setup(async (path) => {
    calls.push(path);
    return { success: true, groups: [group], programs: [program] };
  });
  const root = {
    addEventListener: (name, listener) => {
      listeners[name] = listener;
    },
    querySelector: () => ({ focus() {} }),
  };
  bindEvents({ root, controller, render() {}, canLeave: () => true });
  listeners.input({ target: { hasAttribute: () => true, value: 'ЭК' } });
  listeners.submit({
    preventDefault() {},
    target: {
      dataset: { form: 'education-search' },
      querySelector: () => ({ value: 'ЭК' }),
    },
  });
  await Promise.resolve();
  assert.equal(calls.length, 1);
  listeners.change({ target: { name: 'university-search-type', value: 'programs' } });
  await Promise.resolve();
  assert.match(calls[1], /\/programs\?/);
  const button = {
    dataset: { action: 'education-search-query', kind: 'groups', query: 'ФК-2022-01' },
  };
  listeners.click({ target: { closest: () => button } });
  await Promise.resolve();
  assert.equal(controller.state.educationSearchQuery, 'ФК-2022-01');
  assert.equal(controller.state.educationSearchKind, 'groups');
  button.dataset.action = 'education-search-retry';
  listeners.click({ target: { closest: () => button } });
  await Promise.resolve();
  assert.equal(calls.length, 4);
});
