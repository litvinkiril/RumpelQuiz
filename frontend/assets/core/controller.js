import { createState } from './state.js';
import { createSession } from './session.js';
import { createApi, ApiError } from './api.js';
import { readAccountRoles } from '../shared/roles.js';
import { createAuthController } from '../features/auth/controller.js';
import { createProfileController } from '../features/profile/controller.js';
import { createEducationController } from '../features/education/controller.js';
import { createQuizController } from '../features/quizzes/controller.js';
import { createCatalogController } from '../features/catalog/controller.js';
import { createTestController } from '../features/tests/controller.js';
import { createGameController } from '../features/game/controller.js';
export function createController({
  api = createApi(),
  streamFetch = globalThis.fetch,
  storage,
  onChange = () => {},
  now = Date.now,
  makeGameId,
  origin = globalThis.location?.origin || 'http://localhost:8080',
  joinCode = '',
  testLink = '',
} = {}) {
  let sequence = 0;
  const { state, persist } = createState(storage);
  const resetQuizCatalog = () =>
    Object.assign(state, {
      quizCatalog: null,
      quizCatalogName: '',
      quizCatalogFavourites: false,
      quizCatalogMore: false,
      quizCatalogOffset: 0,
    });
  const clearPeople = () => {
    state.people = null;
    state.peopleQuery = '';
    state.nextOffset = null;
  };
  const clearEducation = () => {
    clearPeople();
    state.selectedUniversity = null;
    state.admins = null;
    state.selectedAdmin = null;
  };
  const emit = (change) => {
    persist();
    onChange({ ...state }, change);
  };
  const run = async (work, apply, { background = false } = {}) => {
    if (state.busy) return false;
    const current = ++sequence;
    let changed = true;
    let change;
    if (!background) {
      state.busy = true;
      state.error = '';
      state.errorCode = '';
      state.success = '';
      emit();
    }
    try {
      const result = await work();
      if (current !== sequence) return false;
      change = apply(result);
      changed = change !== false;
      return true;
    } catch (error) {
      if (current === sequence) {
        state.error =
          error instanceof ApiError
            ? error.message
            : 'Не удалось выполнить запрос. Попробуйте снова.';
        state.errorCode = error.code || '';
        if (state.screen === 'test') state.testErrors = error.details || [];
        if (state.screen === 'quiz') state.quizErrors = error.details || [];
        if (error.status === 403 && state.screen === 'people') {
          state.people = null;
          state.nextOffset = null;
          state.selectedAdmin = null;
        }
        if (error.status === 401 && state.token) {
          state.token = '';
          state.refreshToken = '';
          state.sessionId = '';
          state.userId = '';
          state.screen = 'login';
          state.profile = null;
          state.accountRoles = null;
          state.accountRolesError = '';
          clearEducation();
          tests.resetTestView();
          resetQuizCatalog();
        }
        if (error.code === 'resend_too_soon') state.resendAt = now() + 60000;
        if (error.code === 'invalid_or_expired_token') {
          state.resetToken = '';
          state.screen = 'forgot';
        }
      }
      return false;
    } finally {
      if (current === sequence) {
        if (!background) state.busy = false;
        if (changed) emit(change === 'game-progress' ? change : undefined);
      }
    }
  };
  const fail = (message) => {
    state.error = message;
    state.success = '';
    emit();
    return Promise.resolve(false);
  };
  const { authenticate, authorized, currentAccount } = createSession({
    state,
    api,
    persist,
    getSequence: () => sequence,
  });
  const setAccount = ({
    token,
    refreshToken,
    sessionId,
    userId,
    accountRoles,
    accountRolesError,
  }) => {
    game.resetGameView();
    tests.resetTestView();
    resetQuizCatalog();
    state.token = token;
    state.refreshToken = refreshToken;
    state.sessionId = sessionId;
    state.userId = userId;
    state.screen = 'account';
    state.profile = null;
    state.accountRoles = accountRoles;
    state.accountRolesError = accountRolesError;
    clearEducation();
    state.verificationId = '';
    state.resetToken = '';
    state.resendAt = 0;
  };
  const game = createGameController({
    state,
    authorized,
    run,
    emit,
    fail,
    storage,
    streamFetch,
    now,
    makeId: makeGameId,
    origin,
    joinCode,
  });
  const tests = createTestController({
    state,
    authorized,
    run,
    emit,
    fail,
    now,
    origin,
    ApiError,
    testLink,
  });

  const context = {
    state,
    api,
    authorized,
    run,
    emit,
    fail,
    now,
    storage,
    clearEducation,
    clearPeople,
    resetQuizCatalog,
    cancelPending: () => ++sequence,
  };
  const auth = createAuthController({ ...context, authenticate, setAccount, game, tests });
  const profile = createProfileController(context);
  const education = createEducationController(context);
  const quizzes = createQuizController(context);
  const catalog = createCatalogController(context);
  return {
    ...auth,
    ...profile,
    ...education,
    ...quizzes,
    ...catalog,
    ...tests,
    ...game,
    state,
    reloadAccountRoles() {
      if (
        state.busy ||
        !state.token ||
        !['account', 'quiz-section', 'test-section'].includes(state.screen)
      )
        return Promise.resolve(false);
      return run(
        () => readAccountRoles(() => authorized('profile')),
        (result) => {
          Object.assign(state, result);
        },
      );
    },
    navigate(screen) {
      if (['quiz-section', 'test-section'].includes(screen) && state.token) {
        ++sequence;
        state.busy = false;
        state.screen = screen;
        state.error = '';
        state.success = '';
        emit();
        return;
      }
      if (screen === 'profile' && state.token && state.profile) {
        ++sequence;
        state.busy = false;
        state.screen = 'profile';
        state.error = '';
        state.success = '';
        clearEducation();
        emit();
        return;
      }
      if (screen === 'university' && state.token && state.selectedUniversity) {
        clearPeople();
        ++sequence;
        state.busy = false;
        state.screen = 'university';
        state.error = '';
        state.success = '';
        state.admins = null;
        state.selectedAdmin = null;
        emit();
        return;
      }
      if (screen === 'account' && state.token) {
        ++sequence;
        state.busy = false;
        state.screen = 'account';
        state.error = '';
        state.success = '';
        state.profile = null;
        clearEducation();
        emit();
        return;
      }
      if (!['login', 'register', 'forgot'].includes(screen)) return;
      ++sequence;
      state.busy = false;
      state.screen = screen;
      state.error = '';
      state.success = '';
      state.token = '';
      state.refreshToken = '';
      state.sessionId = '';
      state.userId = '';
      state.profile = null;
      state.accountRoles = null;
      state.accountRolesError = '';
      resetQuizCatalog();
      tests.resetTestView();
      clearEducation();
      state.verificationId = '';
      state.resetToken = '';
      state.resendAt = 0;
      emit();
    },
    async start() {
      if (state.token) {
        const ok = await run(currentAccount, setAccount);
        if (ok) await game.resumeGame();
        if (ok) await tests.resumeTestLink();
        return ok;
      }
      emit();
      return true;
    },
    refresh() {
      return run(currentAccount, (result) => {
        setAccount(result);
        state.success = 'Сессия активна. Доступ к аккаунту подтверждён.';
      });
    },
  };
}
