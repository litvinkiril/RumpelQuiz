import { ApiError } from '../../core/api.js';
import { profileRoles } from '../../shared/roles.js';
export function createProfileController({ state, authorized, run, clearEducation }) {
  return {
    openProfile() {
      if (state.busy || !state.token) return Promise.resolve(false);
      state.screen = 'profile';
      state.profile = null;
      clearEducation();
      return run(
        () => authorized('profile'),
        (result) => {
          if (!result || result.success !== true || !Array.isArray(result.university_position))
            throw new ApiError('Не удалось загрузить профиль. Попробуйте ещё раз.');
          state.profile = result;
          state.accountRoles = profileRoles(result);
          state.accountRolesError = '';
        },
      );
    },
  };
}
