import { createController } from '../core/controller.js';
import { renderView } from './view.js';
import { bindEvents } from './events.js';
import { gameSeconds } from '../features/game/model.js';
import { testSeconds } from '../features/tests/model.js';
import { updateGameProgress } from '../features/game/view.js';
export function mountApp(root, options = {}) {
  let lastScreen = '';
  let lastGame = null;
  let lastContactId = null;
  let lastQuizId = null;
  let controller;
  const render = (state, change) => {
    if (
      change === 'game-progress' &&
      lastScreen === 'game' &&
      state.screen === 'game' &&
      lastGame?.session.id === state.game?.session.id &&
      lastGame?.session.status === state.game?.session.status &&
      lastGame?.current_question?.id === state.game?.current_question?.id
    ) {
      updateGameProgress(root, state.game, lastGame, state.gameAlerts);
      lastGame = state.game;
      tick();
      controller?.syncGameEvents();
      return;
    }
    // Keep values across the busy/error render, never in storage.
    const values = new Map([...root.querySelectorAll('input')].map((el) => [el.name, el.value]));
    const focusName = root.ownerDocument.activeElement?.getAttribute('name');
    const focusId = root.ownerDocument.activeElement?.id;
    root.innerHTML = renderView(state);
    const signedIn = [
      'account',
      'quiz-section',
      'test-section',
      'test-catalog',
      'profile',
      'university',
      'user-create',
      'admins',
      'people',
      'quiz',
      'quizzes',
      'quiz-catalog',
      'quiz-preview',
      'quiz-launch',
      'quiz-sessions',
      'quiz-results',
      'test',
      'tests',
      'available-tests',
      'test-play',
      'test-results',
      'game',
      'game-join',
    ].includes(state.screen);
    root.ownerDocument.body.classList.toggle('signed-in', signedIn);
    root.ownerDocument.body.classList.toggle('home-screen', state.screen === 'account');
    const nav = root.ownerDocument.getElementById('account-nav');
    if (nav) {
      nav.hidden = !signedIn;
      nav.querySelectorAll('button').forEach((button) => {
        button.disabled = state.busy || (!button.dataset.action && !button.dataset.nav);
      });
      nav
        .querySelector('[data-action="profile"]')
        ?.setAttribute(
          'aria-current',
          ['profile', 'university', 'user-create', 'admins', 'people'].includes(state.screen)
            ? 'page'
            : 'false',
        );
      nav
        .querySelector('[data-nav="quiz-section"]')
        ?.setAttribute(
          'aria-current',
          [
            'quiz-section',
            'quiz',
            'quizzes',
            'quiz-catalog',
            'quiz-preview',
            'quiz-launch',
            'quiz-sessions',
            'quiz-results',
            'game',
            'game-join',
          ].includes(state.screen)
            ? 'page'
            : 'false',
        );
      nav
        .querySelector('[data-nav="test-section"]')
        ?.setAttribute(
          'aria-current',
          [
            'test-section',
            'test-catalog',
            'test',
            'tests',
            'available-tests',
            'test-play',
            'test-results',
          ].includes(state.screen)
            ? 'page'
            : 'false',
        );
    }
    root
      .closest('.workspace')
      ?.setAttribute('aria-label', signedIn ? 'Личный кабинет' : 'Авторизация');
    root.querySelectorAll('[data-avatar]').forEach((img) =>
      img.addEventListener('error', (event) => {
        event.target.remove();
      }),
    );
    if (lastScreen === 'user-create' && state.screen === 'user-create') {
      const password = root.querySelector('#new-user-password');
      if (password && values.has('new-user-password'))
        password.value = values.get('new-user-password');
      if (focusId) root.ownerDocument.getElementById(focusId)?.focus({ preventScroll: true });
    } else if (
      lastScreen === state.screen &&
      !['quiz', 'test', 'test-play'].includes(state.screen)
    ) {
      for (const el of root.querySelectorAll('input'))
        if (values.has(el.name)) el.value = values.get(el.name);
      const focus = [...root.querySelectorAll('input')].find((el) => el.name === focusName);
      focus?.focus();
    } else if (['quiz', 'test', 'test-play'].includes(lastScreen) && lastScreen === state.screen) {
      const gameName = root.querySelector('[name="game-name"]');
      if (gameName && values.has('game-name')) gameName.value = values.get('game-name');
      if (focusId) root.ownerDocument.getElementById(focusId)?.focus({ preventScroll: true });
    } else {
      root.querySelector('[tabindex="-1"]')?.focus();
    }
    lastScreen = state.screen;
    lastGame = state.game;
    for (const el of root.querySelectorAll('input,button'))
      if (state.busy && !el.dataset.nav) el.disabled = true;
    const dialog = root.querySelector('dialog');
    if (dialog) {
      const closeDialog = () =>
        dialog.hasAttribute('data-quiz-dialog')
          ? controller.closeQuizActions()
          : controller.closeAdmin();
      dialog.addEventListener('cancel', (event) => {
        event.preventDefault();
        closeDialog();
      });
      dialog.addEventListener('click', (event) => {
        if (event.target !== dialog) return;
        const bounds = dialog.getBoundingClientRect();
        if (
          event.clientX < bounds.left ||
          event.clientX > bounds.right ||
          event.clientY < bounds.top ||
          event.clientY > bounds.bottom
        )
          closeDialog();
      });
      dialog.showModal();
    } else if (lastContactId && ['admins', 'people'].includes(state.screen)) {
      [...root.querySelectorAll('[data-membership-id]')]
        .find((el) => el.dataset.membershipId === lastContactId)
        ?.focus();
    } else if (lastQuizId && state.screen === 'quizzes') {
      [...root.querySelectorAll('[data-action="quiz-actions"]')]
        .find((el) => el.dataset.id === lastQuizId)
        ?.focus();
    }
    lastContactId = state.selectedAdmin?.membership_id || state.selectedAdmin?.user_id || null;
    lastQuizId = state.selectedQuiz?.id || null;
    tick();
    controller?.syncGameEvents();
  };
  controller = createController({
    ...options,
    joinCode: new URL(root.ownerDocument.defaultView.location.href).searchParams.get('join') || '',
    testLink: new URL(root.ownerDocument.defaultView.location.href).searchParams.get('test') || '',
    onChange: render,
  });
  const tick = () => {
    controller.syncGamePresence();
    const secondsLeft = gameSeconds(controller.state);
    const timer = root.querySelector('[data-game-timer]');
    if (timer) timer.textContent = secondsLeft > 0 ? secondsLeft + ' с' : 'Время истекло';
    if (timer && secondsLeft === 0) {
      root.querySelectorAll('[data-game-answer], [data-action="game-submit"]').forEach((el) => {
        el.disabled = true;
      });
    }
    const testTimer = root.querySelector('[data-test-timer]');
    if (testTimer) {
      const left = testSeconds(controller.state);
      testTimer.textContent = left > 0 ? left + ' с' : 'Время истекло';
      if (left === 0) {
        root.querySelectorAll('[data-test-choice], [data-action="test-submit"]').forEach((el) => {
          el.disabled = true;
        });
        if (!controller.state.busy && !controller.state.error) void controller.refreshTest();
      }
    }
    const button = root.querySelector('[data-action="resend"]');
    if (!button) return;
    const seconds = controller.remaining();
    button.disabled = controller.state.busy || seconds > 0;
    button.textContent = seconds > 0 ? `Новый код через ${seconds} с` : 'Получить новый код';
  };
  const interval = setInterval(tick, 1000);
  const canLeave = () =>
    !(
      (controller.state.quizDirty && controller.state.screen === 'quiz') ||
      (controller.state.testDirty && controller.state.screen === 'test') ||
      (controller.state.userCreationDirty && controller.state.screen === 'user-create')
    ) || root.ownerDocument.defaultView.confirm('Есть несохранённые изменения. Выйти со страницы?');
  const beforeUnload = (event) => {
    if (
      (controller.state.screen === 'quiz' && controller.state.quizDirty) ||
      (controller.state.screen === 'test' && controller.state.testDirty) ||
      (controller.state.screen === 'user-create' && controller.state.userCreationDirty)
    ) {
      event.preventDefault();
      event.returnValue = '';
    }
  };
  root.ownerDocument.defaultView.addEventListener('beforeunload', beforeUnload);
  const pageHide = () => controller.suspendGamePresence();
  const pageShow = () => controller.syncGamePresence();
  root.ownerDocument.defaultView.addEventListener('pagehide', pageHide);
  root.ownerDocument.defaultView.addEventListener('pageshow', pageShow);
  bindEvents({ root, controller, render, canLeave });
  void controller.start();
  return {
    controller,
    canLeave,
    destroy: () => {
      clearInterval(interval);
      controller.resetGameView();
      root.ownerDocument.defaultView.removeEventListener('beforeunload', beforeUnload);
      root.ownerDocument.defaultView.removeEventListener('pagehide', pageHide);
      root.ownerDocument.defaultView.removeEventListener('pageshow', pageShow);
    },
  };
}
