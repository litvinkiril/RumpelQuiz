import { ApiError } from '../../core/api.js';
import { newUserDraft, userPayload, userRoles, validateUser } from './model.js';

const creationErrors = {
  email_already_exists: 'Пользователь с такой почтой уже существует. Укажите другую почту.',
  invalid_faculty: 'Выберите факультет из списка. При необходимости обновите список.',
  university_access_denied:
    'У вас нет прав создавать пользователя с выбранной ролью или факультетом.',
  university_not_found: 'Учебное заведение не найдено. Вернитесь в профиль.',
  invalid_request: 'Проверьте имя, фамилию, почту и пароль.',
};

export function createUserCreationController({ state, authorized, run, emit, fail }) {
  const canCreate = () =>
    state.token &&
    state.selectedUniversity &&
    state.profile?.university_position?.some(
      (position) =>
        position.role === 'admin' &&
        position.university_id === state.selectedUniversity.id &&
        (!position.status || position.status === 'active'),
    );
  const loadUserFaculties = (force = false) => {
    if (
      state.busy ||
      !canCreate() ||
      state.screen !== 'user-create' ||
      state.newUserDraft?.role !== 'admin'
    )
      return Promise.resolve(false);
    const universityId = state.selectedUniversity.id;
    if (!force && Object.hasOwn(state.facultyCache, universityId)) {
      state.userFaculties = state.facultyCache[universityId];
      state.error = '';
      emit();
      return Promise.resolve(true);
    }
    state.userFaculties = null;
    const query = new URLSearchParams({ university_id: universityId });
    return run(
      () => authorized('education/faculties?' + query),
      (result) => {
        if (
          result?.success !== true ||
          !Array.isArray(result.faculties) ||
          result.faculties.some(
            (faculty) =>
              !faculty ||
              typeof faculty.id !== 'string' ||
              !faculty.id ||
              typeof faculty.name !== 'string',
          )
        )
          throw new ApiError('Не удалось загрузить факультеты. Попробуйте ещё раз.');
        state.facultyCache[universityId] = result.faculties;
        state.userFaculties = result.faculties;
        if (!result.faculties.some((faculty) => faculty.id === state.newUserDraft.facultet_id))
          state.newUserDraft.facultet_id = '';
      },
    );
  };
  return {
    loadUserFaculties,
    openUserCreation() {
      if (state.busy || !canCreate()) return Promise.resolve(false);
      const scope = state.selectedUniversity.adminScope;
      if (!userRoles(scope).length)
        return fail('Не удалось определить права администратора. Обновите профиль.');
      state.newUserDraft = newUserDraft(scope);
      state.createdUser = null;
      state.userFaculties = null;
      state.userCreationDirty = false;
      state.selectedAdmin = null;
      state.screen = 'user-create';
      state.error = '';
      state.success = '';
      if (state.newUserDraft.role === 'admin') return loadUserFaculties();
      emit();
      return Promise.resolve(true);
    },
    editNewUser(field, value) {
      if (state.busy || state.screen !== 'user-create' || !state.newUserDraft) return;
      if (field === 'password') {
        state.userCreationDirty = !!value || state.userCreationDirty;
        return;
      }
      if (!Object.hasOwn(state.newUserDraft, field) || field === 'role') return;
      state.newUserDraft[field] = value;
      state.userCreationDirty = true;
    },
    selectNewUserRole(role) {
      if (
        state.busy ||
        state.screen !== 'user-create' ||
        !state.newUserDraft ||
        !userRoles(state.selectedUniversity?.adminScope).includes(role)
      )
        return Promise.resolve(false);
      state.newUserDraft.role = role;
      state.newUserDraft.facultet_id = '';
      state.userCreationDirty = true;
      state.error = '';
      state.success = '';
      if (role === 'admin') return loadUserFaculties();
      emit();
      return Promise.resolve(true);
    },
    createUser(password) {
      if (state.busy || !canCreate() || state.screen !== 'user-create' || !state.newUserDraft)
        return Promise.resolve(false);
      const payload = userPayload(state.newUserDraft, state.selectedUniversity.id, password);
      const error = validateUser(payload, state.selectedUniversity.adminScope, state.userFaculties);
      if (error) return fail(error);
      return run(
        async () => {
          try {
            return await authorized('user/create', payload);
          } catch (error) {
            if (creationErrors[error.code])
              throw new ApiError(creationErrors[error.code], error.status, error.code);
            throw error;
          }
        },
        (result) => {
          if (result?.success !== true || typeof result.user_id !== 'string' || !result.user_id)
            throw new ApiError(
              'Не удалось подтвердить создание пользователя. Проверьте его через поиск людей.',
            );
          state.createdUser = {
            id: result.user_id,
            email: payload.email,
            first_name: payload.first_name,
            last_name: payload.last_name,
            middle_name: payload.middle_name,
            role: payload.role,
          };
          state.newUserDraft = null;
          state.userCreationDirty = false;
          state.admins = null;
          state.people = null;
          state.nextOffset = null;
          state.success = 'Пользователь создан. Он может войти с указанной почтой и паролем.';
        },
      );
    },
  };
}
