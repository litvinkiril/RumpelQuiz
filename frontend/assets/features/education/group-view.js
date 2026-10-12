import { html, escapeHtml } from '../../shared/html.js';
import { icon } from '../../shared/icons.js';
import { personAvatar, personName } from '../../shared/people.js';
import { groupPermissions } from './group-model.js';

function memberAction(action, symbol, label, id) {
  return html`<button
    type="button"
    class="group-member-action"
    data-group-placeholder="${action}"
    data-user-id="${escapeHtml(id)}"
    aria-label="${escapeHtml(label)}"
    title="${escapeHtml(label)}"
  >
    ${icon(symbol)}
  </button>`;
}

function renderMember(member, student, permissions) {
  const name = personName(member);
  const contents = html`${personAvatar(member)}<span class="group-member-info">
      <strong>${escapeHtml(name)}</strong><span>${student ? 'Ученик' : 'Преподаватель'}</span>
    </span>`;
  return html`<li class="group-member">
    ${
      permissions.clickable
        ? html`<button
            type="button"
            class="group-member-main"
            data-group-placeholder="member"
            data-user-id="${escapeHtml(member.id)}"
          >
            ${contents}${
              permissions.admin
                ? ''
                : '<span class="group-member-chevron" aria-hidden="true">›</span>'
            }
          </button>`
        : html`<div class="group-member-main">${contents}</div>`
    }
    ${
      permissions.admin
        ? html`<div class="group-member-actions">
            ${student ? memberAction('homework', 'book', 'Посмотреть домашку: ' + name, member.id) : ''}
            ${memberAction('remove', 'user-minus', 'Удалить из группы: ' + name, member.id)}
          </div>`
        : ''
    }
  </li>`;
}

function renderMembers(title, kind, members, permissions) {
  const student = kind === 'students';
  return html`<section class="group-section" aria-labelledby="group-${kind}-title">
    <h3 id="group-${kind}-title">
      ${title}<span class="group-member-count">${members.length}</span>
    </h3>
    ${
      members.length
        ? html`<ul class="group-member-list" aria-label="${title}">
            ${members.map((member) => renderMember(member, student, permissions)).join('')}
          </ul>`
        : html`<p class="group-empty">
            ${student ? 'В группе пока нет учеников.' : 'В группе пока нет преподавателей.'}
          </p>`
    }
    ${
      permissions.admin
        ? html`<button type="button" class="group-add-member" data-group-placeholder="add-${kind}">
            <span aria-hidden="true">＋</span
            >${student ? 'Добавить ученика' : 'Добавить преподавателя'}
          </button>`
        : ''
    }
  </section>`;
}

export function renderGroup(state, notice) {
  const permissions = groupPermissions(state);
  return html`<div class="group-page">
    <button type="button" class="text-button back" data-nav="group-return">← Назад</button>
    <p class="step-label university-label">
      ${escapeHtml(state.selectedGroup?.universityName || '')}
    </p>
    <div class="group-header">
      <div class="group-heading">
        <span class="group-symbol" aria-hidden="true">${icon('people')}</span>
        <div>
          <h2 tabindex="-1">${escapeHtml(state.selectedGroup?.name || 'Учебная группа')}</h2>
          <p>Участники группы</p>
        </div>
      </div>
      <button type="button" class="primary group-homework" data-group-placeholder="group-homework">
        ${icon('book')}Посмотреть домашки
      </button>
    </div>
    ${notice}
    ${
      state.busy
        ? '<p class="list-loading" role="status">Загружаем участников группы…</p>'
        : state.groupMembers
          ? html`${renderMembers('Преподаватели', 'teachers', state.groupMembers.teachers, permissions)}
            ${renderMembers('Ученики', 'students', state.groupMembers.students, permissions)}`
          : '<button type="button" class="secondary education-retry" data-action="group-retry">Загрузить участников ещё раз</button>'
    }
  </div>`;
}
