import { ApiError } from '../../core/api.js';
import { canCreateEducation, educationCreationSections } from './creation-model.js';

const creationErrors = {
  faculty_already_exists: 'Факультет с таким названием уже есть в этом университете.',
  program_already_exists: 'Программа с таким названием уже есть на выбранном факультете.',
  group_already_exists: 'Группа с таким названием уже есть в выбранной программе.',
  invalid_faculty: 'Факультет недоступен. Обновите список и выберите факультет ещё раз.',
  invalid_program: 'Программа недоступна. Обновите список и выберите программу ещё раз.',
  university_access_denied:
    'У вас нет прав создавать этот объект в выбранном университете или факультете.',
  university_not_found: 'Университет не найден. Вернитесь в профиль и обновите его.',
  invalid_request: 'Проверьте название и выбранные значения.',
};

function namedItems(result, kind) {
  const items = result?.[kind];
  if (
    result?.success !== true ||
    !Array.isArray(items) ||
    items.some(
      (item) => !item || typeof item.id !== 'string' || !item.id || typeof item.name !== 'string',
    )
  )
    throw new ApiError(
      kind === 'faculties'
        ? 'Не удалось загрузить факультеты. Попробуйте ещё раз.'
        : 'Не удалось загрузить программы. Попробуйте ещё раз.',
    );
  return items;
}

export function createEducationCreationController({
  state,
  authorized,
  run,
  emit,
  fail,
  cancelPending,
  clearEducationCatalog,
  clearEducationCreation,
}) {
  const canEdit = () =>
    !state.busy &&
    state.screen === 'education-create' &&
    !state.educationCreationSaved &&
    canCreateEducation(state, state.educationCreationKind);
  const loadEducationCreationPrograms = () => {
    if (
      !canEdit() ||
      state.educationCreationKind !== 'groups' ||
      !state.educationCreationFaculties?.some(
        (item) => item.id === state.educationCreationDraft?.faculty_id,
      )
    )
      return Promise.resolve(false);
    const facultyId = state.educationCreationDraft.faculty_id;
    state.educationCreationPrograms = null;
    state.educationCreationRequest = 'programs';
    const query = new URLSearchParams({ facultet_id: facultyId });
    return run(
      () => authorized('education/programs?' + query),
      (result) => {
        const programs = namedItems(result, 'programs');
        state.educationCreationPrograms = programs;
        if (!programs.some((item) => item.id === state.educationCreationDraft.program_id))
          state.educationCreationDraft.program_id = '';
      },
    );
  };
  const loadEducationCreationFaculties = (force = false) => {
    if (!canEdit() || state.educationCreationKind === 'faculties') return Promise.resolve(false);
    const universityId = state.selectedUniversity.id;
    if (!force && Object.hasOwn(state.facultyCache, universityId)) {
      state.educationCreationFaculties = state.facultyCache[universityId];
      emit();
      return Promise.resolve(true);
    }
    state.educationCreationFaculties = null;
    state.educationCreationRequest = 'faculties';
    const query = new URLSearchParams({ university_id: universityId });
    return run(
      () => authorized('education/faculties?' + query),
      (result) => {
        const faculties = namedItems(result, 'faculties');
        state.facultyCache[universityId] = faculties;
        state.educationCreationFaculties = faculties;
        if (!faculties.some((item) => item.id === state.educationCreationDraft.faculty_id)) {
          state.educationCreationDraft.faculty_id = '';
          state.educationCreationDraft.program_id = '';
          state.educationCreationPrograms = null;
        }
      },
    );
  };
  return {
    loadEducationCreationFaculties,
    loadEducationCreationPrograms,
    openEducationCreation(kind) {
      if (
        state.busy ||
        !['university', 'education-create'].includes(state.screen) ||
        !canCreateEducation(state, kind)
      )
        return Promise.resolve(false);
      cancelPending();
      clearEducationCatalog();
      clearEducationCreation();
      state.educationCreationKind = kind;
      state.educationCreationDraft = { name: '', faculty_id: '', program_id: '' };
      state.screen = 'education-create';
      state.error = '';
      state.success = '';
      if (kind !== 'faculties') return loadEducationCreationFaculties();
      emit();
      return Promise.resolve(true);
    },
    editEducationName(value) {
      if (!canEdit() || typeof value !== 'string') return;
      state.educationCreationDraft.name = value;
      state.educationCreationDirty = true;
    },
    selectEducationCreationFaculty(facultyId) {
      if (
        !canEdit() ||
        state.educationCreationKind === 'faculties' ||
        (facultyId !== '' &&
          !state.educationCreationFaculties?.some((item) => item.id === facultyId))
      )
        return Promise.resolve(false);
      state.educationCreationDraft.faculty_id = facultyId;
      state.educationCreationDraft.program_id = '';
      state.educationCreationPrograms = null;
      state.educationCreationDirty = true;
      state.error = '';
      if (facultyId && state.educationCreationKind === 'groups')
        return loadEducationCreationPrograms();
      emit();
      return Promise.resolve(true);
    },
    selectEducationCreationProgram(programId) {
      if (
        !canEdit() ||
        state.educationCreationKind !== 'groups' ||
        (programId !== '' &&
          !state.educationCreationPrograms?.some((item) => item.id === programId))
      )
        return false;
      state.educationCreationDraft.program_id = programId;
      state.educationCreationDirty = true;
      state.error = '';
      emit();
      return true;
    },
    createEducationItem() {
      if (!canEdit()) return Promise.resolve(false);
      const kind = state.educationCreationKind;
      const draft = state.educationCreationDraft;
      const name = draft.name.trim();
      if (!name || name.includes('\0')) return fail('Введите название. Оно не должно быть пустым.');
      if (
        kind !== 'faculties' &&
        !state.educationCreationFaculties?.some((item) => item.id === draft.faculty_id)
      )
        return fail('Выберите факультет из списка.');
      if (
        kind === 'groups' &&
        !state.educationCreationPrograms?.some((item) => item.id === draft.program_id)
      )
        return fail('Выберите программу выбранного факультета.');
      const universityId = state.selectedUniversity.id;
      const payload = { university_id: universityId, name };
      if (kind === 'programs') payload.faculty_ids = [draft.faculty_id];
      if (kind === 'groups') payload.program_id = draft.program_id;
      state.educationCreationRequest = 'create';
      return run(
        async () => {
          try {
            return await authorized('education/' + kind, payload);
          } catch (error) {
            if (creationErrors[error.code])
              throw new ApiError(creationErrors[error.code], error.status, error.code);
            throw error;
          }
        },
        (result) => {
          const id = result?.[educationCreationSections[kind].idField];
          if (result?.success !== true || typeof id !== 'string' || !id)
            throw new ApiError(
              'Сервер не подтвердил создание. Обновите список перед повторной попыткой.',
            );
          state.educationCreationSaved = { id, name };
          state.educationCreationDirty = false;
          state.success = educationCreationSections[kind].success;
          if (kind === 'faculties') delete state.facultyCache[universityId];
        },
      );
    },
  };
}
