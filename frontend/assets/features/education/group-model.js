export function groupPermissions(state) {
  const positions = (state.profile?.university_position || []).filter(
    (position) =>
      position.university_id === state.selectedGroup?.universityId &&
      (!position.status || position.status === 'active'),
  );
  const admin = positions.some((position) => position.role === 'admin');
  return {
    admin,
    clickable: admin || positions.some((position) => position.role === 'teacher'),
  };
}
