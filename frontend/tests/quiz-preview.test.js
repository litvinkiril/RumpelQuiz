import test from 'node:test';
import assert from 'node:assert/strict';
import { createController, createApi, ApiError, renderView } from '../app.js';
import { newQuiz } from '../quiz.js';

const quizId = '33333333-3333-4333-8333-333333333333';
const sessionId = '11111111-1111-4111-8111-111111111111';
function savedQuiz(status = 'ready') {
  const quiz = newQuiz('university');
  Object.assign(quiz, { id: quizId, name: 'Математика', status, description: 'Проверка знаний' });
  Object.assign(quiz.questions[0], { text: 'Сколько будет 2 + 2?', time_seconds: 30 });
  Object.assign(quiz.questions[0].answers[0], { text: '4', is_correct: true });
  quiz.questions[0].answers[1].text = '5';
  return quiz;
}
function setup(api) {
  const controller = createController({ api, makeGameId: () => sessionId });
  Object.assign(controller.state, {
    token: 'token',
    userId: 'host',
    screen: 'quiz-catalog',
    accountRoles: ['teacher'],
    quizCatalogName: 'Математика',
    quizCatalogFavourites: true,
    quizCatalogOffset: 10,
    quizCatalog: [{ quiz_id: quizId, title: 'Математика', question_count: 1, is_favourite: true }],
  });
  return controller;
}

test('each catalog card has independent preview and launch buttons using its quiz id', () => {
  const controller = setup();
  const html = renderView(controller.state);
  assert.match(html, /data-action="view-quiz"\s+data-id="33333333-3333-4333-8333-333333333333"/);
  assert.match(html, /data-action="launch-quiz"\s+data-id="33333333-3333-4333-8333-333333333333"/);
});

test('preview reads the existing endpoint without writes and returns to the filtered catalog', async () => {
  const requests = [];
  const controller = setup(
    createApi(async (url, options) => {
      requests.push({ url, options });
      return { ok: true, json: async () => ({ success: true, quiz: savedQuiz() }) };
    }),
  );
  const rows = controller.state.quizCatalog;
  assert.equal(await controller.loadQuiz(quizId, 'quiz-preview'), true);
  assert.equal(requests.length, 1);
  assert.equal(requests[0].url, '/v1/quizzes/' + quizId);
  assert.equal(requests[0].options.method, 'GET');
  assert.equal(requests[0].options.headers.Authorization, 'Bearer token');
  const html = renderView(controller.state);
  assert.match(html, /Сколько будет 2 \+ 2/);
  assert.match(html, /✓ Правильный ответ/);
  assert.match(html, /30 с/);
  assert.doesNotMatch(html, /data-form="game-create"|data-quiz-field|data-quiz-image/);
  controller.editQuiz('text', 'Changed', 0);
  assert.equal(controller.state.quizDraft.questions[0].text, 'Сколько будет 2 + 2?');
  controller.returnFromQuiz();
  assert.equal(controller.state.screen, 'quiz-catalog');
  assert.equal(controller.state.quizCatalog, rows);
  assert.equal(controller.state.quizCatalogName, 'Математика');
  assert.equal(controller.state.quizCatalogFavourites, true);
  assert.equal(controller.state.quizCatalogOffset, 10);
});

