import { html, escapeHtml } from '../../shared/html.js';
import { safeAvatarUrl, membershipCard } from '../../shared/people.js';
export function renderProfile(state, notice) {
  const { busy } = state;

  const p = state.profile;
  const name = p ? [p.last_name, p.first_name, p.middle_name].filter(Boolean).join(' ') : '';
  const avatar = safeAvatarUrl(p?.avatar_url);
  const description = typeof p?.description === 'string' ? p.description.trim() : '';
  return html`<button type="button" class="text-button back" data-nav="account">
      ← На главную
    </button>
    <p class="step-label">Личный кабинет</p>
    <h2 tabindex="-1">Мой профиль</h2>
    ${notice}${busy ? '<p class="subtitle" role="status">Загружаем профиль…</p>' : ''}
    ${
      p
        ? html`<div class="profile-heading">
              <div class="avatar">
                <span aria-hidden="true"
                  >${escapeHtml((p.first_name || '').slice(0, 1) + (p.last_name || '').slice(0, 1) || 'К')}</span
                >
                ${avatar ? html`<img src="${escapeHtml(avatar)}" alt="Фото профиля" referrerpolicy="no-referrer" data-avatar />` : ''}
              </div>
              <div>
                <h3>${escapeHtml(name || 'Имя не указано')}</h3>
                <p>${escapeHtml(p.email)}</p>
              </div>
            </div>
            <dl class="profile-details">
              <div>
                <dt>Имя</dt>
                <dd>${escapeHtml(p.first_name || 'Не указано')}</dd>
              </div>
              <div>
                <dt>Фамилия</dt>
                <dd>${escapeHtml(p.last_name || 'Не указана')}</dd>
              </div>
              <div>
                <dt>Отчество</dt>
                <dd>${escapeHtml(p.middle_name || 'Не указано')}</dd>
              </div>
            </dl>
            <section class="profile-description" aria-labelledby="profile-description-title">
              <h3 id="profile-description-title" class="section-title">Описание</h3>
              <p>${escapeHtml(description || 'Описание пока не добавлено.')}</p>
            </section>
            <h3 class="section-title">Мои учебные заведения</h3>
            <div class="membership-list">
              ${p.university_position.length ? p.university_position.map(membershipCard).join('') : '<p class="subtitle">Пока нет привязок к учебным заведениям.</p>'}
            </div>`
        : !busy
          ? '<button type="button" class="secondary" data-action="profile">Попробовать снова</button>'
          : ''
    }`;
}
