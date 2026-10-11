import { html, escape } from '../../shared/html.js';

function quizBack(state) {
  return html`<button type="button" class="text-button back" data-action="quiz-back">
    ${state.quizReturnScreen === 'quiz-catalog' ? '← Посмотреть квизы' : '← Мои квизы'}
  </button>`;
}

function previewImage(item, label) {
  if (!/^https:\/\//.test(item.image_url || '')) return '';
  return html`<img
    class="quiz-preview-image"
    src="${escape(item.image_url)}"
    alt="${escape(label)}"
    referrerpolicy="no-referrer"
    loading="lazy"
  />`;
}

export function renderQuizLaunchForm() {
  return html`<form class="game-launch" data-form="game-create">
    <p>Участники смогут подключиться по коду или QR.</p>
    <div class="field">
      <label for="game-name">Название сессии</label>
      <input id="game-name" name="game-name" required placeholder="Например, ИС-21 · Практика 3" />
      <p class="hint">От 1 до 200 символов.</p>
    </div>
    <button type="submit" class="primary">Создать сессию</button>
  </form>`;
}

export function renderQuizLaunch(state, notice) {
  const quiz = state.quizDraft;
  if (!quiz) return notice;
  return html`${quizBack(state)}
    <p class="step-label">Новая игра</p>
    <h2 tabindex="-1">Создать сессию</h2>
    <p class="subtitle quiz-preview-text">${escape(quiz.name || 'Без названия')}</p>
    <p class="hint">Вопросов: ${quiz.questions.length}</p>
    ${notice}
    ${quiz.status === 'ready' ? renderQuizLaunchForm() : '<p class="notice">Создать сессию можно только для опубликованного квиза.</p>'}
    <button type="button" class="text-button" data-action="view-quiz" data-id="${escape(quiz.id)}">
      Посмотреть квиз
    </button>`;
}

export function renderQuizPreview(state, notice) {
  const quiz = state.quizDraft;
  if (!quiz) return notice;
  return html`${quizBack(state)}
    <p class="step-label">Просмотр квиза</p>
    <div class="quiz-heading">
      <h2 tabindex="-1" class="quiz-preview-text">${escape(quiz.name || 'Без названия')}</h2>
      <span class="quiz-badge ${quiz.status === 'ready' ? 'ready' : ''}">
        ${quiz.status === 'ready' ? 'Опубликован' : 'Черновик'}
      </span>
    </div>
    ${quiz.description ? html`<p class="subtitle quiz-preview-text">${escape(quiz.description)}</p>` : ''}
    <p class="hint">Вопросов: ${quiz.questions.length}. Правильные ответы отмечены.</p>
    ${notice}
    ${quiz.status === 'ready' ? html`<button type="button" class="primary quiz-preview-launch" data-action="launch-quiz" data-id="${escape(quiz.id)}">Создать сессию</button>` : ''}
    <div class="quiz-preview-questions">
      ${
        quiz.questions
          .map(
            (question, i) =>
              html`<section class="question-card-editor quiz-preview-question">
                <div class="question-heading">
                  <h3>Вопрос ${i + 1}</h3>
                  <span class="quiz-badge"
                    >${escape(question.time_seconds ?? quiz.default_time_seconds)} с</span
                  >
                </div>
                <p class="quiz-preview-text">${escape(question.text)}</p>
                ${previewImage(question, `Картинка вопроса ${i + 1}`)}
                <p class="hint">
                  ${question.type === 'single' ? 'Один правильный ответ' : 'Несколько правильных ответов'}
                </p>
                <ol class="quiz-preview-answers">
                  ${question.answers
                    .map(
                      (answer, j) =>
                        html`<li class="${answer.is_correct ? 'correct' : ''}">
                          <p class="quiz-preview-text">${escape(answer.text)}</p>
                          ${previewImage(answer, `Картинка ответа ${i + 1}.${j + 1}`)}
                          ${answer.is_correct ? '<span class="quiz-preview-correct">✓ Правильный ответ</span>' : ''}
                        </li>`,
                    )
                    .join('')}
                </ol>
              </section>`,
          )
          .join('') || '<p class="empty-list">В этом квизе пока нет вопросов.</p>'
      }
    </div>`;
}
