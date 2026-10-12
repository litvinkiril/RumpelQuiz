import { ApiError } from '../../core/api.js';
import { messages } from '../../core/messages.js';

const catalogKinds = ['faculties', 'programs', 'groups'];
const catalogLoadErrors = {
  faculties: 'Не удалось загрузить факультеты. Попробуйте ещё раз.',
  programs: 'Не удалось загрузить программы. Попробуйте ещё раз.',
  groups: 'Не удалось загрузить группы. Попробуйте ещё раз.',
};

function catalogItems(result, kind) {
  const items = result?.[kind];
  if (
    result?.success !== true ||
    !Array.isArray(items) ||
    items.some(
      (item) => !item || typeof item.id !== 'string' || !item.id || typeof item.name !== 'string',
    )
  )
    throw new ApiError(catalogLoadErrors[kind]);
  return items;
}

export function createEducationController({
  state,
  authorized,
  run,
  emit,
  fail,
  clearEducation,
  clearEducationCreation,
  clearPeople,
  cancelPending,
}) {
  const canLoadCatalog = () =>
    !state.busy &&
    state.token &&
    state.selectedUniversity &&
    state.screen === 'education-catalog' &&
    catalogKinds.includes(state.educationCatalogKind);
  const loadEducationCatalog = () => {
    if (!canLoadCatalog()) return Promise.resolve(false);
    const kind = state.educationCatalogKind;
    const universityId = state.selectedUniversity.id;
    const query = new URLSearchParams(
      kind !== 'faculties' && state.educationCatalogFacultyId
        ? { facultet_id: state.educationCatalogFacultyId }
        : { university_id: universityId },
    );
    state.educationCatalogItems = null;
    state.educationCatalogRequest = 'items';
    return run(
      () => authorized('education/' + kind + '?' + query),
      (result) => {
        const items = catalogItems(result, kind);
        state.educationCatalogItems = items;
        if (kind === 'faculties') state.facultyCache[universityId] = items;
      },
    );
  };
  const loadEducationCatalogFaculties = () => {
    if (!canLoadCatalog() || state.educationCatalogKind === 'faculties')
      return Promise.resolve(false);
    const universityId = state.selectedUniversity.id;
    state.educationFacultyPickerOpen = true;
    state.error = '';
    state.success = '';
    if (Object.hasOwn(state.facultyCache, universityId)) {
      emit();
      return Promise.resolve(true);
    }
    state.educationCatalogRequest = 'faculties';
    const query = new URLSearchParams({ university_id: universityId });
    return run(
      () => authorized('education/faculties?' + query),
      (result) => {
        state.facultyCache[universityId] = catalogItems(result, 'faculties');
      },
    );
  };
  return {
    loadEducationCatalog,
    loadEducationCatalogFaculties,
    openEducationCatalog(kind) {
      if (
        state.busy ||
        !state.token ||
        !state.selectedUniversity ||
        !['university', 'education-catalog', 'education-create'].includes(state.screen) ||
        !catalogKinds.includes(kind)
      )
        return Promise.resolve(false);
      cancelPending();
      clearEducationCreation();
      clearPeople();
      state.selectedAdmin = null;
      state.educationCatalogKind = kind;
      state.educationCatalogItems = null;
      state.educationCatalogFacultyId = '';
      state.educationFacultyPickerOpen = false;
      state.screen = 'education-catalog';
      return loadEducationCatalog();
    },
    toggleEducationFacultyPicker() {
      if (!canLoadCatalog() || state.educationCatalogKind === 'faculties')
        return Promise.resolve(false);
      if (!state.educationFacultyPickerOpen) return loadEducationCatalogFaculties();
      state.educationFacultyPickerOpen = false;
      state.error = '';
      emit();
      return Promise.resolve(true);
    },
    selectEducationFaculty(facultyId) {
      if (!canLoadCatalog() || state.educationCatalogKind === 'faculties')
        return Promise.resolve(false);
      const faculties = state.facultyCache[state.selectedUniversity.id];
      if (facultyId !== '' && !faculties?.some((faculty) => faculty.id === facultyId))
        return Promise.resolve(false);
      state.educationFacultyPickerOpen = false;
      if (facultyId === state.educationCatalogFacultyId && state.educationCatalogItems !== null) {
        state.error = '';
        emit();
        return Promise.resolve(true);
      }
      state.educationCatalogFacultyId = facultyId;
      return loadEducationCatalog();
    },
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
      cancelPending();
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
