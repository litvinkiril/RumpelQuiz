import { validatePassword } from '../auth/model.js';

export const userRoles = (scope) =>
  scope === 'university' ? ['student', 'teacher', 'admin'] : scope === 'faculties' ? ['admin'] : [];

export function newUserDraft(scope) {
  return {
    first_name: '',
    last_name: '',
    middle_name: '',
    email: '',
    role: userRoles(scope)[0] || '',
    facultet_id: '',
    description: '',
  };
}

export function userPayload(draft, universityId, password) {
  const optional = (value) => value.trim() || null;
  return {
    university: universityId,
    first_name: draft.first_name.trim(),
    last_name: draft.last_name.trim(),
    middle_name: optional(draft.middle_name),
    email: draft.email.trim(),
    password,
    role: draft.role,
    description: optional(draft.description),
    ...(draft.role === 'admin' ? { facultet_id: draft.facultet_id } : {}),
  };
}

export function validateUser(payload, scope, faculties) {
  if (!payload.first_name || !payload.last_name) return 'Укажите имя и фамилию.';
  if (payload.email.length > 254 || !/^[^\s@]+@[^\s@]+$/.test(payload.email))
    return 'Укажите корректную электронную почту.';
  const passwordError = validatePassword(payload.password);
  if (passwordError) return passwordError;
  if (!userRoles(scope).includes(payload.role)) return 'Выбранная роль недоступна для создания.';
  if (Object.values(payload).some((value) => typeof value === 'string' && value.includes('\0')))
    return 'Поля не должны содержать нулевые символы.';
  if (payload.role === 'admin' && !faculties?.some((item) => item.id === payload.facultet_id))
    return 'Выберите факультет для администратора.';
  return '';
}
