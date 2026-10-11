import { html, escape } from '../../shared/html.js';
import { testQuestionComplete, testSeconds } from './model.js';
const statusName = (s) =>
  ({
    draft: 'Черновик',
    public: 'Public',
    private: 'Private',
    in_progress: 'В процессе',
    completed: 'Завершён',
    expired: 'Время истекло',
  })[s] || s;
function showImage(url) {
  return /^https:\/\//.test(url || '')
    ? html`<img src="${escape(url)}" alt="Картинка" referrerpolicy="no-referrer" />`
    : '';
}
export function renderTests(state, notice) {
  if (state.screen === 'test') return renderTestEditor(state, notice);
  if (state.screen === 'tests')
    return html`<button class="text-button back" data-nav="account">← На главную</button>
      <h2 tabindex="-1">Мои тесты</h2>
      ${notice}<button class="primary" data-action="new-test">＋ Создать тест</button>
      <p class="hint">Последние 100 тестов, сначала новые.</p>
      <div class="quiz-list">
        ${
          (state.tests || [])
            .map(
              (t) =>
                html`<button
                  class="quiz-list-item"
                  data-action="edit-test"
                  data-id="${escape(t.id)}"
                >
                  <span
                    ><strong>${escape(t.name || 'Без названия')}</strong
                    ><small>${escape(t.university_name)}</small></span
                  ><span class="quiz-badge ${t.status === 'draft' ? '' : 'ready'}"
                    >${escape(statusName(t.status))}</span
                  >
                </button>`,
            )
            .join('') || '<p>Сохранённых тестов пока нет.</p>'
        }
      </div>`;
  if (state.screen === 'available-tests')
    return html`<button class="text-button back" data-nav="account">← На главную</button>
      <h2 tabindex="-1">Доступные тесты</h2>
      ${notice}
      <p class="hint">
        Одна попытка на тест. После начала время идёт непрерывно. Все принятые ответы сохраняются.
      </p>
      <form data-form="test-link">
        <div class="field">
          <label for="test-address">Ссылка или идентификатор private-теста</label
          ><input
            id="test-address"
            name="test-address"
            value="${escape(state.testLink)}"
            required
          />
        </div>
        <button class="primary" type="submit">Начать / продолжить</button>
      </form>
      <div class="quiz-list">
        ${
          (state.availableTests || [])
            .map(
              (t) =>
                html`<button
                  class="quiz-list-item"
                  data-action="start-test"
                  data-id="${escape(t.id)}"
                >
                  <span
                    ><strong>${escape(t.name)}</strong
                    ><small>${escape(t.university_name)} · ${escape(t.time_to_complete)} сек.</small
                    ><small>${escape(t.description)}</small></span
                  ><span class="quiz-badge"
                    >${t.attempt_status ? escape(statusName(t.attempt_status)) : 'Начать'}</span
                  >
                </button>`,
            )
            .join('') || '<p>Публичных тестов вашего вуза пока нет.</p>'
        }
      </div>`;
  if (state.screen === 'test-results')
    return html`<button
        class="text-button back"
        data-action="edit-test"
        data-id="${escape(state.testResultsId)}"
      >
        ← К тесту
      </button>
      <h2 tabindex="-1">Результаты учеников</h2>
      ${notice}<button class="secondary" data-action="test-results">Обновить</button>
      <div class="quiz-list">
        ${
          (state.testResults || [])
            .map(
              (r) =>
                html`<div class="quiz-list-item">
                  <span
                    ><strong>${escape(r.name || r.student_id)}</strong
                    ><small
                      >${escape(statusName(r.status))} · Отвечено ${escape(r.answered_count)} из
                      ${escape(r.question_count)}</small
                    ></span
                  ><strong>${escape(r.score)} / ${escape(r.question_count)}</strong>
                </div>`,
            )
            .join('') || '<p>Прохождений пока нет.</p>'
        }
      </div>`;
  if (state.screen === 'test-play') {
    const d = state.testPlay;
    if (!d) return notice;
    const q = d.current_question;
    return html`<button class="text-button back" data-action="available-tests">
        ← Доступные тесты
      </button>
      <h2 tabindex="-1">${escape(d.test.name)}</h2>
      ${notice}${
        q
          ? html`<p>
                Вопрос ${q.position + 1} из ${d.question_count} · Осталось
                <span data-test-timer>${testSeconds(state)} с</span>
              </p>
              <p class="hint">
                Выход не останавливает таймер. К принятым ответам вернуться нельзя.
              </p>
              <section class="question-card-editor">
                <h3>${escape(q.text)}</h3>
                <div class="quiz-image">${showImage(q.image_url)}</div>
                <fieldset class="quiz-fields" ${state.busy ? 'disabled' : ''}>
                  <legend>
                    ${q.type === 'single' ? 'Один правильный ответ' : 'Несколько правильных ответов'}
                  </legend>
                  ${q.answers
                    .map(
                      (a) =>
                        html`<label class="game-answer"
                          ><input
                            type="${q.type === 'single' ? 'radio' : 'checkbox'}"
                            name="test-choice"
                            data-test-choice="${escape(a.id)}"
                            ${(state.testChoices || []).includes(a.id) ? 'checked' : ''}
                          /><span
                            >${escape(a.text)}<span class="quiz-image"
                              >${showImage(a.image_url)}</span
                            ></span
                          ></label
                        >`,
                    )
                    .join('')}<button class="primary" data-action="test-submit">
                    Ответить${q.position + 1 === d.question_count ? ' и завершить' : ''}
                  </button>
                </fieldset>
              </section>`
          : html`<p>
                ${d.attempt.status === 'expired' ? 'Время истекло. Сохранённые ответы учтены.' : 'Тест завершён.'}
              </p>
              <p>
                Результат:
                <strong>${escape(d.result?.score)} из ${escape(d.question_count)}</strong>
              </p>
              <p>Отвечено: ${escape(d.answered_count)} из ${escape(d.question_count)}.</p>`
      }<button class="secondary" data-action="test-refresh">Обновить состояние</button>`;
  }
  return notice;
}
function errorLabel(path) {
  const question = /^questions\[(\d+)\]/.exec(path),
    answer = /\.answers\[(\d+)\]/.exec(path);
  if (question)
    return `Вопрос ${Number(question[1]) + 1}${answer ? `, ответ ${Number(answer[1]) + 1}` : ''}`;
  return (
    {
      name: 'Название',
      description: 'Описание',
      university_id: 'Учебное заведение',
      time_to_complete: 'Время',
      questions: 'Вопросы',
      revision: 'Версия теста',
    }[path] || 'Тест'
  );
}

