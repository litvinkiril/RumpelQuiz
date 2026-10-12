export function icon(kind) {
  const paths = {
    bolt: '<path d="m14 2-9 12h7l-2 8 9-12h-7z"/>',
    edit: '<path d="M13 4H5a2 2 0 0 0-2 2v14a2 2 0 0 0 2 2h14a2 2 0 0 0 2-2v-8M16 3l5 5M10 14l-1 5 5-1L23 9l-5-5zM7 8h4M7 12h2"/>',
    chart: '<path d="M5 14v6M12 9v11M19 3v17"/>',
    university: '<path d="m3 9 10-6 10 6H3ZM5 10v12M21 10v12M10 12v7M16 12v7M3 23h20M9 22h8"/>',
    document: '<path d="M15 3H6a2 2 0 0 0-2 2v18h18V10l-7-7ZM15 3v7h7M9 14h8M9 18h8"/>',
    people:
      '<circle cx="10" cy="8" r="4"/><path d="M2 23v-3a6 6 0 0 1 6-6h4a6 6 0 0 1 6 6v3H2ZM18 4a4 4 0 0 1 0 8M20 15a6 6 0 0 1 4 5v3h-3"/>',
    search: '<circle cx="11" cy="11" r="8"/><path d="m17 17 7 7"/>',
    arrow: '<path d="M5 13h16m-6-6 6 6-6 6"/>',
    book: '<path d="M13 6v17M13 6C9 3 5 3 2 4v17c4-1 7-1 11 2 4-3 7-3 11-2V4c-3-1-7-1-11 2Z"/>',
    'user-minus':
      '<g fill="currentColor" stroke="none"><circle cx="10" cy="7" r="4"/><path d="M10 13a8 8 0 0 0-8 8v2h11a8 8 0 0 1 2-8 8 8 0 0 0-5-2Z"/><circle cx="19" cy="19" r="6"/></g><path d="M16.5 19h5" stroke="white" stroke-width="1.8"/>',
  };
  return `<svg viewBox="0 0 26 26" fill="${kind === 'bolt' ? 'currentColor' : 'none'}" stroke="currentColor" stroke-width="${kind === 'chart' ? 5 : 2.2}" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true">${paths[kind]}</svg>`;
}
