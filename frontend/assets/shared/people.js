import { html, escapeHtml } from './html.js';
export const roleNames = { admin: 'Администратор', student: 'Студент', teacher: 'Преподаватель' };
export function safeAvatarUrl(value) {
  if (typeof value !== 'string' || !value) return '';
  try {
    const url = new URL(value, 'http://localhost');
    return ['https:', 'http:'].includes(url.protocol) && !url.username && !url.password
      ? value
      : '';
  } catch {
    return '';
  }
}
export const personName = (person) =>
  [person.last_name, person.first_name, person.middle_name].filter(Boolean).join(' ') ||
  'Имя не указано';
export const personAvatar = (person) => {
  const url = safeAvatarUrl(person.avatar_url);
  const initials =
    (person.first_name || '').slice(0, 1) + (person.last_name || '').slice(0, 1) || '?';
  return html`<span class="avatar person-avatar" aria-hidden="true"
    ><span>${escapeHtml(initials)}</span>
    ${url ? html`<img src="${escapeHtml(url)}" alt="" referrerpolicy="no-referrer" data-avatar />` : ''}</span
  >`;
};
export const membershipCard = (position) => {
  const clickable = position.role === 'admin' && position.university_id;
  const contents = html`<span class="university-icon" aria-hidden="true">▥</span
    ><span class="membership-info">
      <span class="membership-name">${escapeHtml(position.university_name)}</span>
      <span class="role-badge"
        >${escapeHtml(
          position.role === 'admin' && position.admin_scope === 'faculties'
            ? 'Администратор факультетов'
            : roleNames[position.role] || position.role,
        )}</span
      >
      ${position.role === 'student' ? html`<span class="membership-group">Группа: ${escapeHtml(position.group_name || 'Не назначена')}</span>` : ''} </span
    >${clickable ? '<span class="membership-arrow" aria-hidden="true">→</span>' : ''}`;
  return clickable
    ? html`<button
        type="button"
        class="membership membership-button"
        data-action="university"
        data-university-id="${escapeHtml(position.university_id)}"
      >
        ${contents}
      </button>`
    : html`<article class="membership">${contents}</article>`;
};
