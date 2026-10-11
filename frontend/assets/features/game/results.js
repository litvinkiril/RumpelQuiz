import { html, escape } from '../../shared/html.js';
export function renderResults(results, questionCount, { userId = '', status = 'finished' } = {}) {
  if (!results.length)
    return '<div class="results-empty"><span aria-hidden="true">◇</span><p>В этой сессии нет участников.</p></div>';
  const cancelled = status === 'cancelled';
  const count = Number.isFinite(questionCount) ? questionCount : results[0].question_count || 0;
  const best = Math.max(...results.map((r) => Number(r.score) || 0));
  const place = (r) => (!cancelled && Number.isInteger(r.rank) && r.rank > 0 ? r.rank : null);
  const groups = [1, 2, 3]
    .map((rank) => ({ rank, people: results.filter((r) => place(r) === rank && r.score > 0) }))
    .filter((g) => g.people.length);
  const tied =
    new Set(results.map(place).filter(Boolean)).size <
    results.filter((r) => place(r) !== null).length;
  const initials = (name) =>
    String(name || 'Участник')
      .trim()
      .split(/\s+/)
      .slice(0, 2)
      .map((n) => [...n][0] || '')
      .join('')
      .toLocaleUpperCase('ru-RU');
  const percent = (r) =>
    Math.min(
      100,
      Math.max(
        0,
        Math.round(((Number(r.correct_count) || 0) / Math.max(1, r.question_count ?? count)) * 100),
      ),
    );
  return html`<div class="results-board">
    <div class="results-overview">
      <div><span>Участников</span><strong>${results.length}</strong></div>
      <div><span>Вопросов</span><strong>${escape(count)}</strong></div>
      ${
        !cancelled
          ? html`<div>
              <span>Лучший результат</span
              ><strong>${escape(best)}<small> / ${escape(count)}</small></strong>
            </div>`
          : ''
      }
    </div>
    ${
      groups.length
        ? html`<section class="results-leaders" aria-label="Призовые места">
            ${groups
              .map(
                (g) =>
                  html`<article class="result-leader place-${g.rank}">
                    <span class="leader-place"
                      ><span aria-hidden="true">${g.rank === 1 ? '★' : '✦'}</span> ${g.rank}
                      место</span
                    >
                    <div class="leader-names">
                      ${g.people
                        .slice(0, 3)
                        .map((p) => html`<strong>${escape(p.name)}</strong>`)
                        .join(
                          '',
                        )}${g.people.length > 3 ? html`<span class="leader-more">Ещё участников: ${g.people.length - 3} · все в таблице ниже</span>` : ''}
                    </div>
                    <div class="leader-score">
                      ${escape(g.people[0].score)} <span>из ${escape(count)} баллов</span>
                    </div>
                    ${g.people.length > 1 ? '<span class="leader-tie">Разделили место</span>' : ''}
                  </article>`,
              )
              .join('')}
          </section>`
        : ''
    }
    <div class="results-table-heading">
      <h3>${cancelled ? 'Участники сессии' : 'Итоговый рейтинг'}</h3>
      <p>
        ${cancelled ? 'Игра не началась — места не присуждаются.' : tied ? 'При равных баллах участники делят место.' : '1 балл за полностью правильный ответ.'}
      </p>
    </div>
    <div class="results-scroll">
      <table class="game-results ranked-results">
        <caption class="results-sr-only">
          ${cancelled ? 'Участники отменённой сессии' : 'Рейтинг участников по количеству баллов'}
        </caption>
        <thead>
          <tr>
            <th scope="col">Место</th>
            <th scope="col">Участник</th>
            <th scope="col">Ответов</th>
            <th scope="col">Правильных</th>
            <th scope="col">Баллы</th>
          </tr>
        </thead>
        <tbody>
          ${results
            .map((r) => {
              const rank = place(r),
                mine = !!userId && r.user_id === userId;
              return html`<tr class="${mine ? 'result-self' : ''}">
                <td>
                  <span
                    class="result-rank ${rank && rank <= 3 && r.score > 0 ? 'rank-' + rank : ''}"
                    aria-label="${rank ? rank + ' место' : 'Место не присуждено'}"
                    >${rank ?? '—'}</span
                  >
                </td>
                <td>
                  <div class="result-person">
                    <span class="result-avatar" aria-hidden="true">${escape(initials(r.name))}</span
                    ><span
                      >${escape(r.name)}${mine ? '<small class="result-you">Вы</small>' : ''}</span
                    >
                  </div>
                </td>
                <td data-label="Ответов">
                  ${escape(r.answered_count)} / ${escape(r.question_count ?? count)}
                </td>
                <td data-label="Правильных">
                  <span class="result-correct"
                    >${escape(r.correct_count)}<small>${percent(r)}%</small></span
                  ><progress
                    class="result-meter"
                    max="100"
                    value="${percent(r)}"
                    aria-label="Правильных ответов, %"
                  >
                    ${percent(r)}%
                  </progress>
                </td>
                <td>
                  <strong class="result-score">${escape(r.score)}</strong
                  ><span class="result-score-label">баллы</span>
                </td>
              </tr>`;
            })
            .join('')}
        </tbody>
      </table>
    </div>
  </div>`;
}
