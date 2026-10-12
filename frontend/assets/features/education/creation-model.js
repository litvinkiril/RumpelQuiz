export const educationCreationSections = {
  faculties: {
    title: 'Добавить факультет',
    description: 'Укажите название нового факультета.',
    nameLabel: 'Название факультета',
    placeholder: 'Например, Факультет компьютерных наук',
    submit: 'Создать факультет',
    success: 'Факультет создан.',
    list: 'Открыть список факультетов',
    idField: 'faculty_id',
  },
  programs: {
    title: 'Создать программу',
    description: 'Укажите название программы и выберите её факультет.',
    nameLabel: 'Название программы',
    placeholder: 'Например, Программная инженерия',
    submit: 'Создать программу',
    success: 'Программа создана.',
    list: 'Открыть список программ',
    idField: 'program_id',
  },
  groups: {
    title: 'Создать группу',
    description: 'Укажите название группы, выберите факультет, затем программу этого факультета.',
    nameLabel: 'Название группы',
    placeholder: 'Например, ПИ-2026-01',
    submit: 'Создать группу',
    success: 'Группа создана.',
    list: 'Открыть список групп',
    idField: 'group_id',
  },
};

export function canCreateEducation(state, kind) {
  if (!Object.hasOwn(educationCreationSections, kind) || !state.token || !state.selectedUniversity)
    return false;
  const position = state.profile?.university_position?.find(
    (item) =>
      item.role === 'admin' &&
      item.university_id === state.selectedUniversity.id &&
      (!item.status || item.status === 'active'),
  );
  return (
    position?.admin_scope === 'university' ||
    (position?.admin_scope === 'faculties' && kind !== 'faculties')
  );
}
