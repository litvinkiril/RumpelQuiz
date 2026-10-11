import { html, escape } from '../../shared/html.js';
import { icon } from '../../shared/icons.js';
const isAuthor = (state) => state.accountRoles?.some((role) => ['teacher', 'admin'].includes(role));
export function renderAccount(state, notice) {
  const student = state.accountRoles?.includes('student') && !isAuthor(state);
  return html`<section class="home-hero" aria-labelledby="home-title">
      <h1 id="home-title" tabindex="-1">Добро пожаловать в<br /><span>RumpelQuiz</span></h1>
      <p class="home-copy">
        Создавай, запускай и проходи интерактивные квизы<br class="home-break" />
        в удобном формате.
      </p>
      ${notice}<button
        type="button"
        class="home-connect"
        ${student ? 'data-action="game-join"' : 'data-nav="quiz-section"'}
      >
        Подключиться <span aria-hidden="true">→</span>
      </button>
      <button type="button" class="home-profile" data-action="profile">
        Открыть профиль <span aria-hidden="true">→</span>
      </button>
    </section>
    <section class="home-features" aria-label="Возможности RumpelQuiz">
      <article>
        <span class="feature-icon">${icon('bolt')}</span>
        <h2>Быстрый вход в квиз</h2>
        <p>Подключайся по ссылке или коду и проходи квизы без лишних действий.</p>
      </article>
      <article>
        <span class="feature-icon">${icon('edit')}</span>
        <h2>Создание тестов и квизов</h2>
        <p>Легко создавай собственные тесты и квизы с разными типами заданий.</p>
      </article>
      <article>
        <span class="feature-icon">${icon('chart')}</span>
        <h2>
          Твой аккаунт —<br />
          в профиле
        </h2>
        <p>Твой аккаунт, учебные заведения и всё необходимое для участия — в одном месте.</p>
      </article>
    </section>`;
}
function roleNotice(state) {
  if (state.busy) return '<p class="subtitle" role="status">Загружаем роль…</p>';
  if (state.accountRolesError || !Array.isArray(state.accountRoles))
    return html`<p class="notice error" role="alert">
        ${escape(state.accountRolesError || 'Не удалось определить роль. Обновите данные аккаунта.')}
      </p>
      <button type="button" class="secondary" data-action="account-roles">
        Повторить загрузку
      </button>`;
  return '<p class="subtitle">Роль в учебном заведении пока не назначена. Посмотрите привязки в своём профиле.</p><button type="button" class="secondary" data-action="profile">Открыть профиль</button>';
}
export function renderSection(state, notice) {
  const quiz = state.screen === 'quiz-section',
    author = isAuthor(state),
    student = !author && state.accountRoles?.includes('student');
  const card = (action, title, copy, kind, disabled = false) =>
    html`<button
      type="button"
      class="section-choice"
      ${disabled ? 'disabled' : `data-action="${action}"`}
    >
      <span class="feature-icon">${icon(kind)}</span><strong>${title}</strong
      ><span class="section-choice-copy">${copy}</span
      ><span class="section-choice-arrow" aria-hidden="true">${disabled ? 'Скоро' : '→'}</span>
    </button>`;
  let choices = '';
  if (author)
    choices = quiz
      ? card('create-quiz', 'Создать квиз', 'Соберите вопросы и проведите свою игру.', 'edit') +
        card(
          'quiz-catalog',
          'Посмотреть квизы',
          'Найдите квиз по названию или откройте свои.',
          'chart',
        )
      : card(
          'create-test',
          'Создать тест',
          'Подготовьте задания для самостоятельного прохождения.',
          'edit',
        ) + card('', 'Посмотреть тесты', 'Поиск по тестам скоро появится.', 'chart', true);
  else if (student)
    choices = quiz
      ? card(
          'game-join',
          'Пройти квиз',
          'Подключитесь к игре преподавателя по ссылке или коду.',
          'bolt',
        )
      : card(
          'available-tests',
          'Пройти тест',
          'Откройте назначенные тесты или введите ссылку преподавателя.',
          'edit',
        );
  return html`<button type="button" class="text-button back" data-nav="account">
      ← На главную
    </button>
    <p class="step-label">${author ? 'Мастерская преподавателя' : 'Проверим знания?'}</p>
    <h2 tabindex="-1">${quiz ? 'Квизы' : 'Тесты'}</h2>
    <p class="subtitle">
      ${author ? 'Выберите, с чего начнём.' : quiz ? 'Живая игра, интересные вопросы и новые знания.' : 'Проходите задания в своём темпе.'}
    </p>
    ${notice}${choices ? html`<div class="section-choices">${choices}</div>` : roleNotice(state)}`;
}
