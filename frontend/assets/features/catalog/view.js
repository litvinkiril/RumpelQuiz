import { html, escape } from '../../shared/html.js';
import { icon } from '../../shared/icons.js';
export function renderCatalog(state, notice) {
  const rows = state.quizCatalog;
  return html`<button type="button" class="text-button back" data-nav="quiz-section">
      ← Квизы
    </button>
    <p class="step-label">Библиотека заданий</p>
    <h2 tabindex="-1">Посмотреть квизы</h2>
    <p class="subtitle">
      Квизы ваших учебных заведений. Ищите по названию и сохраняйте в избранное.
    </p>
    <div class="catalog-toolbar">
      <button type="button" class="text-button" data-action="my-quizzes">Мои квизы →</button
      ><button type="button" class="text-button" data-action="create-quiz">＋ Создать квиз</button>
    </div>
    <form data-form="quiz-search" class="catalog-search">
      <label for="quiz-query">Название квиза</label>
      <div class="catalog-search-line">
        <input
          id="quiz-query"
          name="quiz-query"
          type="search"
          value="${escape(state.quizCatalogName)}"
          placeholder="Например, история или математика"
        /><button class="primary" type="submit">Найти</button>
      </div>
      <label class="catalog-filter"
        ><input
          type="checkbox"
          name="quiz-favourites"
          ${state.quizCatalogFavourites ? 'checked' : ''}
        />
        Только избранное</label
      >
    </form>
    ${notice}
    ${
      rows === null
        ? state.busy
          ? '<p class="list-loading" role="status">Ищем квизы…</p>'
          : '<button type="button" class="secondary" data-action="quiz-catalog-retry">Повторить поиск</button>'
        : html`<p class="list-count" role="status">
              Найдено${state.quizCatalogMore ? ' не менее' : ''}: ${rows.length}
            </p>
            <div class="catalog-list">
              ${
                rows
                  .map(
                    (q) =>
                      html`<article class="catalog-item">
                        <span class="catalog-item-icon">${icon('edit')}</span>
                        <div>
                          <h3>${escape(q.title || 'Без названия')}</h3>
                          <p>
                            ${escape([q.creator_first_name, q.creator_last_name].filter(Boolean).join(' ') || 'Автор не указан')}
                          </p>
                          <span class="quiz-badge">Вопросов: ${escape(q.question_count)}</span>
                        </div>
                        <button
                          type="button"
                          class="catalog-favourite"
                          data-action="quiz-favourite"
                          data-id="${escape(q.quiz_id)}"
                          aria-pressed="${q.is_favourite ? 'true' : 'false'}"
                          aria-label="${q.is_favourite ? 'Убрать из избранного' : 'Добавить в избранное'}: ${escape(q.title)}"
                        >
                          ${q.is_favourite ? '★' : '☆'}
                        </button>
                      </article>`,
                  )
                  .join('') ||
                '<p class="empty-list">Квизы не найдены. Попробуйте другое название или отключите фильтр избранного.</p>'
              }
            </div>
            ${state.quizCatalogMore ? '<button type="button" class="secondary" data-action="quiz-catalog-more">Показать ещё</button>' : ''}`
    }`;
}
