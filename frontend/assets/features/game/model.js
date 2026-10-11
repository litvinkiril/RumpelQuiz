import qrcode from '../../../vendor/qrcode.js';
export const KEY = 'rumpelquiz.game';
export const UUID = /^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/i;
export const finished = (game) => ['finished', 'cancelled'].includes(game?.session.status);
export function makeGameId() {
  const bytes = globalThis.crypto.getRandomValues(new Uint8Array(16));
  bytes[6] = (bytes[6] & 15) | 64;
  bytes[8] = (bytes[8] & 63) | 128;
  const h = [...bytes].map((b) => b.toString(16).padStart(2, '0')).join('');
  return `${h.slice(0, 8)}-${h.slice(8, 12)}-${h.slice(12, 16)}-${h.slice(16, 20)}-${h.slice(20)}`;
}
export function joinLink(origin, code) {
  const url = new URL(origin);
  if (
    !['http:', 'https:'].includes(url.protocol) ||
    url.username ||
    url.password ||
    !/^\d{6}$/.test(code)
  )
    throw new Error('invalid_join_url');
  return url.origin + '/?join=' + code;
}
const qrCache = new Map();
export function qrSvg(url) {
  if (qrCache.has(url)) return qrCache.get(url);
  const qr = qrcode(0, 'M');
  qr.addData(url);
  qr.make();
  const svg = qr.createSvgTag({ cellSize: 4, margin: 16, scalable: true });
  if (qrCache.size > 5) qrCache.clear();
  qrCache.set(url, svg);
  return svg;
}
export function gameSeconds(state, now = Date.now()) {
  const q = state.game?.current_question;
  return q && q.accepting_answers
    ? Math.max(0, Math.ceil((q.deadline_at_ms - now - (state.gameOffset || 0)) / 1000))
    : 0;
}