function imageField(item, q, a) {
  const attrs = `data-q="${q}" ${a === undefined ? '' : `data-a="${a}"`}`;
  const url = /^(https:\/\/|blob:)/.test(item.image_url || '') ? item.image_url : '';
  return html`<div class="quiz-image">
    ${url ? html`<img src="${escape(url)}" alt="Прикреплённая картинка" referrerpolicy="no-referrer" />` : ''}
    ${item.image_id ? html`<button type="button" class="text-button" data-action="test-remove-image" ${attrs}>Убрать картинку</button>` : ''}
    <label class="image-picker"
      >${item.image_id ? 'Заменить картинку' : '＋ Добавить картинку'}<input
        type="file"
        accept="image/png,image/jpeg,image/webp"
        data-test-image
        ${attrs}
        aria-label="Картинка ${a === undefined ? 'вопроса' : 'ответа'} ${q + 1}${a === undefined ? '' : '.' + (a + 1)}"
    /></label>
    ${item.upload_error ? html`<p class="quiz-field-error">${escape(item.upload_error)} Выберите файл ещё раз.</p>` : ''}
  </div>`;
}

export function renderTestEditor(state, notice) {
  const d = state.testDraft,
    errors = state.testErrors || [];
  const fieldError = (path) =>
    errors
      .filter((e) => e.field === path)
      .map((e) => html`<p class="quiz-field-error" role="alert">${escape(e.message)}</p>`)
      .join('');
  if (!d) return notice;
  const published = d.status !== 'draft';
  const canAdd = d.questions.every((q) => testQuestionComplete(q)) && d.questions.length < 100;
  return html`<button type="button" class="text-button back" data-action="my-tests">
      ← Мои тесты
    </button>
    <div class="quiz-heading">
      <div>
        <p class="step-label">Мастерская преподавателя</p>
        <h2 tabindex="-1">
          ${published ? 'Просмотр теста' : d.id ? 'Редактирование теста' : 'Новый тест'}
        </h2>
      </div>
      <span class="quiz-badge ${d.status !== 'draft' ? 'ready' : ''}"
        >${d.status !== 'draft' ? 'Опубликован' : 'Черновик'}</span
      >
    </div>
    <p class="subtitle">
      ${published ? 'Тест опубликован. Редактирование и возврат в черновик недоступны.' : 'Начните с вопроса. Незавершённую работу можно сохранить в черновик.'}
    </p>
    ${notice}
    ${errors.length ? html`<div class="notice error" role="alert">${errors.map((e) => html`<div>${escape(errorLabel(e.field))}: ${escape(e.message)}</div>`).join('')}</div>` : ''}
    ${published ? html`<div class="field"><label for="test-link">Ссылка для студентов своего вуза</label><input id="test-link" value="${escape(state.testShareUrl)}" readonly /><button type="button" class="secondary" data-action="test-results" data-id="${escape(d.id)}">Результаты учеников</button></div>` : ''}
    <form data-form="test" novalidate>
      <fieldset class="quiz-fields" ${state.busy || published ? 'disabled' : ''}>
        <section class="quiz-settings">
          <div class="field">
            <label for="test-university">Учебное заведение</label
            ><select id="test-university" data-test-field="university_id">
              <option value="">Выберите вуз</option>
              ${(state.testUniversities || []).map((u) => html`<option value="${escape(u.id)}" ${u.id === d.university_id ? 'selected' : ''}>${escape(u.name)}</option>`).join('')}</select
            >${fieldError('university_id')}
          </div>
          <div class="field">
            <label for="test-name">Название теста</label
            ><input
              id="test-name"
              data-test-field="name"
              value="${escape(d.name)}"
              maxlength="500"
              placeholder="Например, география без границ"
            />${fieldError('name')}
          </div>
          <div class="field">
            <label for="test-description">Описание <span class="hint">необязательно</span></label
            ><textarea
              id="test-description"
              data-test-field="description"
              rows="3"
              maxlength="10000"
              placeholder="О чём этот тест?"
            >
${escape(d.description)}</textarea>
          </div>
          <div class="field">
            <label for="test-time">Время на весь тест, сек.</label
            ><input
              id="test-time"
              type="number"
              min="1"
              max="2147483647"
              step="1"
              data-test-field="time_to_complete"
              value="${escape(d.time_to_complete)}"
            />
            <p class="hint">Таймер продолжает идти, даже если ученик выйдет из теста.</p>
            ${fieldError('time_to_complete')}
          </div>
        </section>
        <div class="quiz-questions">
          ${d.questions
            .map(
              (q, i) =>
                html`<section class="question-card-editor">
                  <div class="question-heading">
                    <h3>Вопрос ${i + 1}</h3>
                    ${d.questions.length > 1 ? html`<button type="button" class="text-button" data-action="test-remove-question" data-q="${i}">Удалить вопрос</button>` : ''}
                  </div>
                  <div class="field">
                    <label for="q-${i}-text">Текст вопроса</label
                    ><textarea
                      id="q-${i}-text"
                      data-q="${i}"
                      data-test-field="text"
                      rows="2"
                      maxlength="10000"
                      placeholder="Что вы хотите спросить?"
                    >
${escape(q.text)}</textarea
                    >${fieldError(`questions[${i}].text`)}
                  </div>
                  ${imageField(q, i)}
                  <div class="question-options">
                    <div class="field">
                      <label for="q-${i}-type">Правильные ответы</label
                      ><select id="q-${i}-type" data-q="${i}" data-test-field="type">
                        <option value="single" ${q.type === 'single' ? 'selected' : ''}>
                          Один правильный
                        </option>
                        <option value="multy" ${q.type === 'multy' ? 'selected' : ''}>
                          Несколько правильных
                        </option>
                      </select>
                    </div>
                  </div>
                  <p class="answer-hint">Отметьте правильные варианты слева</p>
                  <div class="answer-list">
                    ${q.answers
                      .map(
                        (a, j) =>
                          html`<div class="answer-editor">
                            <div class="answer-line">
                              <input
                                class="correct-choice"
                                type="${q.type === 'single' ? 'radio' : 'checkbox'}"
                                name="correct-${i}"
                                data-test-field="is_correct"
                                data-q="${i}"
                                data-a="${j}"
                                ${a.is_correct ? 'checked' : ''}
                                aria-label="Правильный ответ ${i + 1}.${j + 1}"
                              /><input
                                aria-label="Ответ ${i + 1}.${j + 1}"
                                data-test-field="text"
                                data-q="${i}"
                                data-a="${j}"
                                value="${escape(a.text)}"
                                maxlength="5000"
                                placeholder="Вариант ${j + 1}"
                              />${q.answers.length > 2 ? html`<button type="button" class="text-button" data-action="test-remove-answer" data-q="${i}" data-a="${j}" aria-label="Удалить ответ ${i + 1}.${j + 1}">×</button>` : ''}
                            </div>
                            ${imageField(a, i, j)}
                          </div>`,
                      )
                      .join('')}
                  </div>
                  ${fieldError(`questions[${i}].answers`)}<button
                    type="button"
                    class="secondary"
                    data-action="test-add-answer"
                    data-q="${i}"
                    ${q.answers.length >= 20 ? 'disabled' : ''}
                  >
                    ＋ Добавить вариант
                  </button>
                </section>`,
            )
            .join('')}
        </div>
        <button
          type="button"
          class="secondary add-question"
          data-action="test-add-question"
          ${canAdd ? '' : 'disabled'}
        >
          ＋ Добавить вопрос
        </button>
        <p class="hint" id="next-question-hint">
          ${canAdd ? 'Можно добавить следующий вопрос.' : 'Заполните вопросы, минимум два ответа и отметьте правильные варианты.'}
        </p>
        ${
          published
            ? ''
            : html`<div class="quiz-save-bar">
                  <span class="hint" data-test-dirty
                    >${state.testDirty ? 'Есть несохранённые изменения' : 'Изменения сохранены'}</span
                  ><button type="button" class="secondary" data-action="test-save-draft">
                    Сохранить черновик</button
                  ><button type="submit" class="primary">Опубликовать public</button
                  ><button type="button" class="secondary" data-action="test-publish-private">
                    Опубликовать private
                  </button>
                </div>
                <p class="hint">
                  После публикации изменить тест или вернуть его в черновик нельзя. Public виден в
                  списке своего вуза; private доступен по ссылке.
                </p>`
        }
      </fieldset>
    </form>
    <p class="hint">
      Картинки: PNG, JPEG, WebP, до 5 МиБ. ${state.testUploading ? 'Картинка загружается…' : ''}
    </p>`;
}
