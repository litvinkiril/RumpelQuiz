import { html, escapeHtml } from '../../shared/html.js';
import { icon } from '../../shared/icons.js';

export function educationSearchPlaceholder(kind) {
  return kind === 'programs' ? 'Введите название программы…' : 'Введите название или номер группы…';
}

export function renderEducationSearchResults(state) {
  if (state.educationSearchLoading)
    return '<p class="university-search-status" role="status">Ищем…</p>';
  if (state.educationSearchError)
    return html`<div class="university-search-error" role="alert">
      <p>${escapeHtml(state.educationSearchError)}</p>
      <button type="button" class="text-button" data-action="education-search-retry">
        Повторить поиск
      </button>
    </div>`;
  const items = state.educationSearchItems;
  if (!items) return '';
  if (!items.length)
    return '<p class="university-search-status" role="status">Ничего не найдено. Попробуйте другое название.</p>';
  const groups = state.educationSearchKind !== 'programs';
  return html`<p class="university-search-status" role="status">Найдено: ${items.length}</p>
    <ul
      class="university-search-list"
      aria-label="${groups ? 'Найденные группы' : 'Найденные программы'}"
    >
      ${items
        .map((item) => {
          const contents = html`
            <span class="university-search-item-icon" aria-hidden="true"
              >${icon(groups ? 'people' : 'document')}</span
            >
            <div>
              <strong>${escapeHtml(item.name)}</strong>
              ${groups ? html`<p>Программа: ${escapeHtml(item.program_name ?? 'Не указана')}</p>` : ''}
              <p>
                Факультеты:
                ${escapeHtml(item.facultet_names.length ? item.facultet_names.join(', ') : 'Не указаны')}
              </p>
            </div>
          `;
          return groups
            ? html`<li>
                <button
                  type="button"
                  class="university-search-item education-group-link"
                  data-action="group"
                  data-group-id="${escapeHtml(item.id)}"
                >
                  ${contents}
                </button>
              </li>`
            : html`<li class="university-search-item">${contents}</li>`;
        })
        .join('')}
    </ul>`;
}

export function renderEducationSearch(state) {
  const kind = state.educationSearchKind || 'groups';
  return html`<section
    class="university-panel university-search"
    aria-labelledby="university-search-title"
  >
    <h3 id="university-search-title">${icon('search')} Поиск</h3>
    <div class="university-search-layout">
      <form class="university-search-controls" data-form="education-search" role="search">
        <div class="university-search-tabs" role="group" aria-label="Тип поиска">
          <label>
            <input
              type="radio"
              name="university-search-type"
              value="groups"
              ${kind === 'groups' ? 'checked' : ''}
            />
            <span>Поиск групп</span>
          </label>
          <label>
            <input
              type="radio"
              name="university-search-type"
              value="programs"
              ${kind === 'programs' ? 'checked' : ''}
            />
            <span>Поиск программ</span>
          </label>
        </div>
        <label class="university-search-field">
          ${icon('search')}
          <input
            type="search"
            name="university-search-query"
            data-education-search-query
            aria-label="Поиск группы или образовательной программы"
            aria-controls="university-search-results"
            placeholder="${educationSearchPlaceholder(kind)}"
            value="${escapeHtml(state.educationSearchQuery || '')}"
            autocomplete="off"
            enterkeyhint="search"
          />
        </label>
      </form>
      <div class="university-popular">
        <h4>Популярные запросы</h4>
        <div class="university-query-list">
          ${[
            ['groups', 'ЭК-2023-01'],
            ['groups', 'ФК-2022-01'],
            ['programs', 'Математическое моделирование'],
            ['programs', 'Прикладная информатика'],
          ]
            .map(
              ([searchKind, query]) =>
                html`<button
                  type="button"
                  class="university-query"
                  data-action="education-search-query"
                  data-kind="${searchKind}"
                  data-query="${escapeHtml(query)}"
                >
                  ${icon('search')}<span>${escapeHtml(query)}</span>
                </button>`,
            )
            .join('')}
        </div>
      </div>
    </div>
    <div
      id="university-search-results"
      class="university-search-results"
      aria-busy="${!!state.educationSearchLoading}"
      ${!state.educationSearchLoading && !state.educationSearchError && !state.educationSearchItems ? 'hidden' : ''}
    >
      ${renderEducationSearchResults(state)}
    </div>
  </section>`;
}