test('launch waits for a name then uses the existing session endpoint and payload', async () => {
  const requests = [];
  const game = {
    success: true,
    server_time_ms: 10000,
    session: {
      id: sessionId,
      quiz_id: quizId,
      join_code: '012345',
      name: 'Группа 1',
      status: 'waiting',
      is_host: true,
      question_count: 1,
      participants_count: 0,
    },
    current_question: null,
    participants: [],
    results: [],
  };
  const controller = setup(
    createApi(async (url, options) => {
      requests.push({ url, options });
      return {
        ok: true,
        json: async () =>
          url === '/v1/quizzes/' + quizId ? { success: true, quiz: savedQuiz() } : game,
      };
    }),
  );
  assert.equal(await controller.loadQuiz(quizId, 'quiz-launch'), true);
  assert.equal(requests.length, 1);
  assert.match(renderView(controller.state), /data-form="game-create"/);
  assert.doesNotMatch(renderView(controller.state), /Сколько будет 2/);
  assert.equal(await controller.createGame(''), false);
  assert.equal(requests.length, 1);
  assert.equal(await controller.createGame('  Группа 1  '), true);
  assert.equal(requests[1].url, '/v1/game/sessions');
  assert.equal(requests[1].options.method, 'POST');
  assert.deepEqual(JSON.parse(requests[1].options.body), {
    quiz_id: quizId,
    session_id: sessionId,
    name: 'Группа 1',
  });
  assert.equal(requests[2].url, '/v1/game/sessions/' + sessionId);
  assert.equal(controller.state.screen, 'game');
});

test('drafts can be previewed without editing; launching them preserves the catalog', async () => {
  const controller = setup(async () => ({ success: true, quiz: savedQuiz('draft') }));
  assert.equal(await controller.loadQuiz(quizId, 'quiz-launch'), false);
  assert.equal(controller.state.screen, 'quiz-catalog');
  assert.match(controller.state.error, /только для опубликованного квиза/);
  assert.equal(await controller.loadQuiz(quizId, 'quiz-preview'), true);
  assert.match(renderView(controller.state), /Черновик/);
  assert.doesNotMatch(renderView(controller.state), /launch-quiz|data-quiz-field/);
  assert.equal(await controller.saveQuiz('ready'), false);
});

test('server access errors and malformed responses leave the catalog available', async () => {
  const controller = setup(async () => {
    throw new ApiError('Нет доступа', 404, 'quiz_not_found');
  });
  assert.equal(await controller.loadQuiz(quizId, 'quiz-preview'), false);
  assert.equal(controller.state.screen, 'quiz-catalog');
  assert.match(renderView(controller.state), /Нет доступа/);
  assert.equal(controller.state.quizCatalog.length, 1);
  const malformed = setup(async () => ({ success: true }));
  assert.equal(await malformed.loadQuiz(quizId, 'quiz-launch'), false);
  assert.equal(malformed.state.screen, 'quiz-catalog');
});

test('preview to launch preserves the return path; navigation ignores a late quiz response', async () => {
  const controller = setup(async () => ({ success: true, quiz: savedQuiz() }));
  await controller.loadQuiz(quizId, 'quiz-preview');
  await controller.loadQuiz(quizId, 'quiz-launch');
  controller.returnFromQuiz();
  assert.equal(controller.state.screen, 'quiz-catalog');
  controller.state.screen = 'quizzes';
  await controller.loadQuiz(quizId, 'quiz-launch');
  controller.returnFromQuiz();
  assert.equal(controller.state.screen, 'quizzes');
  let resolve;
  const pending = setup(
    () =>
      new Promise((done) => {
        resolve = done;
      }),
  );
  const load = pending.loadQuiz(quizId, 'quiz-preview');
  pending.navigate('account');
  resolve({ success: true, quiz: savedQuiz() });
  assert.equal(await load, false);
  assert.equal(pending.state.screen, 'account');
});

test('preview escapes question and answer content and ignores unsafe image URLs', async () => {
  const quiz = savedQuiz();
  quiz.name = '<script>bad</script>';
  quiz.description = '<img onerror=bad>';
  quiz.questions[0].text = '<svg onload=bad>';
  quiz.questions[0].image_url = 'javascript:bad';
  quiz.questions[0].answers[0].text = '<iframe>bad</iframe>';
  quiz.questions[0].answers[0].image_url = 'https://example.test/answer.png';
  const controller = setup(async () => ({ success: true, quiz }));
  await controller.loadQuiz(quizId, 'quiz-preview');
  const html = renderView(controller.state);
  assert.doesNotMatch(html, /<script>|<svg onload|<img onerror|<iframe>|javascript:/);
  assert.match(html, /&lt;svg onload=bad&gt;/);
  assert.match(html, /src="https:\/\/example.test\/answer.png"/);
});
