import {
  newTest,
  newTestQuestion,
  testQuestionComplete,
  testPayload,
  testSeconds,
} from './model.js';
import { newAnswer } from '../quizzes/model.js';
export function createTestController({
  state,
  authorized,
  run,
  emit,
  fail,
  now,
  origin,
  ApiError,
  testLink = '',
}) {
  let linkOpened = false;
  const authorUniversities = (profile) => [
    ...new Map(
      (profile.university_position || [])
        .filter((p) => ['teacher', 'admin'].includes(p.role))
        .map((p) => [p.university_id, { id: p.university_id, name: p.university_name }]),
    ).values(),
  ];
  const mutable = () =>
    !state.busy && state.screen === 'test' && state.testDraft?.status === 'draft';
  const applyPlay = (value) => {
    if (!value?.success || !value.attempt || !value.test)
      throw new ApiError('Не удалось загрузить прохождение.');
    if (state.testPlay?.current_question?.id !== value.current_question?.id) state.testChoices = [];
    state.testPlay = value;
    state.testReceivedAt = now();
    state.screen = 'test-play';
  };
  const controller = {
    resetTestView() {
      Object.assign(state, {
        testDraft: null,
        testPlay: null,
        testChoices: [],
        testResults: null,
        testResultsId: null,
        tests: [],
        availableTests: [],
        testUniversities: [],
        testDirty: false,
        testErrors: [],
        testUploading: false,
        testShareUrl: '',
        testLink: '',
      });
    },
    openTests(create = false) {
      if (!state.token || state.busy) return Promise.resolve(false);
      return run(
        async () => {
          const universities = authorUniversities(await authorized('profile'));
          if (!universities.length)
            throw new ApiError(
              'Создавать тесты могут преподаватели и администраторы выбранного вуза.',
            );
          return { universities, tests: (await authorized('tests')).tests };
        },
        (result) => {
          state.testUniversities = result.universities;
          state.tests = result.tests;
          state.testErrors = [];
          state.testDirty = false;
          state.screen = create ? 'test' : 'tests';
          if (create) controller.newTest();
        },
      );
    },
    newTest() {
      if (!state.testUniversities?.length) return;
      state.testDraft = newTest(
        state.testUniversities.length === 1 ? state.testUniversities[0].id : '',
      );
      state.testDirty = true;
      state.testErrors = [];
      state.error = '';
      state.success = '';
      state.screen = 'test';
      emit();
    },
    loadTest(id) {
      if (state.busy || !state.token) return Promise.resolve(false);
      return run(
        () => authorized('tests/' + encodeURIComponent(id)),
        (result) => {
          state.testDraft = result.test;
          state.testDirty = false;
          state.testErrors = [];
          state.screen = 'test';
          state.testShareUrl = origin + '/?test=' + encodeURIComponent(id);
        },
      );
    },
    editTest(field, value, qi, ai) {
      if (!mutable()) return;
      const d = state.testDraft,
        q = d.questions[qi],
        target = ai === undefined ? q || d : q?.answers[ai];
      if (!target) return;
      if (field === 'is_correct' && q.type === 'single')
        q.answers.forEach((a) => {
          a.is_correct = false;
        });
      target[field] = value;
      if (field === 'type' && value === 'single') {
        const first = q.answers.findIndex((a) => a.is_correct);
        q.answers.forEach((a, i) => {
          a.is_correct = i === first;
        });
      }
      state.testDirty = true;
      state.testErrors = [];
      state.success = '';
      if (field === 'type') emit();
    },
    changeTest(action, qi, ai) {
      if (!mutable()) return;
      const d = state.testDraft,
        q = d.questions[qi];
      if (action === 'test-add-question') {
        if (d.questions.length >= 100 || !d.questions.every(testQuestionComplete)) return;
        d.questions.push(newTestQuestion());
      }
      if (action === 'test-remove-question' && d.questions.length > 1) d.questions.splice(qi, 1);
      if (action === 'test-add-answer' && q?.answers.length < 20) q.answers.push(newAnswer());
      if (action === 'test-remove-answer' && q?.answers.length > 2) q.answers.splice(ai, 1);
      if (action === 'test-remove-image') {
        const target = ai === undefined ? q : q?.answers[ai];
        if (target) Object.assign(target, { image_id: null, image_url: '', upload_error: '' });
      }
      state.testDirty = true;
      state.testErrors = [];
      state.success = '';
      emit();
    },
    saveTest(status) {
      if (!mutable()) return Promise.resolve(false);
      const d = state.testDraft;
      if (!d.university_id) return fail('Выберите учебное заведение.');
      const time = Number(d.time_to_complete);
      if (!Number.isInteger(time) || time <= 0 || time > 2147483647)
        return fail('Укажите целое положительное время на весь тест в секундах.');
      if (!['draft', 'public', 'private'].includes(status)) return Promise.resolve(false);
      return run(
        () =>
          authorized(
            'tests' + (d.id ? '/' + d.id : ''),
            testPayload(d, status),
            d.id ? 'PUT' : 'POST',
          ),
        (result) => {
          d.id = result.test_id;
          d.revision = result.revision;
          d.status = result.status;
          state.testDirty = false;
          state.testShareUrl = origin + '/?test=' + encodeURIComponent(d.id);
          state.success =
            result.status === 'draft'
              ? 'Черновик сохранён.'
              : 'Тест опубликован. Студенты могут начать прохождение.';
        },
      );
    },
    uploadTestImage(file, qi, ai) {
      if (!mutable() || !file) return Promise.resolve(false);
      const target =
        ai === undefined
          ? state.testDraft.questions[qi]
          : state.testDraft.questions[qi]?.answers[ai];
      if (!target) return Promise.resolve(false);
      if (file.size > 5242880) return fail('Максимальный размер картинки — 5 МиБ.');
      if (!['image/png', 'image/jpeg', 'image/webp'].includes(file.type))
        return fail('Поддерживаются PNG, JPEG и WebP.');
      state.testUploading = true;
      target.upload_error = '';
      const form = new FormData();
      form.append('file', file);
      return run(
        () => authorized('media/images', form),
        (result) => {
          target.image_id = result.media_id;
          target.image_url = result.image_url;
          state.testDirty = true;
        },
      ).then((ok) => {
        state.testUploading = false;
        if (!ok && state.screen === 'test') target.upload_error = state.error;
        emit();
        return ok;
      });
    },
    openAvailableTests() {
      if (!state.token || state.busy) return Promise.resolve(false);
      return run(
        () => authorized('tests/available'),
        (result) => {
          state.availableTests = result.tests;
          state.testLink = testLink;
          state.screen = 'available-tests';
        },
      );
    },
    resumeTestLink() {
      if (!testLink || linkOpened) return Promise.resolve(false);
      linkOpened = true;
      return controller.openAvailableTests();
    },
    startTest(id) {
      if (!state.token || state.busy) return Promise.resolve(false);
      const text = String(id || '').trim();
      let testId = text;
      if (/^https?:/.test(text)) {
        try {
          testId = new URL(text).searchParams.get('test') || '';
        } catch {
          testId = '';
        }
      }
      if (!/^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/i.test(testId))
        return fail('Вставьте ссылку на тест или его идентификатор.');
      return run(() => authorized('tests/' + encodeURIComponent(testId) + '/start', {}), applyPlay);
    },
    refreshTest() {
      if (!state.token || state.busy || state.screen !== 'test-play' || !state.testPlay)
        return Promise.resolve(false);
      return run(
        () => authorized('tests/' + encodeURIComponent(state.testPlay.test.id) + '/progress'),
        applyPlay,
      );
    },
    chooseTestAnswer(id, checked) {
      if (
        state.busy ||
        state.screen !== 'test-play' ||
        state.testPlay?.attempt.status !== 'in_progress' ||
        testSeconds(state, now()) <= 0
      )
        return;
      const q = state.testPlay.current_question;
      if (!q.answers.some((a) => a.id === id)) return;
      state.testChoices =
        q.type === 'single'
          ? checked
            ? [id]
            : []
          : checked
            ? [...new Set([...(state.testChoices || []), id])]
            : (state.testChoices || []).filter((a) => a !== id);
    },
    submitTestAnswer() {
      if (
        state.busy ||
        state.screen !== 'test-play' ||
        state.testPlay?.attempt.status !== 'in_progress'
      )
        return Promise.resolve(false);
      if (testSeconds(state, now()) <= 0) return controller.refreshTest();
      if (!state.testChoices?.length) return fail('Выберите ответ.');
      const d = state.testPlay;
      return run(
        () =>
          authorized('tests/' + encodeURIComponent(d.test.id) + '/answers', {
            question_id: d.current_question.id,
            answer_ids: [...state.testChoices],
          }),
        applyPlay,
      ).then(async (ok) => {
        if (!ok && state.screen === 'test-play' && state.errorCode === 'test_attempt_finished')
          await controller.refreshTest();
        return ok;
      });
    },
    openTestResults(id = state.testResultsId) {
      if (state.busy || !state.token || !id) return Promise.resolve(false);
      return run(
        () => authorized('tests/' + encodeURIComponent(id) + '/results'),
        (result) => {
          state.testResults = result.results;
          state.testResultsId = id;
          state.screen = 'test-results';
        },
      );
    },
  };
  return controller;
}
