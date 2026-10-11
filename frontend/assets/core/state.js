import { AUTH_STORAGE_KEY } from '../features/auth/model.js';

export function createState(storage) {
  const state = {
    screen: 'login',
    email: '',
    token: '',
    refreshToken: '',
    sessionId: '',
    userId: '',
    verificationId: '',
    purpose: 'register',
    resetToken: '',
    resendAt: 0,
    busy: false,
    error: '',
    success: '',
    profile: null,
    accountRoles: null,
    accountRolesError: '',
    selectedUniversity: null,
    admins: null,
    selectedAdmin: null,
    people: null,
    peopleQuery: '',
    nextOffset: null,
    quizCatalog: null,
    quizCatalogName: '',
    quizCatalogFavourites: false,
    quizCatalogMore: false,
    quizCatalogOffset: 0,
  };

  try {
    const saved = JSON.parse(storage?.getItem(AUTH_STORAGE_KEY) || '{}');
    if (typeof saved.token === 'string') state.token = saved.token;
    if (typeof saved.refreshToken === 'string') state.refreshToken = saved.refreshToken;
    if (typeof saved.sessionId === 'string') state.sessionId = saved.sessionId;
    if (typeof saved.email === 'string') state.email = saved.email;
    if (typeof saved.verificationId === 'string' && saved.verificationId) {
      state.verificationId = saved.verificationId;
      state.purpose = saved.purpose === 'reset' ? 'reset' : 'register';
      state.resendAt = Number.isFinite(saved.resendAt) ? saved.resendAt : 0;
      state.screen = 'code';
    }
  } catch {
    /* A disabled storage or old session must not block sign-in. */
  }
  const persist = () => {
    try {
      storage?.setItem(
        AUTH_STORAGE_KEY,
        JSON.stringify({
          token: state.token,
          email: state.email,
          verificationId: state.verificationId,
          refreshToken: state.refreshToken,
          sessionId: state.sessionId,
          purpose: state.purpose,
          resendAt: state.resendAt,
        }),
      );
    } catch {}
  };

  return { state, persist };
}
