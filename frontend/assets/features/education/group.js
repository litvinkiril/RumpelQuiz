import { ApiError } from '../../core/api.js';

function groupMembers(result) {
  if (
    result?.success !== true ||
    ['teachers', 'students'].some(
      (kind) =>
        !Array.isArray(result[kind]) ||
        result[kind].some(
          (member) =>
            !member ||
            typeof member.id !== 'string' ||
            !member.id ||
            ['first_name', 'last_name', 'middle_name', 'avatar_url'].some(
              (field) => member[field] !== null && typeof member[field] !== 'string',
            ),
        ),
    )
  )
    throw new ApiError('Не удалось загрузить участников группы. Попробуйте ещё раз.');
  return { teachers: result.teachers, students: result.students };
}

function groupSource(state, groupId) {
  if (state.screen === 'profile') {
    const position = state.profile?.university_position?.find(
      (item) =>
        item.role === 'student' &&
        item.group_id === groupId &&
        item.university_id &&
        (!item.status || item.status === 'active'),
    );
    return (
      position && {
        id: position.group_id,
        name: position.group_name || 'Учебная группа',
        universityId: position.university_id,
        universityName: position.university_name,
      }
    );
  }
  const items =
    state.screen === 'university' && state.educationSearchKind === 'groups'
      ? state.educationSearchItems
      : state.screen === 'education-catalog' && state.educationCatalogKind === 'groups'
        ? state.educationCatalogItems
        : null;
  const group = items?.find((item) => item.id === groupId);
  return (
    group &&
    state.selectedUniversity && {
      id: group.id,
      name: group.name,
      universityId: state.selectedUniversity.id,
      universityName: state.selectedUniversity.name,
    }
  );
}

export function createGroupController({ state, api, run, emit, cancelPending, clearGroup }) {
  const loadGroupMembers = () => {
    if (state.busy || !state.token || state.screen !== 'group' || !state.selectedGroup)
      return Promise.resolve(false);
    const id = state.selectedGroup.id;
    state.groupMembers = null;
    return run(
      () => api('education/groups/' + encodeURIComponent(id) + '/members'),
      (result) => {
        state.groupMembers = groupMembers(result);
      },
    );
  };
  return {
    loadGroupMembers,
    openGroup(groupId) {
      if (state.busy || !state.token || typeof groupId !== 'string' || !groupId)
        return Promise.resolve(false);
      const group = groupSource(state, groupId);
      if (!group) return Promise.resolve(false);
      const returnState = {
        screen: state.screen,
        search:
          state.screen === 'university'
            ? {
                educationSearchKind: state.educationSearchKind,
                educationSearchQuery: state.educationSearchQuery,
                educationSearchItems: state.educationSearchItems,
                educationSearchError: state.educationSearchError,
              }
            : null,
      };
      cancelPending();
      clearGroup();
      state.selectedGroup = group;
      state.groupReturn = returnState;
      state.selectedAdmin = null;
      state.screen = 'group';
      return loadGroupMembers();
    },
    returnFromGroup() {
      if (state.screen !== 'group' || !state.token || !state.groupReturn) return false;
      const { screen, search } = state.groupReturn;
      cancelPending();
      clearGroup();
      state.busy = false;
      state.error = '';
      state.errorCode = '';
      state.success = '';
      state.screen = screen;
      if (search) Object.assign(state, search);
      emit();
      return true;
    },
  };
}
