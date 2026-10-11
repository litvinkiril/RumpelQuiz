import { ApiError } from './api.js';
import { readAccountRoles } from '../shared/roles.js';

export function createSession({ state, api, persist, getSequence }) {
  const authenticate = async (result) => {
    if (!result.access_token) throw new ApiError('Сервер не вернул токен входа.');
    const user = await api('me', undefined, result.access_token);
    if (!user.user_id) throw new ApiError('Не удалось загрузить аккаунт.');
    const roles = await readAccountRoles(() => api('profile', undefined, result.access_token));
    return {
      token: result.access_token,
      refreshToken: result.refresh_token || '',
      sessionId: result.session_id || '',
      userId: user.user_id,
      ...roles,
    };
  };
  let refreshPending = null;
  const authorized = async (path, data, method) => {
    const current = getSequence();
    const accessToken = state.token;
    try {
      return await api(path, data, accessToken, ...(method ? [method] : []));
    } catch (error) {
      if (
        error.status !== 401 ||
        !state.refreshToken ||
        !state.sessionId ||
        current !== getSequence()
      )
        throw error;
      if (accessToken !== state.token)
        return api(path, data, state.token, ...(method ? [method] : []));
      const refreshToken = state.refreshToken,
        sessionId = state.sessionId;
      if (
        !refreshPending ||
        refreshPending.token !== refreshToken ||
        refreshPending.session !== sessionId
      ) {
        const pending = { token: refreshToken, session: sessionId };
        pending.promise = (async () => {
          const tokens = await api('refresh', {
            session_id: sessionId,
            refresh_token: refreshToken,
          });
          if (state.refreshToken !== refreshToken || state.sessionId !== sessionId)
            throw new ApiError('Запрос отменён.');
          if (!tokens.access_token || !tokens.refresh_token || !tokens.session_id)
            throw new ApiError('Сервер не вернул токены сессии.', 401);
          state.token = tokens.access_token;
          state.refreshToken = tokens.refresh_token;
          state.sessionId = tokens.session_id;
          // Persist a consumed refresh token's replacement even if navigation cancelled its request.
          persist();
        })().finally(() => {
          if (refreshPending === pending) refreshPending = null;
        });
        refreshPending = pending;
      }
      await refreshPending.promise;
      if (current !== getSequence()) throw new ApiError('Запрос отменён.');
      return api(path, data, state.token, ...(method ? [method] : []));
    }
  };
  const currentAccount = async () => {
    const user = await authorized('me');
    if (!user.user_id) throw new ApiError('Не удалось загрузить аккаунт.');
    const roles = await readAccountRoles(() => authorized('profile'));
    return {
      token: state.token,
      refreshToken: state.refreshToken,
      sessionId: state.sessionId,
      userId: user.user_id,
      ...roles,
    };
  };

  return { authenticate, authorized, currentAccount };
}
