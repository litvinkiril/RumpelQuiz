import { ApiError } from '../../core/api.js';
import { messages } from '../../core/messages.js';
import { newQuiz, newQuestion, newAnswer, questionComplete, quizPayload } from './model.js';
export function createQuizController({ state, authorized, run, emit, fail }) {
  return {
    openQuizzes(create = false) {
      if (!state.token || state.busy) return Promise.resolve(false);
      return run(
        async () => {
          const profile = await authorized('profile');
          const universities = [
            ...new Map(
              (profile.university_position || [])
                .filter((p) => ['teacher', 'admin'].includes(p.role))
                .map((p) => [p.university_id, { id: p.university_id, name: p.university_name }]),
            ).values(),
          ];
          if (!universities.length) throw new ApiError(messages.quiz_access_denied);
          const list = await authorized('quizzes');
          return { universities, quizzes: list.quizzes };
        },
        (result) => {
          state.quizUniversities = result.universities;
          state.quizzes = result.quizzes;
          state.quizErrors = [];
          state.quizDirty = false;
          state.selectedQuiz = null;
          state.historyQuiz = null;
          state.quizSessions = null;
          state.sessionResults = null;
          state.screen = create ? 'quiz' : 'quizzes';
          if (create) {
            state.quizDraft = newQuiz(
              result.universities.length === 1 ? result.universities[0].id : '',
            );
            state.quizDirty = true;
          }
        },
      );
    },
    openQuizActions(id) {
      if (state.busy || !state.token || state.screen !== 'quizzes') return;
      state.selectedQuiz =
        state.quizzes?.find((quiz) => quiz.id === id && quiz.status === 'ready') || null;
      state.error = '';
      state.success = '';
      emit();
    },
    closeQuizActions() {
      state.selectedQuiz = null;
      emit();
    },
    openQuizSessions(id = state.historyQuiz?.id) {
      if (state.busy || !state.token) return Promise.resolve(false);
      const quiz = state.quizzes?.find((quiz) => quiz.id === id);
      if (!quiz) return Promise.resolve(false);
      state.selectedQuiz = null;
      state.historyQuiz = quiz;
      state.quizSessions = null;
      state.sessionResults = null;
      state.screen = 'quiz-sessions';
      return run(
        () => authorized('quizzes/' + encodeURIComponent(id) + '/sessions'),
        (result) => {
          if (!result?.success || !Array.isArray(result.sessions))
            throw new ApiError('Не удалось загрузить сессии. Попробуйте ещё раз.');
          state.quizSessions = result.sessions;
        },
      );
    },
    openSessionResults(id = state.resultsSession?.session_id) {
      if (state.busy || !state.token) return Promise.resolve(false);
      const session = state.quizSessions?.find((item) => item.session_id === id);
      if (!session) return Promise.resolve(false);
      state.resultsSession = session;
      state.sessionResults = null;
      state.screen = 'quiz-results';
      return run(
        () => authorized('game/sessions/' + encodeURIComponent(id) + '/results'),
        (result) => {
          if (!result?.success || result.session_id !== id || !Array.isArray(result.results))
            throw new ApiError('Не удалось загрузить результаты. Попробуйте ещё раз.');
          state.sessionResults = result.results;
        },
      );
    },
    newQuiz() {
      if (state.busy || !state.quizUniversities?.length) return;
      state.quizDraft = newQuiz(
        state.quizUniversities.length === 1 ? state.quizUniversities[0].id : '',
      );
      state.quizDirty = true;
      state.quizErrors = [];
      state.error = '';
      state.success = '';
      state.screen = 'quiz';
      emit();
    },
    loadQuiz(id, screen = 'quiz') {
      if (state.busy || !state.token) return Promise.resolve(false);
      if (!['quiz', 'quiz-preview', 'quiz-launch'].includes(screen)) return Promise.resolve(false);
      const returnScreen = ['quiz-preview', 'quiz-launch'].includes(state.screen)
        ? state.quizReturnScreen
        : state.screen === 'quiz-catalog'
          ? 'quiz-catalog'
          : 'quizzes';
      return run(
        () => authorized('quizzes/' + encodeURIComponent(id)),
        (result) => {
          if (!result?.quiz || !Array.isArray(result.quiz.questions))
            throw new ApiError('Не удалось загрузить квиз. Попробуйте ещё раз.');
          if (screen === 'quiz-launch' && result.quiz.status !== 'ready')
            throw new ApiError('Создать сессию можно только для опубликованного квиза.');
          state.selectedQuiz = null;
          state.quizDraft = result.quiz;
          state.quizDirty = false;
          state.quizErrors = [];
          state.quizReturnScreen = returnScreen;
          state.screen = screen;
        },
      );
    },
    returnFromQuiz() {
      if (state.busy || !state.token) return;
      state.screen = state.quizReturnScreen === 'quiz-catalog' ? 'quiz-catalog' : 'quizzes';
      state.error = '';
      state.success = '';
      emit();
    },
    editQuiz(field, value, qi, ai) {
      if (state.busy || state.screen !== 'quiz') return;
      if (state.quizDraft.status === 'ready') return;
      const d = state.quizDraft,
        q = d.questions[qi];
      const target = ai === undefined ? q || d : q?.answers[ai];
      if (!target) return;
      if (field === 'is_correct' && q.type === 'single')
        q.answers.forEach((a) => {
          a.is_correct = false;
        });
      target[field] = field === 'time_seconds' ? (value === '' ? null : value) : value;
      if (field === 'type' && value === 'single') {
        const first = q.answers.findIndex((a) => a.is_correct);
        q.answers.forEach((a, i) => {
          a.is_correct = i === first;
        });
      }
      state.quizDirty = true;
      state.success = '';
      state.quizErrors = [];
      if (field === 'type') emit();
    },
    changeQuiz(action, qi, ai) {
      if (state.busy || state.screen !== 'quiz') return;
      if (state.quizDraft.status === 'ready') return;
      const d = state.quizDraft,
        q = d.questions[qi];
      if (action === 'quiz-add-question') {
        if (
          d.questions.length >= 100 ||
          !d.questions.every((q) => questionComplete(q, d.default_time_seconds))
        )
          return;
        d.questions.push(newQuestion());
      }
      if (action === 'quiz-remove-question' && d.questions.length > 1) d.questions.splice(qi, 1);
      if (action === 'quiz-add-answer' && q.answers.length < 20) q.answers.push(newAnswer());
      if (action === 'quiz-remove-answer' && q.answers.length > 2) q.answers.splice(ai, 1);
      if (action === 'quiz-remove-image') {
        const target = ai === undefined ? q : q.answers[ai];
        target.image_id = null;
        target.image_url = '';
        target.upload_error = '';
      }
      state.quizDirty = true;
      state.quizErrors = [];
      state.success = '';
      emit();
    },
    saveQuiz(status) {
      if (state.busy || state.screen !== 'quiz') return Promise.resolve(false);
      if (state.quizDraft.status === 'ready') return Promise.resolve(false);
      const d = state.quizDraft;
      if (!d.university_id) return fail('Выберите учебное заведение.');
      if (
        !(Number(d.default_time_seconds) > 0) ||
        d.questions.some((q) => q.time_seconds !== null && !(Number(q.time_seconds) > 0))
      )
        return fail(
          'Время должно быть больше нуля. Пустое время вопроса означает время по умолчанию.',
        );
      state.quizErrors = [];
      return run(
        () =>
          authorized(
            'quizzes' + (d.id ? '/' + d.id : ''),
            quizPayload(d, status),
            d.id ? 'PUT' : 'POST',
          ),
        (result) => {
          d.id = result.quiz_id;
          d.revision = result.revision;
          d.status = result.status;
          state.quizDirty = false;
          state.success =
            result.status === 'draft'
              ? 'Черновик сохранён. Его можно открыть в «Мои квизы».'
              : 'Квиз опубликован. Редактирование больше недоступно.';
        },
      );
    },
    uploadQuizImage(file, qi, ai) {
      if (state.busy || state.screen !== 'quiz' || !file) return Promise.resolve(false);
      if (state.quizDraft.status === 'ready') return Promise.resolve(false);
      const q = state.quizDraft.questions[qi],
        target = ai === undefined ? q : q.answers[ai];
      if (file.size > 5242880) return fail('Максимальный размер картинки — 5 МиБ.');
      if (!['image/png', 'image/jpeg', 'image/webp'].includes(file.type))
        return fail(messages.unsupported_image_format);
      target.upload_error = '';
      state.quizUploading = true;
      const form = new FormData();
      form.append('file', file);
      return run(
        () => authorized('media/images', form),
        (result) => {
          target.image_id = result.media_id;
          target.image_url = result.image_url;
          state.quizDirty = true;
        },
      ).then((ok) => {
        state.quizUploading = false;
        if (!ok && state.screen === 'quiz') target.upload_error = state.error;
        emit();
        return ok;
      });
    },
  };
}
