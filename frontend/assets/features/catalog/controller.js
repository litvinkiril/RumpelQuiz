import { ApiError } from '../../core/api.js';
import { messages } from '../../core/messages.js';
export function createCatalogController({ state, authorized, run, fail }) {
  const searchQuizCatalog = (
    name = state.quizCatalogName,
    favourites = state.quizCatalogFavourites,
    append = false,
  ) => {
    if (state.busy || !state.token || state.screen !== 'quiz-catalog')
      return Promise.resolve(false);
    if (append && !state.quizCatalogMore) return Promise.resolve(false);
    if (!append) {
      state.quizCatalogName = String(name || '').trim();
      state.quizCatalogFavourites = !!favourites;
      state.quizCatalog = null;
      state.quizCatalogOffset = 0;
      state.quizCatalogMore = false;
    }
    const offset = state.quizCatalogOffset;
    const params = new URLSearchParams({
      name: state.quizCatalogName,
      favourites: String(state.quizCatalogFavourites),
      count_spend: String(offset),
    });
    return run(
      () => authorized('quizzes/search?' + params),
      (result) => {
        if (!Array.isArray(result?.quizzes))
          throw new ApiError('Не удалось загрузить квизы. Попробуйте ещё раз.');
        state.quizCatalog = [
          ...new Map(
            [...(append ? state.quizCatalog || [] : []), ...result.quizzes].map((q) => [
              q.quiz_id,
              q,
            ]),
          ).values(),
        ];
        state.quizCatalogOffset = offset + result.quizzes.length;
        state.quizCatalogMore = result.quizzes.length === 10;
      },
    );
  };

  return {
    searchQuizCatalog,
    openQuizCatalog() {
      if (state.busy || !state.token) return Promise.resolve(false);
      if (!state.accountRoles?.some((role) => ['teacher', 'admin'].includes(role)))
        return fail(messages.quiz_access_denied);
      state.screen = 'quiz-catalog';
      return searchQuizCatalog();
    },
    toggleQuizFavourite(id) {
      if (state.busy || !state.token || state.screen !== 'quiz-catalog')
        return Promise.resolve(false);
      const quiz = state.quizCatalog?.find((q) => q.quiz_id === id);
      if (!quiz) return Promise.resolve(false);
      return run(
        () =>
          authorized(
            'quizzes/' + encodeURIComponent(id) + '/favourite',
            undefined,
            quiz.is_favourite ? 'DELETE' : 'POST',
          ),
        () => {
          quiz.is_favourite = !quiz.is_favourite;
          if (state.quizCatalogFavourites && !quiz.is_favourite) {
            state.quizCatalog = state.quizCatalog.filter((q) => q.quiz_id !== id);
            state.quizCatalogOffset = Math.max(0, state.quizCatalogOffset - 1);
          }
        },
      );
    },
  };
}
