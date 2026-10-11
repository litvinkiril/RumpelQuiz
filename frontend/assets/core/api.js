import { messages } from './messages.js';
export class ApiError extends Error {
  constructor(message, status = 0, code = '', details = []) {
    super(message);
    this.status = status;
    this.code = code;
    this.details = details;
  }
}
export function createApi(fetcher = globalThis.fetch) {
  return async (path, data, token, method) => {
    const abort = new AbortController();
    const timeout = setTimeout(() => abort.abort(), 15000);
    try {
      const url =
        path === 'profile'
          ? '/v1/user/profile'
          : /^(user\/|education\/|quizzes(?:\/|$)|tests(?:\/|$)|media\/|game\/)/.test(path)
            ? '/v1/' + path
            : '/v1/auth/' + path;
      const multipart = typeof FormData !== 'undefined' && data instanceof FormData;
      const response = await fetcher(url, {
        method: method || (data === undefined ? 'GET' : 'POST'),
        headers: {
          ...(data === undefined || multipart ? {} : { 'Content-Type': 'application/json' }),
          ...(token ? { Authorization: 'Bearer ' + token } : {}),
        },
        ...(data === undefined ? {} : { body: multipart ? data : JSON.stringify(data) }),
        signal: abort.signal,
      });
      const body = await response.json().catch(() => ({}));
      if (!response.ok) {
        throw new ApiError(
          messages[body.error] ||
            (response.status >= 500
              ? 'Сервис временно недоступен. Попробуйте ещё раз.'
              : 'Не удалось выполнить запрос. Проверьте данные.'),
          response.status,
          body.error,
          body.details || [],
        );
      }
      return body;
    } catch (error) {
      if (error instanceof ApiError) throw error;
      throw new ApiError(
        error.name === 'AbortError'
          ? 'Сервер не ответил вовремя. Попробуйте ещё раз.'
          : 'Не удалось связаться с сервером. Проверьте подключение.',
      );
    } finally {
      clearTimeout(timeout);
    }
  };
}
