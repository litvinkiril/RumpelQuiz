import { html, escapeHtml } from '../../shared/html.js';
const passwordField = (name, label, autocomplete = 'new-password') => `
<div class="field"><label for="${name}">${label}</label><div class="input-wrap">
<input id="${name}" name="${name}" type="password" autocomplete="${autocomplete}" required data-password placeholder="${name === 'password_confirmation' ? 'Ещё раз тот же пароль' : 'Ваш пароль'}">
<button class="show-password" type="button" data-toggle="${name}" aria-label="Показать: ${label}" aria-pressed="false">Показать</button></div></div>`;
const emailField = (email) =>
  html`<div class="field">
    <label for="email">Электронная почта</label
    ><input
      id="email"
      name="email"
      type="email"
      autocomplete="email"
      maxlength="254"
      required
      placeholder="you@example.com"
      value="${escapeHtml(email)}"
    />
  </div>`;
const primary = (label, busy) =>
  html`<button type="submit" class="primary" ${busy ? 'disabled' : ''}>
    <strong>${busy ? 'Подождите…' : label}</strong><span aria-hidden="true">↗</span>
  </button>`;

export function renderAuth(state, notice) {
  const { screen, busy, email } = state;
  const back = (target = 'login') =>
    html`<button type="button" class="text-button back" data-nav="${target}">
      ← Вернуться ко входу
    </button>`;
  let content;
  if (screen === 'login' || screen === 'register') {
    const register = screen === 'register';
    content = html`<div class="tabs" role="tablist" aria-label="Вход или регистрация">
        <button role="tab" type="button" aria-selected="${!register}" data-nav="login">
          Войти
        </button>
        <button role="tab" type="button" aria-selected="${register}" data-nav="register">
          Регистрация
        </button>
      </div>
      <h2 tabindex="-1">${register ? 'Давайте знакомиться' : 'С возвращением!'}</h2>
      <p class="subtitle">
        ${register ? 'Пара деталей — и у вас будет свой аккаунт.' : 'Войдите, чтобы продолжить свою историю.'}
      </p>
      ${notice}
      <form data-form="${screen}">
        ${emailField(email)}
        ${!register ? '<div class="field-head"><span></span><button class="text-button" type="button" data-nav="forgot">Не помню пароль</button></div>' : ''}
        ${passwordField('password', 'Пароль', register ? 'new-password' : 'current-password')}
        ${register ? passwordField('password_confirmation', 'Повторите пароль') + '<p class="hint">До 72 байт. Для кириллицы один символ занимает несколько байт.</p><br>' : ''}
        ${primary(register ? 'Создать аккаунт' : 'Войти в аккаунт', busy)}
      </form>
      <p class="switch-prompt">
        ${register ? 'Уже знакомы?' : 'Пока нет аккаунта?'}
        <button type="button" class="text-button" data-nav="${register ? 'login' : 'register'}">
          ${register ? 'Войти' : 'Зарегистрироваться'}
        </button>
      </p>`;
  } else if (screen === 'forgot') {
    content = `${back()}<p class="step-label">Восстановление · 1 из 3</p><h2 tabindex="-1">Забыли пароль?</h2>
      <p class="subtitle">Укажите почту вашего аккаунта. Получите код и задайте новый пароль.</p>
      ${notice}<form data-form="forgot">${emailField(email)}${primary('Получить код', busy)}</form>
      <p class="switch-prompt">Доступно для подтверждённой почты.</p>`;
  } else if (screen === 'code') {
    content = `${back()}<div class="progress" aria-hidden="true"><span class="done"></span><span class="done"></span><span></span></div>
      <p class="step-label">${state.purpose === 'reset' ? 'Восстановление · 2 из 3' : 'Подтверждение почты'}</p>
      <h2 tabindex="-1">Остался только код</h2><p class="subtitle">Введите 6 цифр для<br><strong>${escapeHtml(email)}</strong></p>
      ${notice}<form data-form="code"><div class="field"><label for="code">Код подтверждения</label>
      <input class="code-input" id="code" name="code" inputmode="numeric" autocomplete="one-time-code" pattern="[0-9]{6}" maxlength="6" required placeholder="000000"></div>
      ${primary('Подтвердить код', busy)}</form>
      <div class="resend"><button type="button" class="text-button" data-action="resend" ${busy ? 'disabled' : ''}>Получить новый код</button></div>
      <p class="dev-note">Локальный режим: код отображается в консоли запущенного сервера. Отправка через Postbox пока отключена.</p>`;
  } else if (screen === 'password') {
    content = `${back()}<p class="step-label">Восстановление · 3 из 3</p><h2 tabindex="-1">Новый пароль</h2>
      <p class="subtitle">Почта подтверждена. Придумайте новый пароль для вашего аккаунта.</p>
      ${notice}<form data-form="password">${passwordField('password', 'Новый пароль')}${passwordField('password_confirmation', 'Повторите новый пароль')}
      ${primary('Сохранить пароль', busy)}</form>`;
  }
  return content;
}
