import test from 'node:test';
import assert from 'node:assert/strict';
import { createApi, createController, ApiError, renderView } from '../app.js';

const row = (i = 0) => ({
  test_id: 'test-' + i,
  title: 'Математика ' + i,
  creator_first_name: 'Анна',
  creator_last_name: 'Иванова',
  question_count: 3,
  is_favourite: false,
});
function setup(api, role = 'teacher') {
  const controller = createController({ api });
  Object.assign(controller.state, { token: 'token', screen: 'test-section', accountRoles: [role] });
  return controller;
}

for (const role of ['teacher', 'admin']) {
  test(`${role} opens the test catalog using the existing authenticated search endpoint`, async () => {
    let request;
    const controller = setup(
      createApi(async (url, options) => {
        request = { url, options };
        return { ok: true, json: async () => ({ tests: [row()] }) };
      }),
      role,
    );
    assert.equal(await controller.openTestCatalog(), true);
    const url = new URL(request.url, 'https://example.test');
    assert.equal(url.pathname, '/v1/tests/search');
    assert.equal(url.searchParams.get('name'), '');
    assert.equal(url.searchParams.get('favourites'), 'false');
    assert.equal(url.searchParams.get('count_spend'), '0');
    assert.equal(request.options.method, 'GET');
    assert.equal(request.options.headers.Authorization, 'Bearer token');
    assert.equal(controller.state.screen, 'test-catalog');
    assert.equal(controller.state.testCatalog.length, 1);
    assert.match(renderView(controller.state), /Посмотреть тесты/);
  });
}

test('test search encodes the query, pages, resets and keeps quiz search state independent', async () => {
  const calls = [];
  const controller = setup(async (path) => {
    calls.push(path);
    return { tests: calls.length === 1 ? Array.from({ length: 10 }, (_, i) => row(i)) : [row(10)] };
  });
  Object.assign(controller.state, {
    testCatalogName: '  Алгебра & геометрия  ',
    testCatalogFavourites: true,
    quizCatalog: [{ quiz_id: 'quiz' }],
    quizCatalogName: 'История',
    quizCatalogOffset: 10,
  });
  await controller.openTestCatalog();
  let url = new URL(calls[0], 'https://example.test');
  assert.equal(url.searchParams.get('name'), 'Алгебра & геометрия');
  assert.equal(url.searchParams.get('favourites'), 'true');
  assert.equal(controller.state.testCatalogMore, true);
  await controller.searchTestCatalog('unsent query', false, true);
  url = new URL(calls[1], 'https://example.test');
  assert.equal(url.searchParams.get('name'), 'Алгебра & геометрия');
  assert.equal(url.searchParams.get('favourites'), 'true');
  assert.equal(url.searchParams.get('count_spend'), '10');
  assert.equal(controller.state.testCatalog.length, 11);
  assert.equal(controller.state.testCatalogMore, false);
  assert.equal(await controller.searchTestCatalog(undefined, undefined, true), false);
  await controller.searchTestCatalog('  Физика  ', false);
  url = new URL(calls[2], 'https://example.test');
  assert.equal(url.searchParams.get('name'), 'Физика');
  assert.equal(url.searchParams.get('count_spend'), '0');
  assert.equal(controller.state.testCatalog.length, 1);
  assert.equal(controller.state.quizCatalogName, 'История');
  assert.equal(controller.state.quizCatalogOffset, 10);
  assert.deepEqual(controller.state.quizCatalog, [{ quiz_id: 'quiz' }]);
});

test('test favourites use POST and DELETE without a body and preserve state on failure', async () => {
  const requests = [];
  let failed = false;
  const controller = setup(
    createApi(async (url, options) => {
      requests.push({ url, options });
      return { ok: !failed, status: failed ? 503 : 200, json: async () => ({ success: !failed }) };
    }),
  );
  Object.assign(controller.state, {
    screen: 'test-catalog',
    testCatalog: [row()],
    testCatalogOffset: 10,
  });
  assert.equal(await controller.toggleTestFavourite('missing'), false);
  assert.equal(requests.length, 0);
  assert.equal(await controller.toggleTestFavourite('test-0'), true);
  assert.equal(requests[0].url, '/v1/tests/test-0/favourite');
  assert.equal(requests[0].options.method, 'POST');
  assert.equal(requests[0].options.headers.Authorization, 'Bearer token');
  assert.equal(requests[0].options.body, undefined);
  assert.equal(controller.state.testCatalog[0].is_favourite, true);
  failed = true;
  assert.equal(await controller.toggleTestFavourite('test-0'), false);
  assert.equal(controller.state.testCatalog[0].is_favourite, true);
  failed = false;
  controller.state.testCatalogFavourites = true;
  assert.equal(await controller.toggleTestFavourite('test-0'), true);
  assert.equal(requests.at(-1).options.method, 'DELETE');
  assert.equal(requests.at(-1).options.body, undefined);
  assert.equal(controller.state.testCatalog.length, 0);
  assert.equal(controller.state.testCatalogOffset, 9);
});

test('failed and malformed searches can retry, and navigation ignores late responses', async () => {
  let resolve;
  const controller = setup(
    () =>
      new Promise((done) => {
        resolve = done;
      }),
  );
  const pending = controller.openTestCatalog();
  controller.navigate('quiz-section');
  resolve({ tests: [row()] });
  assert.equal(await pending, false);
  assert.equal(controller.state.screen, 'quiz-section');
  assert.equal(controller.state.testCatalog, null);
  const malformed = controller.openTestCatalog();
  resolve({ quizzes: [] });
  assert.equal(await malformed, false);
  assert.match(renderView(controller.state), /test-catalog-retry/);
  assert.match(controller.state.error, /Не удалось загрузить тесты/);
  const retry = controller.searchTestCatalog();
  resolve({ tests: [] });
  assert.equal(await retry, true);
  assert.match(renderView(controller.state), /Тесты не найдены/);
});

test('students cannot open the author catalog and expired auth clears both catalogs', async () => {
  const student = setup(
    () => assert.fail('Student must not request the author catalog'),
    'student',
  );
  assert.equal(await student.openTestCatalog(), false);
  assert.equal(student.state.screen, 'test-section');
  const controller = setup(async () => {
    throw new ApiError('Expired', 401);
  });
  controller.state.quizCatalog = [{ quiz_id: 'quiz' }];
  controller.state.testCatalog = [row()];
  assert.equal(await controller.openTestCatalog(), false);
  assert.equal(controller.state.screen, 'login');
  assert.equal(controller.state.quizCatalog, null);
  assert.equal(controller.state.testCatalog, null);
  const signedOut = setup(async () => ({ success: true }));
  signedOut.state.testCatalog = [row()];
  signedOut.state.testCatalogName = 'Private query';
  await signedOut.logout();
  assert.equal(signedOut.state.testCatalog, null);
  assert.equal(signedOut.state.testCatalogName, '');
});

test('test catalog escapes metadata and offers existing author screens without quiz actions', () => {
  const controller = setup();
  Object.assign(controller.state, {
    screen: 'test-catalog',
    testCatalogName: '" onfocus="bad',
    testCatalog: [
      { ...row(), title: '<script>bad</script>', creator_first_name: '<img onerror=bad>' },
    ],
  });
  const html = renderView(controller.state);
  assert.doesNotMatch(html, /<script>|<img onerror| onfocus="bad|view-quiz|launch-quiz/);
  assert.match(html, /data-action="my-tests"/);
  assert.match(html, /data-action="create-test"/);
  assert.match(html, /data-action="test-favourite"/);
  assert.match(html, /data-form="test-search"/);
  assert.match(html, /data-nav="test-section"/);
});
