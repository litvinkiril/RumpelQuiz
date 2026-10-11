import { mountApp } from './assets/ui/mount.js';
// Public entry points are also used by the existing frontend tests.
export { createApi, ApiError } from './assets/core/api.js';
export { messages } from './assets/core/messages.js';
export { validatePassword } from './assets/features/auth/model.js';
export { createController } from './assets/core/controller.js';
export { renderView } from './assets/ui/view.js';
export { escapeHtml } from './assets/shared/html.js';
export { safeAvatarUrl } from './assets/shared/people.js';
export { mountApp };
if (typeof document !== 'undefined') {
  const root = document.getElementById('app');
  if (root) {
    let storage;
    try {
      storage = window.sessionStorage;
    } catch {}
    const app = mountApp(root, { storage });

    document.getElementById('account-nav')?.addEventListener('click', (event) => {
      if (app.controller.state.busy) return;
      if (!app.canLeave()) return;
      const button = event.target.closest('button');
      if (!button || button.disabled) return;
      if (button.dataset.nav) {
        app.controller.navigate(button.dataset.nav);
        return;
      }
      const action = button.dataset.action;
      if (action === 'profile') void app.controller.openProfile();
      if (action === 'logout') void app.controller.logout();
    });
    document.querySelector('.brand')?.addEventListener('click', (event) => {
      event.preventDefault();
      if (!app.controller.state.busy && app.canLeave())
        app.controller.navigate(app.controller.state.token ? 'account' : 'login');
    });
  }
}
