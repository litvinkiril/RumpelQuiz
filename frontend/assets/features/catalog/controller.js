import { ApiError } from '../../core/api.js';
import { messages } from '../../core/messages.js';
function createResourceCatalog({ state, authorized, run, fail }, kind, resource) {
  const rowsKey = kind + 'Catalog';
  const nameKey = rowsKey + 'Name';
  const favouritesKey = rowsKey + 'Favourites';
  const moreKey = rowsKey + 'More';
  const offsetKey = rowsKey + 'Offset';
  const idKey = kind + '_id';
  const screen = kind + '-catalog';
  const search = (name = state[nameKey], favourites = state[favouritesKey], append = false) => {
    if (state.busy || !state.token || state.screen !== screen) return Promise.resolve(false);
    if (append && !state[moreKey]) return Promise.resolve(false);
    if (!append) {
      state[nameKey] = String(name || '').trim();
      state[favouritesKey] = !!favourites;
      state[rowsKey] = null;
      state[offsetKey] = 0;
      state[moreKey] = false;
    }
    const offset = state[offsetKey];
    const params = new URLSearchParams({
      name: state[nameKey],
      favourites: String(state[favouritesKey]),
      count_spend: String(offset),
    });
    return run(
      () => authorized(resource + '/search?' + params),
      (result) => {
        const rows = result?.[resource];
        if (!Array.isArray(rows))
          throw new ApiError(
            `Не удалось загрузить ${kind === 'quiz' ? 'квизы' : 'тесты'}. Попробуйте ещё раз.`,
          );
        state[rowsKey] = [
          ...new Map(
            [...(append ? state[rowsKey] || [] : []), ...rows].map((item) => [item[idKey], item]),
          ).values(),
        ];
        state[offsetKey] = offset + rows.length;
        state[moreKey] = rows.length === 10;
      },
    );
  };

  return {
    search,
    open() {
      if (state.busy || !state.token) return Promise.resolve(false);
      if (!state.accountRoles?.some((role) => ['teacher', 'admin'].includes(role)))
        return fail(messages[kind + '_access_denied']);
      state.screen = screen;
      return search();
    },
    toggleFavourite(id) {
      if (state.busy || !state.token || state.screen !== screen) return Promise.resolve(false);
      const item = state[rowsKey]?.find((row) => row[idKey] === id);
      if (!item) return Promise.resolve(false);
      return run(
        () =>
          authorized(
            resource + '/' + encodeURIComponent(id) + '/favourite',
            undefined,
            item.is_favourite ? 'DELETE' : 'POST',
          ),
        () => {
          item.is_favourite = !item.is_favourite;
          if (state[favouritesKey] && !item.is_favourite) {
            state[rowsKey] = state[rowsKey].filter((row) => row[idKey] !== id);
            state[offsetKey] = Math.max(0, state[offsetKey] - 1);
          }
        },
      );
    },
  };
}

export function createCatalogController(context) {
  const quizzes = createResourceCatalog(context, 'quiz', 'quizzes');
  const tests = createResourceCatalog(context, 'test', 'tests');
  return {
    openQuizCatalog: quizzes.open,
    searchQuizCatalog: quizzes.search,
    toggleQuizFavourite: quizzes.toggleFavourite,
    openTestCatalog: tests.open,
    searchTestCatalog: tests.search,
    toggleTestFavourite: tests.toggleFavourite,
  };
}
