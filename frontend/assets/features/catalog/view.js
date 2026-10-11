import { html, escape } from '../../shared/html.js';
import { icon } from '../../shared/icons.js';
export function renderCatalog(state, notice) {
  const isTest = state.screen === 'test-catalog';
  const kind = isTest ? 'test' : 'quiz';
  const prefix = kind + 'Catalog';
  const rows = state[prefix];
  return html`<button type="button" class="text-button back" data-nav="${kind}-section">
      ← ${isTest ? 'Тесты' : 'Квизы'}
    </button>
    <p class="step-label">Библиотека заданий</p>
    <h2 tabindex="-1">Посмотреть ${isTest ? 'тесты' : 'квизы'}</h2>
    <p class="subtitle">
      ${isTest ? 'Тесты' : 'Квизы'} ваших учебных заведений. Ищите по названию и сохраняйте в
      избранное.
    </p>
    <div class="catalog-toolbar">
      <button type="button" class="text-button" data-action="${isTest ? 'my-tests' : 'my-quizzes'}">
        Мои ${isTest ? 'тесты' : 'квизы'} →</button
      ><button type="button" class="text-button" data-action="create-${kind}">
        ＋ Создать ${isTest ? 'тест' : 'квиз'}
      </button>
    </div>
    <form data-form="${kind}-search" class="catalog-search">
      <label for="${kind}-query">Название ${isTest ? 'теста' : 'квиза'}</label>
      <div class="catalog-search-line">
        <input
          id="${kind}-query"
          name="${kind}-query"
          type="search"
          value="${escape(state[prefix + 'Name'])}"
          placeholder="Например, история или математика"
        /><button class="primary" type="submit">Найти</button>
      </div>
      <label class="catalog-filter"
        ><input
          type="checkbox"
          name="${kind}-favourites"
          ${state[prefix + 'Favourites'] ? 'checked' : ''}
        />
        Только избранное</label
      >
    </form>
    ${notice}
    ${
      rows === null
        ? state.busy
          ? html`<p class="list-loading" role="status">Ищем ${isTest ? 'тесты' : 'квизы'}…</p>`
          : html`<button type="button" class="secondary" data-action="${kind}-catalog-retry">
              Повторить поиск
            </button>`
        : html`<p class="list-count" role="status">
              Найдено${state[prefix + 'More'] ? ' не менее' : ''}: ${rows.length}
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
                          ${
                            isTest
                              ? ''
                              : html`<div class="catalog-item-actions">
                                  <button
                                    type="button"
                                    class="secondary"
                                    data-action="view-quiz"
                                    data-id="${escape(q.quiz_id)}"
                                  >
                                    Посмотреть квиз
                                  </button>
                                  <button
                                    type="button"
                                    class="primary"
                                    data-action="launch-quiz"
                                    data-id="${escape(q.quiz_id)}"
                                  >
                                    Создать сессию
                                  </button>
                                </div>`
                          }
                        </div>
                        <button
                          type="button"
                          class="catalog-favourite"
                          data-action="${kind}-favourite"
                          data-id="${escape(q[kind + '_id'])}"
                          aria-pressed="${q.is_favourite ? 'true' : 'false'}"
                          aria-label="${q.is_favourite ? 'Убрать из избранного' : 'Добавить в избранное'}: ${escape(q.title)}"
                        >
                          ${q.is_favourite ? '★' : '☆'}
                        </button>
                      </article>`,
                  )
                  .join('') ||
                html`<p class="empty-list">
                  ${isTest ? 'Тесты' : 'Квизы'} не найдены. Попробуйте другое название или отключите
                  фильтр избранного.
                </p>`
              }
            </div>
            ${state[prefix + 'More'] ? html`<button type="button" class="secondary" data-action="${kind}-catalog-more">Показать ещё</button>` : ''}`
    }`;
}
