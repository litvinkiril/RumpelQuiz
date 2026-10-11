import { html, escapeHtml } from '../shared/html.js';
import { renderAuth } from '../features/auth/view.js';
import { renderProfile } from '../features/profile/view.js';
import { renderEducation } from '../features/education/view.js';
import { renderAccount, renderSection } from '../features/home/view.js';
import { renderCatalog } from '../features/catalog/view.js';
import { renderQuiz } from '../features/quizzes/view.js';
import { renderTests } from '../features/tests/view.js';
import { renderGame } from '../features/game/view.js';

const views = {
  account: renderAccount,
  login: renderAuth,
  register: renderAuth,
  forgot: renderAuth,
  code: renderAuth,
  password: renderAuth,
  profile: renderProfile,
  university: renderEducation,
  people: renderEducation,
  admins: renderEducation,
  'quiz-section': renderSection,
  'test-section': renderSection,
  'quiz-catalog': renderCatalog,
  quiz: renderQuiz,
  quizzes: renderQuiz,
  'quiz-sessions': renderQuiz,
  'quiz-results': renderQuiz,
  test: renderTests,
  tests: renderTests,
  'available-tests': renderTests,
  'test-play': renderTests,
  'test-results': renderTests,
  game: renderGame,
  'game-join': renderGame,
};
export function renderView(state) {
  const notice = state.error
    ? html`<div class="notice error" role="alert">${escapeHtml(state.error)}</div>`
    : state.success
      ? html`<div class="notice success" role="status">${escapeHtml(state.success)}</div>`
      : '';
  const render = views[state.screen] || renderAccount;
  const content = render(state, notice);
  return html`<div
    class="auth-card${state.screen === 'account' ? ' home-content' : ''}"
    aria-busy="${!!state.busy}"
  >
    ${content}
  </div>`;
}
