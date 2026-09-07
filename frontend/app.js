export const messages = {
  invalid_credentials: 'Проверьте почту и пароль. Если вы ещё не подтвердили почту, завершите регистрацию.',
  email_already_exists: 'Эта почта уже зарегистрирована. Войдите или восстановите пароль.',
  passwords_do_not_match: 'Пароли не совпадают.',
  invalid_password: 'Пароль должен содержать от 1 до 72 байт и не содержать нулевых символов.',
  invalid_request: 'Проверьте заполненные поля.',
  invalid_or_expired_code: 'Код неверный, истёк или уже использован. Проверьте его или запросите новый.',
  invalid_or_expired_token: 'Время на смену пароля истекло. Запросите новый код.',
  email_not_found: 'Аккаунт с такой почтой не найден.',
  email_not_verified: 'Сначала подтвердите почту: вернитесь к регистрации.',
  verification_not_found: 'Этот запрос уже недействителен. Начните регистрацию заново.',
  resend_too_soon: 'Повторный код можно запросить через минуту.',
};
export class ApiError extends Error {
  constructor(message, status = 0, code = '') { super(message); this.status = status; this.code = code; }
}
export function createApi(fetcher = globalThis.fetch) {
  return async (path, data, token) => {
    const abort = new AbortController();
    const timeout = setTimeout(() => abort.abort(), 15000);
    try {
      const response = await fetcher('/v1/auth/' + path, {
        method: data === undefined ? 'GET' : 'POST',
        headers: { ...(data === undefined ? {} : {'Content-Type': 'application/json'}),
          ...(token ? {Authorization: 'Bearer ' + token} : {}) },
        ...(data === undefined ? {} : {body: JSON.stringify(data)}),
        signal: abort.signal,
      });
      const body = await response.json().catch(() => ({}));
      if (!response.ok) {
        throw new ApiError(messages[body.error] || (response.status >= 500
          ? 'Сервис временно недоступен. Попробуйте ещё раз.'
          : 'Не удалось выполнить запрос. Проверьте данные.'), response.status, body.error);
      }
      return body;
    } catch (error) {
      if (error instanceof ApiError) throw error;
      throw new ApiError(error.name === 'AbortError'
        ? 'Сервер не ответил вовремя. Попробуйте ещё раз.'
        : 'Не удалось связаться с сервером. Проверьте подключение.');
    } finally { clearTimeout(timeout); }
  };
}
export function validatePassword(password, confirmation) {
  if (!password || new TextEncoder().encode(password).length > 72 || password.includes('\0'))
    return messages.invalid_password;
  if (confirmation !== undefined && password !== confirmation) return messages.passwords_do_not_match;
  return '';
}
const KEY = 'rumpelquiz.auth';
export function createController({api = createApi(), storage, onChange = () => {}, now = Date.now} = {}) {
  let sequence = 0;
  const state = {screen: 'login', email: '', token: '', userId: '', verificationId: '',
    purpose: 'register', resetToken: '', resendAt: 0, busy: false, error: '', success: ''};
  try {
    const saved = JSON.parse(storage?.getItem(KEY) || '{}');
    if (typeof saved.token === 'string') state.token = saved.token;
    if (typeof saved.email === 'string') state.email = saved.email;
    if (typeof saved.verificationId === 'string' && saved.verificationId) {
      state.verificationId = saved.verificationId;
      state.purpose = saved.purpose === 'reset' ? 'reset' : 'register';
      state.resendAt = Number.isFinite(saved.resendAt) ? saved.resendAt : 0;
      state.screen = 'code';
    }
  } catch { /* A disabled storage or old session must not block sign-in. */ }
  const persist = () => {
    try { storage?.setItem(KEY, JSON.stringify({
      token: state.token, email: state.email, verificationId: state.verificationId,
      purpose: state.purpose, resendAt: state.resendAt,
    })); } catch {}
  };
  const emit = () => { persist(); onChange({...state}); };
  const run = async (work, apply) => {
    if (state.busy) return false;
    const current = ++sequence;
    state.busy = true; state.error = ''; state.success = ''; emit();
    try {
      const result = await work();
      if (current !== sequence) return false;
      apply(result);
      return true;
    } catch (error) {
      if (current === sequence) {
        state.error = error instanceof ApiError ? error.message : 'Не удалось выполнить запрос. Попробуйте снова.';
        if (error.status === 401 && state.token) { state.token = ''; state.userId = ''; state.screen = 'login'; }
        if (error.code === 'resend_too_soon') state.resendAt = now() + 60000;
        if (error.code === 'invalid_or_expired_token') { state.resetToken = ''; state.screen = 'forgot'; }
      }
      return false;
    } finally {
      if (current === sequence) { state.busy = false; emit(); }
    }
  };
  const fail = (message) => { state.error = message; state.success = ''; emit(); return Promise.resolve(false); };
  const setCode = (result, purpose) => {
    if (!result.verification_id) throw new ApiError('Сервер не вернул запрос подтверждения.');
    state.verificationId = result.verification_id; state.purpose = purpose;
    state.resendAt = now() + 60000; state.screen = 'code'; state.resetToken = '';
  };
  const authenticate = async (result) => {
    if (!result.access_token) throw new ApiError('Сервер не вернул токен входа.');
    const user = await api('me', undefined, result.access_token);
    if (!user.user_id) throw new ApiError('Не удалось загрузить аккаунт.');
    return {token: result.access_token, userId: user.user_id};
  };
  const setAccount = ({token, userId}) => {
    state.token = token; state.userId = userId; state.screen = 'account';
    state.verificationId = ''; state.resetToken = ''; state.resendAt = 0;
  };
  return {
    state,
    remaining: () => Math.max(0, Math.ceil((state.resendAt - now()) / 1000)),
    navigate(screen) {
      if (!['login', 'register', 'forgot'].includes(screen)) return;
      ++sequence; state.busy = false; state.screen = screen; state.error = ''; state.success = '';
      state.token = ''; state.userId = '';
      state.verificationId = ''; state.resetToken = ''; state.resendAt = 0; emit();
    },
    async start() {
      if (state.token) return run(() => authenticate({access_token: state.token}), setAccount);
      emit(); return true;
    },
    login(email, password) {
      if (state.busy) return Promise.resolve(false);
      const error = validatePassword(password);
      if (error) return fail(error);
      state.email = email.trim();
      return run(async () => authenticate(await api('login', {email: state.email, password})), setAccount);
    },
    register(email, password, confirmation) {
      if (state.busy) return Promise.resolve(false);
      const error = validatePassword(password, confirmation);
      if (error) return fail(error);
      state.email = email.trim();
      return run(() => api('register', {email: state.email, password, password_confirmation: confirmation}),
        result => setCode(result, 'register'));
    },
    requestReset(email) {
      if (state.busy) return Promise.resolve(false);
      state.email = email.trim();
      return run(() => api('forgot-password/email-check', {email: state.email}), result => setCode(result, 'reset'));
    },
    verify(code) {
      if (!/^\d{6}$/.test(code)) return fail('Введите шестизначный код.');
      if (!state.verificationId) return fail('Запросите новый код.');
      const payload = {verification_id: state.verificationId, code};
      if (state.purpose === 'reset') {
        return run(() => api('forgot-password/verify-code', payload), result => {
          if (!result.reset_token) throw new ApiError('Сервер не вернул разрешение на смену пароля.');
          state.resetToken = result.reset_token; state.verificationId = ''; state.screen = 'password';
        });
      }
      return run(async () => authenticate(await api('verify-email', payload)), setAccount);
    },
    resend() {
      if (state.resendAt > now()) return Promise.resolve(false);
      return run(() => state.purpose === 'reset'
        ? api('forgot-password/email-check', {email: state.email})
        : api('resend-code', {verification_id: state.verificationId}), result => {
          setCode(result, state.purpose); state.success = 'Новый код готов. Предыдущий больше не действует.';
        });
    },
    updatePassword(password, confirmation) {
      const error = validatePassword(password, confirmation);
      if (error) return fail(error);
      if (!state.resetToken) { this.navigate('forgot'); return fail('Запросите новый код для смены пароля.'); }
      return run(() => api('forgot-password/update-password', {
        reset_token: state.resetToken, password, password_confirmation: confirmation,
      }), () => {
        state.screen = 'login'; state.resetToken = ''; state.token = ''; state.userId = '';
        state.verificationId = ''; state.success = 'Пароль изменён. Войдите с новым паролем.';
      });
    },
    refresh() { return run(() => authenticate({access_token: state.token}), result => {
      setAccount(result); state.success = 'Сессия активна. Доступ к аккаунту подтверждён.';
    }); },
    logout() {
      ++sequence;
      Object.assign(state, {screen: 'login', token: '', userId: '', verificationId: '', resetToken: '',
        resendAt: 0, busy: false, error: '', success: 'Вы вышли из аккаунта.'});
      try { storage?.removeItem(KEY); } catch {}
      emit();
    },
  };
}
export const escapeHtml = (value) => String(value).replace(/[&<>"']/g, c => ({
  '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;',
}[c]));
const passwordField = (name, label, autocomplete = 'new-password') => `
<div class="field"><label for="${name}">${label}</label><div class="input-wrap">
<input id="${name}" name="${name}" type="password" autocomplete="${autocomplete}" required data-password placeholder="${name === 'password_confirmation' ? 'Ещё раз тот же пароль' : 'Ваш пароль'}">
<button class="show-password" type="button" data-toggle="${name}" aria-label="Показать: ${label}" aria-pressed="false">Показать</button></div></div>`;
const emailField = (email) => `<div class="field"><label for="email">Электронная почта</label><input id="email" name="email" type="email" autocomplete="email" maxlength="254" required placeholder="you@example.com" value="${escapeHtml(email)}"></div>`;
const primary = (label, busy) => `<button type="submit" class="primary" ${busy ? 'disabled' : ''}><strong>${busy ? 'Подождите…' : label}</strong><span aria-hidden="true">↗</span></button>`;
export function renderView(state) {
  const {screen, busy, email} = state;
  const notice = state.error ? `<div class="notice error" role="alert">${escapeHtml(state.error)}</div>`
    : state.success ? `<div class="notice success" role="status">${escapeHtml(state.success)}</div>` : '';
  const back = (target = 'login') => `<button type="button" class="text-button back" data-nav="${target}">← Вернуться ко входу</button>`;
  let content;
  if (screen === 'login' || screen === 'register') {
    const register = screen === 'register';
    content = `<div class="tabs" role="tablist" aria-label="Вход или регистрация">
      <button role="tab" type="button" aria-selected="${!register}" data-nav="login">Войти</button>
      <button role="tab" type="button" aria-selected="${register}" data-nav="register">Регистрация</button></div>
      <h2 tabindex="-1">${register ? 'Давайте знакомиться' : 'С возвращением!'}</h2>
      <p class="subtitle">${register ? 'Пара деталей — и у вас будет свой аккаунт.' : 'Войдите, чтобы продолжить свою историю.'}</p>
      ${notice}<form data-form="${screen}">${emailField(email)}
      ${!register ? '<div class="field-head"><span></span><button class="text-button" type="button" data-nav="forgot">Не помню пароль</button></div>' : ''}
      ${passwordField('password', 'Пароль', register ? 'new-password' : 'current-password')}
      ${register ? passwordField('password_confirmation', 'Повторите пароль') + '<p class="hint">До 72 байт. Для кириллицы один символ занимает несколько байт.</p><br>' : ''}
      ${primary(register ? 'Создать аккаунт' : 'Войти в аккаунт', busy)}</form>
      <p class="switch-prompt">${register ? 'Уже знакомы?' : 'Пока нет аккаунта?'} <button type="button" class="text-button" data-nav="${register ? 'login' : 'register'}">${register ? 'Войти' : 'Зарегистрироваться'}</button></p>`;
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
  } else {
    content = `<div class="success-icon" aria-hidden="true">✓</div><p class="step-label">Ваш аккаунт</p>
      <h2 tabindex="-1">Вы на месте.</h2><p class="subtitle">Вход выполнен. Здесь можно проверить доступ к аккаунту и изменить пароль.</p>
      ${notice}<dl class="account-info"><dt>Электронная почта</dt><dd>${escapeHtml(email || 'Подтверждена')}</dd>
      <dt>Статус</dt><dd class="verified">✓ Почта подтверждена</dd><dt>Идентификатор аккаунта</dt><dd>${escapeHtml(state.userId)}</dd></dl>
      <button type="button" class="primary" data-action="refresh" ${busy ? 'disabled' : ''}><strong>${busy ? 'Проверяем…' : 'Проверить сессию'}</strong><span aria-hidden="true">↗</span></button>
      <div class="account-actions"><button class="text-button" data-nav="forgot" type="button">Изменить пароль</button><button class="text-button" data-action="logout" type="button">Выйти из аккаунта</button></div>`;
  }
  return `<div class="auth-card" aria-busy="${busy}">${content}</div>`;
}
export function mountApp(root, options = {}) {
  let lastScreen = '';
  let controller;
  const render = (state) => {
    // Keep values across the busy/error render, never in storage.
    const values = new Map([...root.querySelectorAll('input')].map(el => [el.name, el.value]));
    const focusName = root.ownerDocument.activeElement?.getAttribute('name');
    root.innerHTML = renderView(state);
    if (lastScreen === state.screen) {
      for (const el of root.querySelectorAll('input')) if (values.has(el.name)) el.value = values.get(el.name);
      const focus = [...root.querySelectorAll('input')].find(el => el.name === focusName);
      focus?.focus();
    } else {
      root.querySelector('h2')?.focus();
    }
    lastScreen = state.screen;
    for (const el of root.querySelectorAll('input,button')) if (state.busy) el.disabled = true;
    tick();
  };
  controller = createController({...options, onChange: render});
  const tick = () => {
    const button = root.querySelector('[data-action="resend"]');
    if (!button) return;
    const seconds = controller.remaining();
    button.disabled = controller.state.busy || seconds > 0;
    button.textContent = seconds > 0 ? `Новый код через ${seconds} с` : 'Получить новый код';
  };
  const interval = setInterval(tick, 1000);
  root.addEventListener('submit', event => {
    event.preventDefault();
    if (controller.state.busy) return;
    const form = event.target;
    if (!form.reportValidity()) return;
    const data = Object.fromEntries(new FormData(form));
    const kind = form.dataset.form;
    if (kind === 'login') void controller.login(data.email, data.password);
    if (kind === 'register') void controller.register(data.email, data.password, data.password_confirmation);
    if (kind === 'forgot') void controller.requestReset(data.email);
    if (kind === 'code') void controller.verify(data.code);
    if (kind === 'password') void controller.updatePassword(data.password, data.password_confirmation);
  });
  root.addEventListener('click', event => {
    const button = event.target.closest('button');
    if (!button || controller.state.busy) return;
    if (button.dataset.nav) controller.navigate(button.dataset.nav);
    if (button.dataset.action === 'logout') controller.logout();
    if (button.dataset.action === 'refresh') void controller.refresh();
    if (button.dataset.action === 'resend') void controller.resend();
    if (button.dataset.toggle) {
      const input = root.querySelector('#' + button.dataset.toggle);
      const show = input.type === 'password';
      input.type = show ? 'text' : 'password';
      button.textContent = show ? 'Скрыть' : 'Показать';
      button.setAttribute('aria-pressed', String(show));
    }
  });
  void controller.start();
  return {controller, destroy: () => clearInterval(interval)};
}
if (typeof document !== 'undefined') {
  const root = document.getElementById('app');
  if (root) {
    let storage;
    try { storage = window.sessionStorage; } catch {}
    const app = mountApp(root, {storage});
    document.querySelector('.brand')?.addEventListener('click', event => {
      event.preventDefault();
      if (!app.controller.state.busy) app.controller.navigate('login');
    });
  }
}
