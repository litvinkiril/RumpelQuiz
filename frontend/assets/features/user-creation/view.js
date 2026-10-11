import { html, escape } from '../../shared/html.js';
import { personName, roleNames } from '../../shared/people.js';
import { userRoles } from './model.js';

function facultiesField(state) {
  const faculties = state.userFaculties;
  return html`<div class="field user-faculty-field">
    <label for="new-user-faculty">Факультет</label>
    ${
      faculties === null
        ? state.busy
          ? '<p class="hint" role="status">Загружаем факультеты…</p>'
          : '<button type="button" class="secondary" data-action="user-faculties-retry">Загрузить факультеты</button>'
        : faculties.length
          ? html`<select
              id="new-user-faculty"
              name="facultet_id"
              data-user-field="facultet_id"
              required
              aria-describedby="faculty-hint"
            >
              <option value="">Выберите факультет</option>
              ${faculties.map((faculty) => html`<option value="${escape(faculty.id)}" ${state.newUserDraft.facultet_id === faculty.id ? 'selected' : ''}>${escape(faculty.name)}</option>`).join('')}
            </select>`
          : '<p class="empty-list">В этом вузе пока нет факультетов. Для создания администратора нужен факультет.</p>'
    }
    <p id="faculty-hint" class="hint">Администратор назначается на один факультет.</p>
    ${faculties !== null ? '<button type="button" class="text-button" data-action="user-faculties-retry">Обновить список</button>' : ''}
  </div>`;
}

export function renderUserCreation(state, notice) {
  const draft = state.newUserDraft,
    created = state.createdUser;
  const roles = userRoles(state.selectedUniversity?.adminScope);
  return html`<button type="button" class="text-button back" data-nav="university">
      ← К разделам вуза
    </button>
    <p class="step-label university-label">${escape(state.selectedUniversity?.name)}</p>
    <h2 tabindex="-1">${created ? 'Пользователь создан' : 'Создать пользователя'}</h2>
    ${notice}
    ${
      created
        ? html`<section class="user-created">
              <h3>${escape(personName(created))}</h3>
              <p>${escape(created.email)}</p>
              <span class="quiz-badge ready">${escape(roleNames[created.role])}</span>
            </section>
            <div class="user-form-actions">
              <button type="button" class="primary" data-action="create-user">
                Создать ещё пользователя
              </button>
              <button type="button" class="secondary" data-nav="university">К разделам вуза</button>
            </div>`
        : draft
          ? html`<p class="subtitle">
                Заполните данные для входа и выберите роль в учебном заведении.
              </p>
              <form data-form="user-create" class="user-create-form">
                <fieldset ${state.busy ? 'disabled' : ''}>
                  <div class="user-name-fields">
                    <div class="field">
                      <label for="new-user-first">Имя</label>
                      <input
                        id="new-user-first"
                        name="first_name"
                        data-user-field="first_name"
                        value="${escape(draft.first_name)}"
                        required
                        autocomplete="off"
                        placeholder="Иван"
                      />
                    </div>
                    <div class="field">
                      <label for="new-user-last">Фамилия</label>
                      <input
                        id="new-user-last"
                        name="last_name"
                        data-user-field="last_name"
                        value="${escape(draft.last_name)}"
                        required
                        autocomplete="off"
                        placeholder="Иванов"
                      />
                    </div>
                    <div class="field">
                      <label for="new-user-middle"
                        >Отчество <span class="hint">необязательно</span></label
                      >
                      <input
                        id="new-user-middle"
                        name="middle_name"
                        data-user-field="middle_name"
                        value="${escape(draft.middle_name)}"
                        autocomplete="off"
                        placeholder="Иванович"
                      />
                    </div>
                  </div>
                  <div class="user-form-grid">
                    <div class="field">
                      <label for="new-user-email">Email для входа</label>
                      <input
                        id="new-user-email"
                        name="email"
                        data-user-field="email"
                        type="email"
                        value="${escape(draft.email)}"
                        maxlength="254"
                        required
                        autocomplete="off"
                        placeholder="student@example.com"
                      />
                    </div>
                    <div class="field">
                      <label for="new-user-password">Пароль</label>
                      <div class="input-wrap">
                        <input
                          id="new-user-password"
                          name="new-user-password"
                          data-user-field="password"
                          data-password
                          type="password"
                          required
                          autocomplete="new-password"
                          aria-describedby="new-user-password-hint"
                        />
                        <button
                          type="button"
                          class="show-password"
                          data-toggle="new-user-password"
                          aria-label="Показать пароль"
                          aria-pressed="false"
                        >
                          Показать
                        </button>
                      </div>
                      <p class="hint" id="new-user-password-hint">
                        От 1 до 72 байт. Пробелы в пароле сохраняются.
                      </p>
                    </div>
                    <div class="field">
                      <label for="new-user-role">Роль</label>
                      <select id="new-user-role" name="role" data-user-role>
                        ${roles.map((role) => html`<option value="${role}" ${draft.role === role ? 'selected' : ''}>${roleNames[role]}</option>`).join('')}
                      </select>
                      ${state.selectedUniversity?.adminScope === 'faculties' ? '<p class="hint">Вы можете создавать администраторов своих факультетов.</p>' : ''}
                    </div>
                    ${draft.role === 'admin' ? facultiesField(state) : ''}
                  </div>
                  <div class="field">
                    <label for="new-user-description"
                      >Описание <span class="hint">необязательно</span></label
                    >
                    <textarea
                      id="new-user-description"
                      name="description"
                      data-user-field="description"
                      rows="4"
                      placeholder="Дополнительная информация о пользователе"
                    >
${escape(draft.description)}</textarea>
                  </div>
                  <div class="user-form-actions">
                    <button
                      type="submit"
                      class="primary"
                      ${draft.role === 'admin' && !state.userFaculties?.length ? 'disabled' : ''}
                    >
                      Создать пользователя
                    </button>
                    <button type="button" class="secondary" data-nav="university">Отмена</button>
                  </div>
                </fieldset>
              </form>`
          : ''
    }`;
}
