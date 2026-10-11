import { html, escape } from '../../shared/html.js';
import { finished, gameSeconds, joinLink, qrSvg } from './model.js';
import { renderResults } from './results.js';
const gameImage = (url) =>
  /^https:\/\//.test(url || '')
    ? html`<img
        class="game-image"
        src="${escape(url)}"
        alt="Изображение к вопросу или варианту"
        referrerpolicy="no-referrer"
      />`
    : '';
const renderParticipants = (participants, presence = []) => {
  const online = new Map(presence.map((p) => [p.user_id, p.online]));
  return (
    participants
      .map(
        (p) =>
          html`<span class="${online.get(p.user_id) === false ? 'game-person-offline' : ''}"
            >${escape(p.name)}${online.has(p.user_id) ? html`<small>${online.get(p.user_id) ? 'В квизе' : 'Нет связи'}</small>` : ''}</span
          >`,
      )
      .join('') || '<p class="hint">Участники появятся здесь после подключения.</p>'
  );
};
const renderGameAlerts = (alerts) =>
  (alerts || [])
    .map(
      (a) =>
        html`<li class="${a.online ? 'returned' : 'left'}">
          <strong>${escape(a.name)}</strong> —
          ${a.online ? 'снова в квизе' : 'вышел из квиза или потерял связь'}<time
            >${escape(new Date(a.time).toLocaleTimeString('ru-RU', { hour: '2-digit', minute: '2-digit', second: '2-digit' }))}</time
          >
        </li>`,
    )
    .join('');
