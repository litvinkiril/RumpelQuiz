export function icon(kind) {
  const paths = {
    bolt: '<path d="m14 2-9 12h7l-2 8 9-12h-7z"/>',
    edit: '<path d="M13 4H5a2 2 0 0 0-2 2v14a2 2 0 0 0 2 2h14a2 2 0 0 0 2-2v-8M16 3l5 5M10 14l-1 5 5-1L23 9l-5-5zM7 8h4M7 12h2"/>',
    chart: '<path d="M5 14v6M12 9v11M19 3v17"/>',
  };
  return `<svg viewBox="0 0 26 26" fill="${kind === 'bolt' ? 'currentColor' : 'none'}" stroke="currentColor" stroke-width="${kind === 'chart' ? 5 : 2.2}" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true">${paths[kind]}</svg>`;
}
