import { html, escapeHtml } from '../../shared/html.js';
import { roleNames, personName, personAvatar } from '../../shared/people.js';
export function renderEducation(state, notice) {
  const { screen, busy } = state;
  let content;
  if (screen === 'university') {
    content = html`<button type="button" class="text-button back" data-nav="profile">
        ← Назад в профиль
      </button>
      <p class="step-label">
        ${state.selectedUniversity?.adminScope === 'faculties' ? 'Администрирование факультетов' : state.selectedUniversity?.adminScope === 'university' ? 'Управление вузом' : 'Учебное заведение'}
      </p>
      <h2 tabindex="-1">${escapeHtml(state.selectedUniversity?.name || 'Учебное заведение')}</h2>
      <p class="subtitle">Выберите раздел для просмотра и управления.</p>
      ${notice}
      <button type="button" class="secondary" data-action="people">Найти человека в вузе →</button>
      <div class="university-sections">
        <button type="button" class="section-card" data-action="admins">
          <span class="section-icon" aria-hidden="true">♙</span>
          <span
            ><strong>Посмотреть администраторов</strong
            ><span class="section-description">Сотрудники и контакты</span></span
          ><span class="section-arrow" aria-hidden="true">→</span>
        </button>
        ${[
          ['Посмотреть преподавателей', 'Преподаватели и их группы'],
          ['Посмотреть студентов', 'Студенты вашего вуза'],
          ['Посмотреть группы', 'Учебные группы'],
        ]
          .map(
            ([title, description]) => `
          <button type="button" class="section-card" disabled><span class="section-icon" aria-hidden="true">▥</span>
          <span><strong>${title}</strong><span class="section-description">${description}</span></span><span class="section-soon">Скоро</span></button>`,
          )
          .join('')}
      </div>`;
  } else if (screen === 'people') {
    content = html`<button type="button" class="text-button back" data-nav="university">
        ← К разделам вуза
      </button>
      <p class="step-label university-label">${escapeHtml(state.selectedUniversity?.name || '')}</p>
      <h2 tabindex="-1">Поиск людей</h2>
      <p class="subtitle">
        Введите имя, фамилию или отчество в любом порядке. Нажмите на человека, чтобы посмотреть
        почту.
      </p>
      <form data-form="people" role="search">
        <label for="people-query">Кого ищем?</label>
        <div class="people-search">
          <input
            id="people-query"
            name="query"
            type="search"
            required
            aria-describedby="search-hint"
            placeholder="Например, Литвин Кирилл"
            value="${escapeHtml(state.peopleQuery || '')}"
          />
          <button type="submit" class="primary">Найти</button>
        </div>
        <p id="search-hint" class="hint">От 2 до 100 символов. Поиск только в выбранном вузе.</p>
      </form>
      ${notice}${busy ? '<p role="status" class="list-loading">Ищем людей…</p>' : ''}
      ${
        state.people === null
          ? !busy && !state.error
            ? '<p class="empty-list">Начните с имени или фамилии — результаты появятся здесь.</p>'
            : ''
          : `
        <p class="list-count" role="status">По запросу «${escapeHtml(state.peopleQuery)}» показано: ${state.people.length}</p>
        <div class="admin-list">${state.people
          .map(
            (person) =>
              html`<button
                type="button"
                class="admin-card"
                data-action="person-contact"
                data-membership-id="${escapeHtml(person.user_id)}"
                aria-haspopup="dialog"
              >
                ${personAvatar(person)}<span class="person-info"
                  ><span class="admin-name">${escapeHtml(personName(person))}</span>
                  <span class="person-roles"
                    >${(person.roles || []).map((role) => escapeHtml(roleNames[role] || role)).join(' · ')}</span
                  >
                  ${person.roles?.includes('student') ? html`<span class="person-study">Группа: ${escapeHtml(person.student_details?.group?.name || 'Не назначена')}<br />Факультеты: ${escapeHtml(person.student_details?.faculties?.map((f) => f.name).join(', ') || 'Не указаны')}</span>` : ''} </span
                ><span class="contact-arrow" aria-hidden="true">↗</span>
              </button>`,
          )
          .join('')}</div>
        ${state.people.length ? '' : '<p class="empty-list">Никого не нашли. Попробуйте другую часть имени или проверьте написание.</p>'}
        ${state.nextOffset !== null ? '<button type="button" class="secondary" data-action="more-people">Показать ещё</button>' : ''}`
      }
      ${
        state.selectedAdmin
          ? html`<dialog class="contact-dialog" aria-labelledby="contact-title">
              <button
                type="button"
                class="dialog-close"
                data-action="close-contact"
                aria-label="Закрыть окно"
                autofocus
              >
                ×
              </button>
              ${personAvatar(state.selectedAdmin)}
              <h3 id="contact-title">${escapeHtml(personName(state.selectedAdmin))}</h3>
              <p class="contact-label">Электронная почта</p>
              <a
                class="contact-email"
                href="mailto:${escapeHtml(encodeURIComponent(state.selectedAdmin.email))}"
                >${escapeHtml(state.selectedAdmin.email)}</a
              >
            </dialog>`
          : ''
      }`;
  } else if (screen === 'admins') {
    content = html`<button type="button" class="text-button back" data-nav="university">
        ← К разделам вуза
      </button>
      <p class="step-label university-label">${escapeHtml(state.selectedUniversity?.name || '')}</p>
      <h2 tabindex="-1">Администраторы</h2>
      <p class="subtitle">Нажмите на сотрудника, чтобы посмотреть почту.</p>
      ${notice}${busy ? '<p class="list-loading" role="status">Загружаем администраторов…</p>' : ''}
      ${
        state.admins
          ? html`<p class="list-count">Всего: ${state.admins.length}</p>
              <div class="admin-list">
                ${state.admins
                  .map(
                    (admin) =>
                      html`<button
                        type="button"
                        class="admin-card"
                        data-action="admin-contact"
                        data-membership-id="${escapeHtml(admin.membership_id)}"
                        aria-haspopup="dialog"
                      >
                        ${personAvatar(admin)}<span class="admin-name"
                          >${escapeHtml(personName(admin))}</span
                        ><span class="contact-arrow" aria-hidden="true">↗</span>
                      </button>`,
                  )
                  .join('')}
              </div>
              ${state.admins.length ? '' : '<p class="empty-list">В этом вузе пока нет активных администраторов.</p>'}`
          : !busy
            ? '<button type="button" class="secondary" data-action="admins">Попробовать снова</button>'
            : ''
      }
      ${
        state.selectedAdmin
          ? html`<dialog class="contact-dialog" aria-labelledby="contact-title">
              <button
                type="button"
                class="dialog-close"
                data-action="close-contact"
                aria-label="Закрыть окно"
                autofocus
              >
                ×
              </button>
              ${personAvatar(state.selectedAdmin)}
              <h3 id="contact-title">${escapeHtml(personName(state.selectedAdmin))}</h3>
              <p class="contact-label">Электронная почта</p>
              <a
                class="contact-email"
                href="mailto:${escapeHtml(encodeURIComponent(state.selectedAdmin.email))}"
                >${escapeHtml(state.selectedAdmin.email)}</a
              >
            </dialog>`
          : ''
      }`;
  }
  return content;
}
