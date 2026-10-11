export const escapeHtml = (value) =>
  String(value).replace(
    /[&<>"']/g,
    (c) =>
      ({
        '&': '&amp;',
        '<': '&lt;',
        '>': '&gt;',
        '"': '&quot;',
        "'": '&#39;',
      })[c],
  );

export const escape = (value) => escapeHtml(value ?? '');

// A regular template string, tagged so the formatter also understands its HTML.
export function html(strings, ...values) {
  return strings.reduce(
    (output, part, index) => output + part + (index < values.length ? values[index] : ''),
    '',
  );
}
