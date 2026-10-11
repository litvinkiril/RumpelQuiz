import { ApiError } from '../../core/api.js';
import { validatePassword, AUTH_STORAGE_KEY } from './model.js';
export function createAuthController({
  state,
  api,
  authorized,
  run,
  fail,
  emit,
  now,
  authenticate,
  setAccount,
  game,
  tests,
  clearEducation,
  resetCatalogs,
  storage,
}) {
  const setCode = (result, purpose) => {
    if (!result.verification_id) throw new ApiError('Сервер не вернул запрос подтверждения.');
    state.verificationId = result.verification_id;
    state.purpose = purpose;
    state.resendAt = now() + 60000;
    state.screen = 'code';
    state.resetToken = '';
  };

  return {
    remaining: () => Math.max(0, Math.ceil((state.resendAt - now()) / 1000)),
    login(email, password) {
      if (state.busy) return Promise.resolve(false);
      const error = validatePassword(password);
      if (error) return fail(error);
      state.email = email.trim();
      return run(
        async () => authenticate(await api('login', { email: state.email, password })),
        setAccount,
      ).then(async (ok) => {
        if (ok) await game.resumeGame();
        if (ok) await tests.resumeTestLink();
        return ok;
      });
    },
    register(email, password, confirmation) {
      if (state.busy) return Promise.resolve(false);
      const error = validatePassword(password, confirmation);
      if (error) return fail(error);
      state.email = email.trim();
      return run(
        () =>
          api('register', { email: state.email, password, password_confirmation: confirmation }),
        (result) => setCode(result, 'register'),
      );
    },
    requestReset(email) {
      if (state.busy) return Promise.resolve(false);
      state.email = email.trim();
      return run(
        () => api('forgot-password/email-check', { email: state.email }),
        (result) => setCode(result, 'reset'),
      );
    },
    verify(code) {
      if (!/^\d{6}$/.test(code)) return fail('Введите шестизначный код.');
      if (!state.verificationId) return fail('Запросите новый код.');
      const payload = { verification_id: state.verificationId, code };
      if (state.purpose === 'reset') {
        return run(
          () => api('forgot-password/verify-code', payload),
          (result) => {
            if (!result.reset_token)
              throw new ApiError('Сервер не вернул разрешение на смену пароля.');
            state.resetToken = result.reset_token;
            state.verificationId = '';
            state.screen = 'password';
          },
        );
      }
      return run(async () => authenticate(await api('verify-email', payload)), setAccount).then(
        async (ok) => {
          if (ok) await game.resumeGame();
          if (ok) await tests.resumeTestLink();
          return ok;
        },
      );
    },
    resend() {
      if (state.resendAt > now()) return Promise.resolve(false);
      return run(
        () =>
          state.purpose === 'reset'
            ? api('forgot-password/email-check', { email: state.email })
            : api('resend-code', { verification_id: state.verificationId }),
        (result) => {
          setCode(result, state.purpose);
          state.success = 'Новый код готов. Предыдущий больше не действует.';
        },
      );
    },
    updatePassword(password, confirmation) {
      const error = validatePassword(password, confirmation);
      if (error) return fail(error);
      if (!state.resetToken) {
        this.navigate('forgot');
        return fail('Запросите новый код для смены пароля.');
      }
      return run(
        () =>
          api('forgot-password/update-password', {
            reset_token: state.resetToken,
            password,
            password_confirmation: confirmation,
          }),
        () => {
          state.screen = 'login';
          state.resetToken = '';
          state.token = '';
          state.refreshToken = '';
          state.sessionId = '';
          state.userId = '';
          state.verificationId = '';
          state.success = 'Пароль изменён. Войдите с новым паролем.';
        },
      );
    },
    logout() {
      return run(
        async () => {
          try {
            await authorized('logout', {});
          } catch (error) {
            if (error.status !== 401) throw error;
          }
        },
        () => {
          Object.assign(state, {
            screen: 'login',
            token: '',
            refreshToken: '',
            sessionId: '',
            userId: '',
            verificationId: '',
            resetToken: '',
            profile: null,
            accountRoles: null,
            accountRolesError: '',
            resendAt: 0,
            busy: false,
            error: '',
            success: 'Вы вышли из аккаунта.',
          });
          clearEducation();
          game.resetGameView();
          tests.resetTestView();
          resetCatalogs();
          try {
            storage?.removeItem(AUTH_STORAGE_KEY);
          } catch {}
          emit();
        },
      );
    },
  };
}
