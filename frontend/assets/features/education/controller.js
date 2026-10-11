import { ApiError } from '../../core/api.js';
import { messages } from '../../core/messages.js';
export function createEducationController({
  state,
  authorized,
  run,
  emit,
  fail,
  clearEducation,
  clearPeople,
  cancelPending,
}) {
  return {
    openUniversity(universityId) {
      if (state.busy || !state.token) return false;
      const position = state.profile?.university_position.find(
        (item) => item.role === 'admin' && item.university_id === universityId,
      );
      if (!position || !universityId) return false;
      cancelPending();
      clearEducation();
      state.selectedUniversity = {
        id: position.university_id,
        name: position.university_name,
        adminScope: position.admin_scope ?? null,
      };
      state.screen = 'university';
      state.error = '';
      state.success = '';
      emit();
      return true;
    },
    openPeople() {
      if (state.busy || !state.token || !state.selectedUniversity) return false;
      cancelPending();
      clearPeople();
      state.selectedAdmin = null;
      state.screen = 'people';
      state.error = '';
      state.success = '';
      emit();
      return true;
    },
    searchPeople(query, more = false) {
      if (state.busy || state.screen !== 'people' || !state.selectedUniversity)
        return Promise.resolve(false);
      const normalized = (more ? state.peopleQuery : query).trim();
      if ([...normalized].length < 2 || [...normalized].length > 100 || normalized.includes('\0'))
        return fail(messages.invalid_search_params);
      if (more && state.nextOffset === null) return Promise.resolve(false);
      const offset = more ? state.nextOffset : 0;
      if (!more) {
        state.people = null;
        state.nextOffset = null;
      }
      state.peopleQuery = normalized;
      state.selectedAdmin = null;
      const params = new URLSearchParams({ q: normalized, limit: '20', offset: String(offset) });
      return run(
        () =>
          authorized(
            'education/universities/' +
              encodeURIComponent(state.selectedUniversity.id) +
              '/people?' +
              params,
          ),
        (result) => {
          if (
            result?.success !== true ||
            !Array.isArray(result.people) ||
            typeof result.has_more !== 'boolean' ||
            (result.has_more &&
              (!Number.isInteger(result.next_offset) ||
                result.next_offset <= offset ||
                result.next_offset > 10000))
          )
            throw new ApiError('Не удалось загрузить результаты. Попробуйте ещё раз.');
          state.people = [
            ...new Map(
              [...(more ? state.people || [] : []), ...result.people].map((person) => [
                person.user_id,
                person,
              ]),
            ).values(),
          ];
          state.nextOffset = result.has_more ? result.next_offset : null;
        },
      );
    },
    openPerson(userId) {
      if (state.busy || state.screen !== 'people') return;
      state.selectedAdmin = state.people?.find((person) => person.user_id === userId) || null;
      emit();
    },
    openAdmins() {
      if (state.busy || !state.token || !state.selectedUniversity) return Promise.resolve(false);
      const universityId = state.selectedUniversity.id;
      state.screen = 'admins';
      state.admins = null;
      state.selectedAdmin = null;
      return run(
        () => authorized('education/universities/' + encodeURIComponent(universityId) + '/admins'),
        (result) => {
          if (result?.success !== true || !Array.isArray(result.admins))
            throw new ApiError('Не удалось загрузить администраторов. Попробуйте ещё раз.');
          state.admins = result.admins;
        },
      );
    },
    openAdmin(membershipId) {
      if (state.busy || state.screen !== 'admins') return;
      state.selectedAdmin =
        state.admins?.find((item) => item.membership_id === membershipId) || null;
      emit();
    },
    closeAdmin() {
      state.selectedAdmin = null;
      emit();
    },
  };
}
