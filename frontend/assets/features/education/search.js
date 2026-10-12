import { ApiError } from '../../core/api.js';

const searchError = 'Не удалось загрузить результаты. Попробуйте ещё раз.';

function searchItems(result, kind) {
  const items = result?.[kind];
  if (
    result?.success !== true ||
    !Array.isArray(items) ||
    items.length > 30 ||
    items.some(
      (item) =>
        !item ||
        typeof item.id !== 'string' ||
        !item.id ||
        typeof item.name !== 'string' ||
        !Array.isArray(item.facultet_names) ||
        item.facultet_names.some((name) => typeof name !== 'string') ||
        (kind === 'groups' && item.program_name !== null && typeof item.program_name !== 'string'),
    )
  )
    throw new ApiError(searchError);
  return items;
}

export function createEducationSearchController({ state, api, emit }) {
  let sequence = 0;
  let timer;
  const canSearch = () =>
    !state.busy && state.screen === 'university' && state.token && state.selectedUniversity;
  const cancel = () => {
    clearTimeout(timer);
    timer = undefined;
    ++sequence;
    state.educationSearchItems = null;
    state.educationSearchLoading = false;
    state.educationSearchError = '';
  };
  const searchEducation = async (
    query = state.educationSearchQuery,
    kind = state.educationSearchKind,
  ) => {
    if (!canSearch() || typeof query !== 'string' || !['groups', 'programs'].includes(kind))
      return false;
    cancel();
    state.educationSearchQuery = query;
    state.educationSearchKind = kind;
    const name = query.trim();
    if (!name) {
      emit('education-search');
      return false;
    }
    if (name.includes('\0')) {
      state.educationSearchError = 'Введите название без нулевого символа.';
      emit('education-search');
      return false;
    }
    const current = sequence;
    const universityId = state.selectedUniversity.id;
    const isCurrent = () =>
      current === sequence &&
      state.screen === 'university' &&
      state.selectedUniversity?.id === universityId &&
      state.educationSearchKind === kind &&
      state.educationSearchQuery === query;
    const params = new URLSearchParams({ university_id: universityId, name });
    state.educationSearchLoading = true;
    emit('education-search');
    try {
      const result = await api('education/search/' + kind + '?' + params);
      if (!isCurrent()) return false;
      if (result?.success === false && result.error === 'too_many_results') {
        state.educationSearchError = 'Введите более точное название';
        return false;
      }
      state.educationSearchItems = searchItems(result, kind);
      return true;
    } catch (error) {
      if (isCurrent())
        state.educationSearchError = error instanceof ApiError ? error.message : searchError;
      return false;
    } finally {
      if (isCurrent()) {
        state.educationSearchLoading = false;
        emit('education-search');
      }
    }
  };
  return {
    searchEducation,
    resetEducationSearch() {
      cancel();
      state.educationSearchKind = 'groups';
      state.educationSearchQuery = '';
    },
    editEducationSearchQuery(query, composing = false) {
      if (!canSearch() || typeof query !== 'string') return;
      cancel();
      state.educationSearchQuery = query;
      if (!composing && query.trim()) timer = setTimeout(() => void searchEducation(), 300);
      emit('education-search');
    },
    selectEducationSearchKind(kind) {
      if (!canSearch() || !['groups', 'programs'].includes(kind)) return Promise.resolve(false);
      return searchEducation(state.educationSearchQuery, kind);
    },
  };
}
