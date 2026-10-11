import { messages } from '../../core/messages.js';
export const AUTH_STORAGE_KEY = 'rumpelquiz.auth';
export function validatePassword(password, confirmation) {
  if (!password || new TextEncoder().encode(password).length > 72 || password.includes('\0'))
    return messages.invalid_password;
  if (confirmation !== undefined && password !== confirmation)
    return messages.passwords_do_not_match;
  return '';
}
