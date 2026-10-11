import { ApiError } from '../core/api.js';
export const profileRoles = (profile) => [
  ...new Set(
    profile.university_position
      .filter((p) => !p.status || p.status === 'active')
      .map((p) => p.role)
      .filter((role) => ['student', 'teacher', 'admin'].includes(role)),
  ),
];
export async function readAccountRoles(read) {
  try {
    const profile = await read();
    if (profile?.success !== true || !Array.isArray(profile.university_position))
      throw new ApiError('Не удалось загрузить роль. Попробуйте ещё раз.');
    return { accountRoles: profileRoles(profile), accountRolesError: '' };
  } catch (error) {
    if (error.status === 401) throw error;
    return {
      accountRoles: null,
      accountRolesError: 'Не удалось загрузить роль. Попробуйте ещё раз.',
    };
  }
}
