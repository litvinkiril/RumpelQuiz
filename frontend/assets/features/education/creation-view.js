import { html, escapeHtml } from '../../shared/html.js';
import { educationCreationSections } from './creation-model.js';

function options(items, selectedId, placeholder) {
  return html`<option value="">${placeholder}</option>
    ${items.map((item) => html`<option value="${escapeHtml(item.id)}" ${item.id === selectedId ? 'selected' : ''}>${escapeHtml(item.name)}</option>`).join('')}`;
}

export function renderEducationCreation(state, notice) {
  const kind = state.educationCreationKind;
  const section = educationCreationSections[kind];
  const draft = state.educationCreationDraft;
  const faculties = state.educationCreationFaculties;
  const programs = state.educationCreationPrograms;
  const facultyLoading = state.busy && state.educationCreationRequest === 'faculties';
  const programLoading = state.busy && state.educationCreationRequest === 'programs';
  const canSubmit =
    kind === 'faculties' ||
    (faculties?.some((item) => item.id === draft?.faculty_id) &&
      (kind !== 'groups' || programs?.some((item) => item.id === draft?.program_id)));
  return html`<button type="button" class="text-button back" data-nav="university">
      ← К университету
    </button>
    <p class="step-label university-label">${escapeHtml(state.selectedUniversity?.name || '')}</p>
    <h2 tabindex="-1">${section.title}</h2>
    <p class="subtitle">${section.description}</p>
    ${notice}
    ${
      state.educationCreationSaved
        ? html`<section class="education-created" aria-label="Результат создания">
            <h3>${escapeHtml(state.educationCreationSaved.name)}</h3>
            <p>Сохранено в университете «${escapeHtml(state.selectedUniversity.name)}».</p>
            <div class="education-form-actions">
              <button
                type="button"
                class="primary"
                data-action="education-catalog"
                data-kind="${kind}"
              >
                ${section.list}
              </button>
              <button
                type="button"
                class="secondary"
                data-action="education-create"
                data-kind="${kind}"
              >
                Создать ещё
              </button>
            </div>
          </section>`
        : html`<form class="education-create-form" data-form="education-create">
            <fieldset ${state.busy ? 'disabled' : ''}>
              <div class="field">
                <label for="education-new-name">${section.nameLabel}</label>
                <input
                  id="education-new-name"
                  name="education-name"
                  type="text"
                  data-education-name
                  required
                  placeholder="${section.placeholder}"
                  value="${escapeHtml(draft.name)}"
                />
              </div>
              ${
                kind !== 'faculties'
                  ? html`<div class="field">
                      <div class="field-head">
                        <label for="education-new-faculty">Факультет</label>
                        <button
                          type="button"
                          class="text-button"
                          data-action="education-creation-faculties"
                        >
                          Обновить список
                        </button>
                      </div>
                      ${
                        faculties === null
                          ? facultyLoading
                            ? '<p class="list-loading" role="status">Загружаем факультеты…</p>'
                            : '<button type="button" class="secondary" data-action="education-creation-faculties">Загрузить факультеты ещё раз</button>'
                          : faculties.length
                            ? html`<select
                                id="education-new-faculty"
                                name="education-faculty"
                                data-education-faculty
                                required
                              >
                                ${options(faculties, draft.faculty_id, 'Выберите факультет')}
                              </select>`
                            : '<p class="empty-list">В университете пока нет факультетов. Сначала добавьте факультет, затем вернитесь к созданию.</p>'
                      }
                    </div>`
                  : ''
              }
              ${
                kind === 'groups'
                  ? draft.faculty_id
                    ? html`<div class="field">
                        <div class="field-head">
                          <label for="education-new-program">Программа</label>
                          <button
                            type="button"
                            class="text-button"
                            data-action="education-creation-programs"
                          >
                            Обновить список
                          </button>
                        </div>
                        ${
                          programs === null
                            ? programLoading
                              ? '<p class="list-loading" role="status">Загружаем программы факультета…</p>'
                              : '<button type="button" class="secondary" data-action="education-creation-programs">Загрузить программы ещё раз</button>'
                            : programs.length
                              ? html`<select
                                    id="education-new-program"
                                    name="education-program"
                                    data-education-program
                                    required
                                  >
                                    ${options(programs, draft.program_id, 'Выберите программу')}
                                  </select>
                                  <p class="hint">Показаны программы выбранного факультета.</p>`
                              : '<p class="empty-list">На этом факультете пока нет программ. Выберите другой факультет или сначала создайте программу.</p>'
                        }
                      </div>`
                    : '<p class="education-program-hint">Сначала выберите факультет — после этого появится список его программ.</p>'
                  : ''
              }
              <div class="education-form-actions">
                <button type="submit" class="primary" ${!canSubmit ? 'disabled' : ''}>
                  ${state.busy && state.educationCreationRequest === 'create' ? 'Создаём…' : section.submit}
                </button>
                <button type="button" class="secondary" data-nav="university">Отмена</button>
              </div>
            </fieldset>
          </form>`
    }`;
}