export function updateGameProgress(root, game, previous, alerts = []) {
  const answered = root.querySelector('[data-game-answered]');
  if (answered)
    answered.textContent = `Ответили ${game.current_question.answered_count} из ${game.session.participants_count}`;
  const count = root.querySelector('[data-game-participants-count]');
  if (count) count.textContent = `Участники · ${game.session.participants_count}`;
  if (
    JSON.stringify(previous.participants) !== JSON.stringify(game.participants) ||
    JSON.stringify(previous.presence) !== JSON.stringify(game.presence)
  ) {
    const list = root.querySelector('.game-people-list');
    if (list) list.innerHTML = renderParticipants(game.participants, game.presence);
  }
  const notifications = root.querySelector('[data-game-alerts]');
  if (notifications) {
    const html = renderGameAlerts(alerts);
    if (notifications.innerHTML !== html) notifications.innerHTML = html;
  }
}
export function renderGame(state, notice) {
  if (state.screen === 'game-join')
    return html`<button class="text-button back" data-nav="account">← На главную</button>
      <p class="step-label">Присоединиться к игре</p>
      <h2 tabindex="-1">Введите код сессии</h2>
      <p class="subtitle">Код из шести цифр находится на экране преподавателя.</p>
      ${notice}
      <form data-form="game-join">
        <div class="field">
          <label for="game-code">Код сессии</label>
          <input
            id="game-code"
            name="game-code"
            class="game-code-input"
            inputmode="numeric"
            pattern="[0-9]{6}"
            maxlength="6"
            required
            value="${escape(state.gameJoinCode)}"
            placeholder="000000"
          />
        </div>
        <button class="primary" type="submit" ${state.busy ? 'disabled' : ''}>
          Присоединиться
        </button>
      </form>`;
  const game = state.game;
  if (!game)
    return html`<h2 tabindex="-1">Восстанавливаем сессию</h2>
      ${notice}<button class="secondary" data-action="game-refresh">Повторить загрузку</button
      ><button class="text-button" data-nav="account">На главную</button>`;
  const s = game.session,
    q = game.current_question,
    ended = finished(game),
    seconds = gameSeconds(state);
  let link = '',
    qr = '';
  try {
    link = joinLink(state.gameOrigin, s.join_code);
    qr = qrSvg(link);
  } catch {}
  const title = ended
    ? s.status === 'cancelled'
      ? 'Сессия закрыта'
      : 'Квиз завершён'
    : q
      ? 'Вопрос ' + (q.position + 1) + ' из ' + s.question_count
      : 'Все готовы?';
  return html`<button class="text-button back" data-nav="account">← На главную</button>
    <div class="game-heading">
      <div>
        <p class="step-label">${s.is_host ? 'Экран ведущего' : 'Участник'} · ${escape(s.name)}</p>
        <h2 tabindex="-1">${title}</h2>
      </div>
      <span class="quiz-badge ${ended ? '' : 'ready'}"
        >${ended ? 'Завершено' : q ? 'Игра идёт' : 'Ожидание'}</span
      >
    </div>
    ${notice}
    ${state.error ? '<button class="secondary" data-action="game-refresh">Обновить состояние</button>' : ''}
    ${
      s.is_host && !ended
        ? html`<section class="game-lobby">
            <div>
              <p class="game-label">Код для подключения</p>
              <div class="game-code" aria-label="Код сессии">${escape(s.join_code)}</div>
              <p class="hint">Откройте ссылку или отсканируйте QR-код.</p>
              <a class="game-link" href="${escape(link)}">${escape(link)}</a>
              <details class="game-network">
                <summary>Адрес для участников</summary>
                <form data-form="game-origin">
                  <label for="game-origin">Адрес сайта</label
                  ><input
                    id="game-origin"
                    name="game-origin"
                    type="url"
                    value="${escape(state.gameOrigin)}"
                    required
                  /><button type="submit" class="secondary">Обновить QR</button>
                </form>
                <p class="hint">
                  Для телефона в одной сети укажите адрес компьютера, например
                  http://192.168.1.50:8080.
                </p>
              </details>
              ${/^https?:\/\/(localhost|127\.0\.0\.1|\[::1\])(?=:|\/|$)/.test(link) ? '<p class="hint">Сейчас ссылка работает только на этом компьютере. Для других устройств измените адрес выше.</p>' : ''}
            </div>
            <div class="game-qr" role="img" aria-label="QR-код подключения к сессии">${qr}</div>
          </section>`
        : ''
    }
    ${
      !ended && q
        ? html`<section class="game-question">
            <div class="game-question-top">
              <span ${s.is_host ? 'data-game-answered' : ''}
                >${s.is_host ? `Ответили ${q.answered_count} из ${s.participants_count}` : 'Выберите ответ'}</span
              ><strong data-game-timer role="timer"
                >${seconds > 0 ? seconds + ' с' : 'Время истекло'}</strong
              >
            </div>
            <h3>${escape(q.text)}</h3>
            ${gameImage(q.image_url)}
            <div class="game-answers">
              ${q.answers
                .map(
                  (a, i) =>
                    html`<label
                      class="game-answer ${state.gameSelected.includes(a.id) ? 'selected' : ''}"
                      ><span class="game-answer-index">${i + 1}</span>
                      ${!s.is_host ? html`<input type="${q.type === 'single' ? 'radio' : 'checkbox'}" name="game-answer" data-game-answer="${escape(a.id)}" ${state.gameSelected.includes(a.id) ? 'checked' : ''} ${q.submitted || seconds === 0 || state.busy ? 'disabled' : ''} />` : ''}
                      <span>${escape(a.text)}${gameImage(a.image_url)}</span></label
                    >`,
                )
                .join('')}
            </div>
            ${
              !s.is_host
                ? html`<p class="hint" data-game-answer-status>
                      ${q.submitted ? 'Ответ сохранён. Ждём следующий вопрос.' : seconds === 0 ? 'Приём ответов завершён.' : q.type === 'multy' ? 'Можно выбрать несколько вариантов. После отправки ответ нельзя изменить.' : 'После отправки ответ нельзя изменить.'}
                    </p>
                    <button
                      class="primary"
                      data-action="game-submit"
                      ${q.submitted || seconds === 0 || !state.gameSelected.length || state.busy ? 'disabled' : ''}
                    >
                      ${q.submitted ? 'Ответ сохранён' : 'Отправить ответ'}
                    </button>`
                : ''
            }
          </section>`
        : ''
    }
    ${!q && !ended && !s.is_host ? '<div class="game-wait"><span aria-hidden="true">✦</span><h3>Вы в игре</h3><p>Преподаватель скоро откроет первый вопрос.</p></div>' : ''}
    ${
      s.is_host && !ended
        ? html`<section class="game-people">
              <h3 data-game-participants-count>Участники · ${s.participants_count}</h3>
              <div class="game-people-list">
                ${renderParticipants(game.participants, game.presence)}
              </div>
              <ul
                class="game-alerts"
                data-game-alerts
                aria-live="polite"
                aria-relevant="additions"
                aria-label="Уведомления об участниках"
              >
                ${renderGameAlerts(state.gameAlerts)}
              </ul>
            </section>
            <div class="game-controls">
              <button
                class="primary"
                data-action="game-next"
                ${state.busy || q?.has_next === false ? 'disabled' : ''}
              >
                Следующий вопрос</button
              ><button
                class="secondary game-close"
                data-action="game-close"
                ${state.busy ? 'disabled' : ''}
              >
                Закрыть сессию
              </button>
            </div>
            <p class="hint">
              ${q?.has_next === false ? 'Это последний вопрос. Закройте сессию, когда будете готовы.' : !q ? '«Следующий вопрос» запустит первый вопрос.' : 'Переход завершает приём ответов на текущий вопрос.'}
            </p>`
        : ''
    }
    ${
      ended
        ? html`<section class="game-finished">
            <div class="game-finished-icon" aria-hidden="true">
              ${s.status === 'cancelled' ? '◇' : '✦'}
            </div>
            <h3>
              ${s.status === 'cancelled' ? 'Игра не была начата' : 'Квиз пройден. Вот итоги!'}
            </h3>
            <p class="subtitle">
              ${s.status === 'cancelled' ? 'Сессия завершилась до первого вопроса.' : 'Спасибо за игру! Каждый правильный ответ — шаг к победе.'}
            </p>
            ${renderResults(game.results, s.question_count, { userId: state.userId, status: s.status })}
            <button class="secondary" data-nav="account">На главную</button>
          </section>`
        : ''
    }`;
}
