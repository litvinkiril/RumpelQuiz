import { testQuestionComplete } from '../features/tests/model.js';
import { questionComplete } from '../features/quizzes/model.js';
export function bindEvents({ root, controller, render, canLeave }) {
  root.addEventListener('input', (event) => {
    if (event.target.dataset.userField) {
      controller.editNewUser(event.target.dataset.userField, event.target.value);
      return;
    }
    if (event.target.dataset.testField) {
      const el = event.target;
      if (el.tagName === 'SELECT' || ['radio', 'checkbox'].includes(el.type)) return;
      controller.editTest(
        el.dataset.testField,
        el.value,
        el.dataset.q === undefined ? undefined : Number(el.dataset.q),
        el.dataset.a === undefined ? undefined : Number(el.dataset.a),
      );
      const d = controller.state.testDraft,
        complete = d.questions.length < 100 && d.questions.every(testQuestionComplete);
      const add = root.querySelector('[data-action="test-add-question"]');
      if (add) add.disabled = !complete;
      const hint = root.querySelector('#next-question-hint');
      if (hint)
        hint.textContent = complete
          ? 'Можно добавить следующий вопрос.'
          : 'Заполните вопросы, минимум два ответа и отметьте правильные варианты.';
      const dirty = root.querySelector('[data-test-dirty]');
      if (dirty) dirty.textContent = 'Есть несохранённые изменения';
      return;
    }
    const el = event.target,
      field = el.dataset.quizField;
    if (!field || el.tagName === 'SELECT' || ['radio', 'checkbox'].includes(el.type)) return;
    controller.editQuiz(
      field,
      el.value,
      el.dataset.q === undefined ? undefined : Number(el.dataset.q),
      el.dataset.a === undefined ? undefined : Number(el.dataset.a),
    );
    const d = controller.state.quizDraft;
    const add = root.querySelector('[data-action="quiz-add-question"]');
    const complete =
      d.questions.length < 100 &&
      d.questions.every((q) => questionComplete(q, d.default_time_seconds));
    if (add) add.disabled = !complete;
    const hint = root.querySelector('#next-question-hint');
    if (hint)
      hint.textContent = complete
        ? 'Можно добавить следующий вопрос.'
        : 'Заполните вопросы, минимум два ответа и отметьте правильные варианты.';
    const dirty = root.querySelector('[data-quiz-dirty]');
    if (dirty) dirty.textContent = 'Есть несохранённые изменения';
  });
  root.addEventListener('change', (event) => {
    if (event.target.hasAttribute('data-user-role')) {
      void controller.selectNewUserRole(event.target.value);
      return;
    }
    if (event.target.dataset.userField) {
      controller.editNewUser(event.target.dataset.userField, event.target.value);
      return;
    }
    if (event.target.dataset.gameAnswer) {
      controller.chooseGameAnswer(event.target.dataset.gameAnswer, event.target.checked);
      return;
    }
    const el = event.target,
      qi = el.dataset.q === undefined ? undefined : Number(el.dataset.q),
      ai = el.dataset.a === undefined ? undefined : Number(el.dataset.a);
    if (el.dataset.testChoice) {
      controller.chooseTestAnswer(el.dataset.testChoice, el.checked);
      return;
    }
    if (el.hasAttribute('data-test-image')) {
      void controller.uploadTestImage(el.files?.[0], qi, ai);
      return;
    }
    if (
      el.dataset.testField &&
      (el.tagName === 'SELECT' || ['radio', 'checkbox'].includes(el.type))
    ) {
      controller.editTest(
        el.dataset.testField,
        ['radio', 'checkbox'].includes(el.type) ? el.checked : el.value,
        qi,
        ai,
      );
      if (el.dataset.testField !== 'type') render(controller.state);
      return;
    }
    if (el.hasAttribute('data-quiz-image')) {
      void controller.uploadQuizImage(el.files?.[0], qi, ai);
      return;
    }
    if (
      el.dataset.quizField &&
      (el.tagName === 'SELECT' || ['radio', 'checkbox'].includes(el.type))
    ) {
      controller.editQuiz(
        el.dataset.quizField,
        ['radio', 'checkbox'].includes(el.type) ? el.checked : el.value,
        qi,
        ai,
      );
      if (el.dataset.quizField !== 'type') render(controller.state);
    }
  });
  root.addEventListener('submit', (event) => {
    event.preventDefault();
    if (controller.state.busy) return;
    const form = event.target;
    if (form.dataset.form === 'user-create') {
      if (form.reportValidity())
        void controller.createUser(new FormData(form).get('new-user-password'));
      return;
    }
    if (form.dataset.form === 'game-create') {
      if (form.reportValidity()) void controller.createGame(new FormData(form).get('game-name'));
      return;
    }
    if (form.dataset.form === 'game-join') {
      if (form.reportValidity()) void controller.joinGame(new FormData(form).get('game-code'));
      return;
    }
    if (form.dataset.form === 'game-origin') {
      if (form.reportValidity()) controller.setGameOrigin(new FormData(form).get('game-origin'));
      return;
    }
    if (form.dataset.form === 'test') {
      void controller.saveTest('public');
      return;
    }
    if (form.dataset.form === 'test-link') {
      if (form.reportValidity()) void controller.startTest(new FormData(form).get('test-address'));
      return;
    }
    if (form.dataset.form === 'quiz') {
      void controller.saveQuiz('ready');
      return;
    }
    if (form.dataset.form === 'quiz-search') {
      const query = new FormData(form);
      void controller.searchQuizCatalog(query.get('quiz-query'), query.has('quiz-favourites'));
      return;
    }
    if (form.dataset.form === 'test-search') {
      const query = new FormData(form);
      void controller.searchTestCatalog(query.get('test-query'), query.has('test-favourites'));
      return;
    }
    if (!form.reportValidity()) return;
    const data = Object.fromEntries(new FormData(form));
    const kind = form.dataset.form;
    if (kind === 'people') void controller.searchPeople(data.query);
    if (kind === 'login') void controller.login(data.email, data.password);
    if (kind === 'register')
      void controller.register(data.email, data.password, data.password_confirmation);
    if (kind === 'forgot') void controller.requestReset(data.email);
    if (kind === 'code') void controller.verify(data.code);
    if (kind === 'password')
      void controller.updatePassword(data.password, data.password_confirmation);
  });
  root.addEventListener('click', (event) => {
    const button = event.target.closest('button');
    if (!button || button.disabled) return;
    if (button.dataset.nav) {
      if (canLeave()) controller.navigate(button.dataset.nav);
      return;
    }
    if (controller.state.busy) return;
    const action = button.dataset.action,
      qi = Number(button.dataset.q),
      ai = button.dataset.a === undefined ? undefined : Number(button.dataset.a);
    if (action === 'account-roles') void controller.reloadAccountRoles();
    if (
      [
        'create-quiz',
        'my-quizzes',
        'quiz-catalog',
        'new-quiz',
        'edit-quiz',
        'view-quiz',
        'launch-quiz',
        'create-test',
        'test-catalog',
        'my-tests',
        'new-test',
        'edit-test',
        'available-tests',
        'start-test',
        'game-join',
        'game-resume',
        'profile',
        'create-user',
        'logout',
      ].includes(action) &&
      !canLeave()
    )
      return;
    if (action === 'quiz-catalog') void controller.openQuizCatalog();
    if (action === 'create-user') void controller.openUserCreation();
    if (action === 'user-faculties-retry') void controller.loadUserFaculties(true);
    if (action === 'quiz-catalog-retry') void controller.searchQuizCatalog();
    if (action === 'quiz-catalog-more')
      void controller.searchQuizCatalog(undefined, undefined, true);
    if (action === 'quiz-favourite') void controller.toggleQuizFavourite(button.dataset.id);
    if (action === 'test-catalog') void controller.openTestCatalog();
    if (action === 'test-catalog-retry') void controller.searchTestCatalog();
    if (action === 'test-catalog-more')
      void controller.searchTestCatalog(undefined, undefined, true);
    if (action === 'test-favourite') void controller.toggleTestFavourite(button.dataset.id);
    if (action === 'game-next') void controller.nextGameQuestion();
    if (action === 'game-close') void controller.closeGame();
    if (action === 'game-join') controller.openGameJoin();
    if (action === 'game-resume') void controller.openLastGame();
    if (action === 'game-refresh') void controller.refreshGame();
    if (action === 'game-submit') void controller.submitGameAnswer();
    if (action === 'create-test') void controller.openTests(true);
    if (action === 'my-tests') void controller.openTests();
    if (action === 'new-test') controller.newTest();
    if (action === 'edit-test') void controller.loadTest(button.dataset.id);
    if (action === 'test-save-draft') void controller.saveTest('draft');
    if (action === 'test-publish-private') void controller.saveTest('private');
    if (
      [
        'test-add-question',
        'test-remove-question',
        'test-add-answer',
        'test-remove-answer',
        'test-remove-image',
      ].includes(action)
    )
      controller.changeTest(action, qi, ai);
    if (action === 'available-tests') void controller.openAvailableTests();
    if (action === 'start-test') void controller.startTest(button.dataset.id);
    if (action === 'test-submit') void controller.submitTestAnswer();
    if (action === 'test-refresh') void controller.refreshTest();
    if (action === 'test-results') void controller.openTestResults(button.dataset.id);
    if (action === 'create-quiz') void controller.openQuizzes(true);
    if (action === 'my-quizzes') void controller.openQuizzes();
    if (action === 'quiz-actions') controller.openQuizActions(button.dataset.id);
    if (action === 'close-quiz-actions') controller.closeQuizActions();
    if (action === 'quiz-sessions') void controller.openQuizSessions(button.dataset.id);
    if (action === 'session-results') void controller.openSessionResults(button.dataset.id);
    if (action === 'new-quiz') controller.newQuiz();
    if (action === 'edit-quiz') void controller.loadQuiz(button.dataset.id);
    if (action === 'view-quiz') void controller.loadQuiz(button.dataset.id, 'quiz-preview');
    if (action === 'launch-quiz') void controller.loadQuiz(button.dataset.id, 'quiz-launch');
    if (action === 'quiz-back') controller.returnFromQuiz();
    if (action === 'quiz-save-draft') void controller.saveQuiz('draft');
    if (
      [
        'quiz-add-question',
        'quiz-remove-question',
        'quiz-add-answer',
        'quiz-remove-answer',
        'quiz-remove-image',
      ].includes(action)
    )
      controller.changeQuiz(action, qi, ai);
    if (button.dataset.action === 'logout') controller.logout();
    if (button.dataset.action === 'refresh') void controller.refresh();
    if (button.dataset.action === 'resend') void controller.resend();
    if (button.dataset.action === 'profile') void controller.openProfile();
    if (button.dataset.action === 'university')
      controller.openUniversity(button.dataset.universityId);
    if (button.dataset.action === 'admins') void controller.openAdmins();
    if (button.dataset.action === 'people') controller.openPeople();
    if (button.dataset.action === 'more-people') void controller.searchPeople('', true);
    if (button.dataset.action === 'person-contact')
      controller.openPerson(button.dataset.membershipId);
    if (button.dataset.action === 'admin-contact')
      controller.openAdmin(button.dataset.membershipId);
    if (button.dataset.action === 'close-contact') controller.closeAdmin();
    if (button.dataset.toggle) {
      const input = root.querySelector('#' + button.dataset.toggle);
      const show = input.type === 'password';
      input.type = show ? 'text' : 'password';
      button.textContent = show ? 'Скрыть' : 'Показать';
      button.setAttribute('aria-pressed', String(show));
    }
  });
}
