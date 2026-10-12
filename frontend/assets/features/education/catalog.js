import { html, escapeHtml } from '../../shared/html.js';
import { icon } from '../../shared/icons.js';

const sections = {
  faculties: { title: 'Факультеты', symbol: 'university', empty: 'факультетов' },
  programs: { title: 'Программы', symbol: 'document', empty: 'программ' },
  groups: { title: 'Группы', symbol: 'people', empty: 'групп' },
};

function facultyOption(id, name, selectedId) {
  return html`<li>
    <button
      type="button"
      data-action="education-faculty"
      data-faculty-id="${escapeHtml(id)}"
      aria-pressed="${id === selectedId}"
    >
      <span>${escapeHtml(name)}</span>
      ${id === selectedId ? '<span aria-hidden="true">✓</span>' : ''}
    </button>
  </li>`;
}

export function renderEducationCatalog(state, notice) {
  const kind = state.educationCatalogKind;
  const section = sections[kind];
  const items = state.educationCatalogItems;
  const faculties = state.facultyCache?.[state.selectedUniversity?.id];
  const selectedFaculty = faculties?.find(
    (faculty) => faculty.id === state.educationCatalogFacultyId,
  );
  const itemsLoading = state.busy && state.educationCatalogRequest === 'items';
  const facultiesLoading = state.busy && state.educationCatalogRequest === 'faculties';
  return html`<div class="education-catalog">
    <button type="button" class="text-button back" data-nav="university">← К университету</button>
    <p class="step-label university-label">${escapeHtml(state.selectedUniversity?.name || '')}</p>
    <h2 tabindex="-1">${section.title}</h2>
    <p class="subtitle">
      ${
        kind === 'faculties'
          ? 'Все факультеты выбранного университета.'
          : kind === 'programs'
            ? 'Все образовательные программы университета. Выберите факультет, чтобы уточнить список.'
            : 'Все учебные группы университета. Выберите факультет, чтобы уточнить список.'
      }
    </p>
    ${
      kind !== 'faculties'
        ? html`<section class="education-faculty-filter" aria-labelledby="education-faculty-title">
            <div class="education-filter-heading">
              <div>
                <h3 id="education-faculty-title">Факультет</h3>
                <p>
                  ${selectedFaculty ? 'Показаны результаты выбранного факультета.' : 'Сейчас показан весь университет.'}
                </p>
              </div>
              ${
                selectedFaculty
                  ? '<button type="button" class="text-button" data-action="education-faculty" data-faculty-id="">Все факультеты</button>'
                  : ''
              }
            </div>
            <button
              type="button"
              class="education-faculty-toggle"
              data-action="education-faculty-picker"
              aria-expanded="${!!state.educationFacultyPickerOpen}"
              aria-controls="education-faculty-options"
            >
              ${icon('university')}
              <span>${escapeHtml(selectedFaculty?.name || 'Выбрать факультет')}</span>
              <span class="education-filter-chevron" aria-hidden="true">⌄</span>
            </button>
            <div id="education-faculty-options" ${state.educationFacultyPickerOpen ? '' : 'hidden'}>
              ${
                facultiesLoading
                  ? '<p class="list-loading" role="status">Загружаем факультеты…</p>'
                  : faculties
                    ? html`<ul
                          class="education-faculty-options"
                          aria-label="Факультеты университета"
                        >
                          ${facultyOption('', 'Все факультеты', state.educationCatalogFacultyId)}
                          ${faculties.map((faculty) => facultyOption(faculty.id, faculty.name, state.educationCatalogFacultyId)).join('')}
                        </ul>
                        ${faculties.length ? '' : '<p class="hint">В этом университете пока нет факультетов.</p>'}`
                    : '<button type="button" class="secondary education-retry" data-action="education-faculties-retry">Загрузить факультеты ещё раз</button>'
              }
            </div>
          </section>`
        : ''
    }
    ${notice}
    ${
      itemsLoading
        ? '<p class="list-loading" role="status">Загружаем список…</p>'
        : items === null
          ? '<button type="button" class="secondary education-retry" data-action="education-catalog-retry">Загрузить список ещё раз</button>'
          : html`<p class="list-count" role="status">Всего: ${items.length}</p>
              ${
                items.length
                  ? html`<ul class="education-catalog-list" aria-label="${section.title}">
                      ${items
                        .map((item) => {
                          const contents = html`
                            <span class="education-catalog-icon" aria-hidden="true"
                              >${icon(section.symbol)}</span
                            >
                            <span>${escapeHtml(item.name)}</span>
                          `;
                          return kind === 'groups'
                            ? html`<li>
                                <button
                                  type="button"
                                  class="education-catalog-item education-group-link"
                                  data-action="group"
                                  data-group-id="${escapeHtml(item.id)}"
                                >
                                  ${contents}
                                </button>
                              </li>`
                            : html`<li class="education-catalog-item">${contents}</li>`;
                        })
                        .join('')}
                    </ul>`
                  : html`<p class="empty-list">
                      ${selectedFaculty ? 'На выбранном факультете' : 'В этом университете'} пока
                      нет ${section.empty}.
                    </p>`
              }`
    }
  </div>`;
}
