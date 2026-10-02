import qrcode from './vendor/qrcode.js';

const escape = value => String(value ?? '').replace(/[&<>"']/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
const KEY = 'rumpelquiz.game';
const UUID = /^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/i;
const finished = game => ['finished','cancelled'].includes(game?.session.status);
export function makeGameId() {
  const bytes = globalThis.crypto.getRandomValues(new Uint8Array(16));
  bytes[6] = (bytes[6] & 15) | 64; bytes[8] = (bytes[8] & 63) | 128;
  const h = [...bytes].map(b=>b.toString(16).padStart(2,'0')).join('');
  return `${h.slice(0,8)}-${h.slice(8,12)}-${h.slice(12,16)}-${h.slice(16,20)}-${h.slice(20)}`;
}
export function joinLink(origin, code) {
  const url = new URL(origin);
  if (!['http:','https:'].includes(url.protocol) || url.username || url.password || !/^\d{6}$/.test(code))
    throw new Error('invalid_join_url');
  return url.origin + '/?join=' + code;
}
const qrCache = new Map();
export function qrSvg(url) {
  if (qrCache.has(url)) return qrCache.get(url);
  const qr = qrcode(0, 'M'); qr.addData(url); qr.make();
  const svg = qr.createSvgTag({cellSize:4, margin:16, scalable:true});
  if (qrCache.size>5) qrCache.clear();
  qrCache.set(url,svg); return svg;
}
export function gameSeconds(state, now=Date.now()) {
  const q=state.game?.current_question;
  return q && q.accepting_answers ? Math.max(0,Math.ceil((q.deadline_at_ms-now-(state.gameOffset || 0))/1000)) : 0;
}
export function createGameController({state, authorized, run, emit, fail, storage, now=Date.now, makeId=makeGameId, origin='http://localhost:8080', joinCode=''}) {
  let polling=false;
  let saved={};
  try { const value=JSON.parse(storage?.getItem(KEY) || '{}'); if(value && typeof value==='object') saved=value; } catch {}
  Object.assign(state,{game:null,gameSelected:[],gameOffset:0,gameOrigin:origin,gameJoinCode:/^\d{6}$/.test(joinCode)?joinCode:''});
  const remember = patch => {
    saved={...saved,...patch,owner:state.userId};
    try { storage?.setItem(KEY,JSON.stringify(saved)); } catch {}
  };
  const accept = value => {
    if(!value?.success || !UUID.test(value.session?.id || '') || !['waiting','running','finished','cancelled'].includes(value.session.status) || !Number.isFinite(value.server_time_ms))
      throw new Error('Invalid game state');
    const previous=state.game?.current_question?.id;
    state.game=value; state.gameOffset=value.server_time_ms-now();
    const question=value.current_question;
    if(question?.submitted) state.gameSelected=[...question.selected_ids];
    else if(previous!==question?.id) state.gameSelected=[];
    state.screen='game';
    remember({sessionId:value.session.id,code:value.session.join_code,pending:null});
  };
  const get = id => authorized('game/sessions/'+encodeURIComponent(id));
  const load = async id => {
    const value=await get(id);
    return value;
  };
  const controller = {
    resetGameView() { state.game=null; state.gameSelected=[]; },
    async resumeGame() {
      if(!state.token || state.busy) return false;
      if(state.gameJoinCode && (saved.owner!==state.userId || saved.code!==state.gameJoinCode)) {
        state.screen='game-join'; emit(); return true;
      }
      if(saved.owner===state.userId && UUID.test(saved.sessionId || '')) {
        state.screen='game'; emit();
        return run(()=>load(saved.sessionId),accept);
      }
      return true;
    },
    openGameJoin() { if(state.busy || !state.token) return; state.screen='game-join'; state.error=''; state.success=''; emit(); },
    openLastGame() { return controller.resumeGame(); },
    createGame() {
      if(state.busy || !state.token || state.quizDraft?.status!=='ready') return Promise.resolve(false);
      const quiz=state.quizDraft;
      return run(async()=>{
        if(saved.owner!==state.userId || saved.pending?.quiz_id!==quiz.id) {
          remember({sessionId:null,code:null,pending:{quiz_id:quiz.id,session_id:makeId()}});
        }
        const result=await authorized('game/sessions',saved.pending);
        if(!result?.success || !UUID.test(result.session?.id || '')) throw new Error('Invalid game creation');
        remember({sessionId:result.session.id,code:result.session.join_code});
        return load(result.session.id);
      },accept);
    },
    joinGame(code) {
      if(state.busy || !state.token) return Promise.resolve(false);
      code=code.trim();
      if(!/^\d{6}$/.test(code)) return fail('Введите шестизначный код сессии.');
      state.gameJoinCode=code;
      return run(async()=>{
        const result=await authorized('game/sessions/join',{code});
        if(!result?.success || !UUID.test(result.session_id || '')) throw new Error('Invalid join');
        remember({sessionId:result.session_id,code,pending:null});
        return load(result.session_id);
      },accept);
    },
    refreshGame() {
      if(state.busy || !state.token || state.screen!=='game') return Promise.resolve(false);
      const id=state.game?.session.id || (saved.owner===state.userId?saved.sessionId:null);
      if(!UUID.test(id || '')) return Promise.resolve(false);
      return run(()=>load(id),accept);
    },
    async pollGame() {
      if(polling || state.screen!=='game' || state.busy || state.error || !state.game || finished(state.game)) return false;
      polling=true;
      try {
        return await run(()=>load(state.game.session.id),value=>{
          const {server_time_ms:previousTime,...previous}=state.game;
          const {server_time_ms:nextTime,...next}=value;
          if(Number.isFinite(nextTime) && JSON.stringify(previous)===JSON.stringify(next)) {
            state.gameOffset=nextTime-now();
            return false;
          }
          accept(value);
        },{background:true});
      } finally { polling=false; }
    },
    nextGameQuestion() {
      const game=state.game;
      if(state.busy || !state.token || !game?.session.is_host || finished(game) || game.current_question?.has_next===false) return Promise.resolve(false);
      return run(async()=>{
        try {
          await authorized('game/sessions/'+game.session.id+'/next',{expected_question_id:game.current_question?.id || null});
        } catch(error) {
          if(!['session_state_changed','session_closed','no_more_questions'].includes(error.code)) throw error;
        }
        return load(game.session.id);
      },accept);
    },
    closeGame() {
      const game=state.game;
      if(state.busy || !state.token || !game?.session.is_host || finished(game)) return Promise.resolve(false);
      return run(async()=>{
        await authorized('game/sessions/'+game.session.id+'/close',{});
        return load(game.session.id);
      },accept);
    },
    chooseGameAnswer(id, checked=true) {
      const q=state.game?.current_question;
      if(state.busy || state.game?.session.is_host || !q || q.submitted || gameSeconds(state,now())===0 || !q.answers.some(a=>a.id===id)) return;
      state.gameSelected=q.type==='single'?[id]:checked?[...new Set([...state.gameSelected,id])]:state.gameSelected.filter(a=>a!==id);
      emit();
    },
    submitGameAnswer() {
      const game=state.game,q=game?.current_question;
      if(state.busy || !state.token || game?.session.is_host || !q || q.submitted || !state.gameSelected.length || gameSeconds(state,now())===0) return Promise.resolve(false);
      const choices=[...state.gameSelected];
      return run(async()=>{
        try {
          await authorized('game/sessions/'+game.session.id+'/answers',{question_id:q.id,answer_ids:choices});
        } catch(error) {
          if(!['answer_already_saved','question_closed','session_closed'].includes(error.code)) throw error;
        }
        return load(game.session.id);
      },accept);
    },
    setGameOrigin(value) {
      try {
        joinLink(value,state.game?.session.join_code);
        state.gameOrigin=new URL(value).origin; state.error=''; emit();
      } catch { return fail('Укажите адрес сайта с http:// или https://, без логина и пароля.'); }
    },
  };
  return controller;
}
const gameImage=url=>/^https:\/\//.test(url || '')?`<img class="game-image" src="${escape(url)}" alt="Изображение к вопросу или варианту" referrerpolicy="no-referrer">`:'';
export function renderGame(state,notice) {
  if(state.screen==='game-join') return `<button class="text-button back" data-nav="account">← На главную</button>
    <p class="step-label">Присоединиться к игре</p><h2 tabindex="-1">Введите код сессии</h2>
    <p class="subtitle">Код из шести цифр находится на экране преподавателя.</p>${notice}
    <form data-form="game-join"><div class="field"><label for="game-code">Код сессии</label>
    <input id="game-code" name="game-code" class="game-code-input" inputmode="numeric" pattern="[0-9]{6}" maxlength="6" required value="${escape(state.gameJoinCode)}" placeholder="000000"></div>
    <button class="primary" type="submit" ${state.busy?'disabled':''}>Присоединиться</button></form>`;
  const game=state.game;
  if(!game) return `<h2 tabindex="-1">Восстанавливаем сессию</h2>${notice}<button class="secondary" data-action="game-refresh">Повторить загрузку</button><button class="text-button" data-nav="account">На главную</button>`;
  const s=game.session,q=game.current_question,ended=finished(game),seconds=gameSeconds(state);
  let link='',qr='';
  try { link=joinLink(state.gameOrigin,s.join_code); qr=qrSvg(link); } catch {}
  const title=ended?(s.status==='cancelled'?'Сессия закрыта':'Квиз завершён'):q?('Вопрос '+(q.position+1)+' из '+s.question_count):'Все готовы?';
  return `<button class="text-button back" data-nav="account">← На главную</button>
    <div class="game-heading"><div><p class="step-label">${s.is_host?'Экран ведущего':'Участник'} · ${escape(s.name)}</p><h2 tabindex="-1">${title}</h2></div>
    <span class="quiz-badge ${ended?'':'ready'}">${ended?'Завершено':q?'Игра идёт':'Ожидание'}</span></div>${notice}
    ${state.error?'<button class="secondary" data-action="game-refresh">Обновить состояние</button>':''}
    ${s.is_host && !ended?`<section class="game-lobby"><div><p class="game-label">Код для подключения</p><div class="game-code" aria-label="Код сессии">${escape(s.join_code)}</div>
      <p class="hint">Откройте ссылку или отсканируйте QR-код.</p><a class="game-link" href="${escape(link)}">${escape(link)}</a>
      <details class="game-network"><summary>Адрес для участников</summary><form data-form="game-origin"><label for="game-origin">Адрес сайта</label><input id="game-origin" name="game-origin" type="url" value="${escape(state.gameOrigin)}" required><button type="submit" class="secondary">Обновить QR</button></form>
      <p class="hint">Для телефона в одной сети укажите адрес компьютера, например http://192.168.1.50:8080.</p></details>
      ${/^https?:\/\/(localhost|127\.0\.0\.1|\[::1\])(?=:|\/|$)/.test(link)?'<p class="hint">Сейчас ссылка работает только на этом компьютере. Для других устройств измените адрес выше.</p>':''}</div>
      <div class="game-qr" role="img" aria-label="QR-код подключения к сессии">${qr}</div></section>`:''}
    ${!ended && q?`<section class="game-question"><div class="game-question-top"><span>${s.is_host?`Ответили ${q.answered_count} из ${s.participants_count}`:'Выберите ответ'}</span><strong data-game-timer role="timer">${seconds>0?seconds+' с':'Время истекло'}</strong></div>
      <h3>${escape(q.text)}</h3>${gameImage(q.image_url)}
      <div class="game-answers">${q.answers.map((a,i)=>`<label class="game-answer ${state.gameSelected.includes(a.id)?'selected':''}"><span class="game-answer-index">${i+1}</span>
      ${!s.is_host?`<input type="${q.type==='single'?'radio':'checkbox'}" name="game-answer" data-game-answer="${escape(a.id)}" ${state.gameSelected.includes(a.id)?'checked':''} ${q.submitted || seconds===0 || state.busy?'disabled':''}>`:''}
      <span>${escape(a.text)}${gameImage(a.image_url)}</span></label>`).join('')}</div>
      ${!s.is_host?`<p class="hint" data-game-answer-status>${q.submitted?'Ответ сохранён. Ждём следующий вопрос.':seconds===0?'Приём ответов завершён.':q.type==='multy'?'Можно выбрать несколько вариантов. После отправки ответ нельзя изменить.':'После отправки ответ нельзя изменить.'}</p>
      <button class="primary" data-action="game-submit" ${q.submitted || seconds===0 || !state.gameSelected.length || state.busy?'disabled':''}>${q.submitted?'Ответ сохранён':'Отправить ответ'}</button>`:''}</section>`:''}
    ${!q && !ended && !s.is_host?'<div class="game-wait"><span aria-hidden="true">✦</span><h3>Вы в игре</h3><p>Преподаватель скоро откроет первый вопрос.</p></div>':''}
    ${s.is_host && !ended?`<section class="game-people"><h3>Участники · ${s.participants_count}</h3><div class="game-people-list">${game.participants.map(p=>`<span>${escape(p.name)}</span>`).join('') || '<p class="hint">Участники появятся здесь после подключения.</p>'}</div></section>
      <div class="game-controls"><button class="primary" data-action="game-next" ${state.busy || q?.has_next===false?'disabled':''}>Следующий вопрос</button><button class="secondary game-close" data-action="game-close" ${state.busy?'disabled':''}>Закрыть сессию</button></div>
      <p class="hint">${q?.has_next===false?'Это последний вопрос. Закройте сессию, когда будете готовы.':!q?'«Следующий вопрос» запустит первый вопрос.':'Переход завершает приём ответов на текущий вопрос.'}</p>`:''}
    ${ended?`<section class="game-finished"><div class="game-finished-icon" aria-hidden="true">✓</div><h3>${s.status==='cancelled'?'Игра не была начата':'Ответы сохранены'}</h3><p class="subtitle">Участников: ${s.participants_count}. ${s.status==='finished'?'Показано число отправленных ответов, без расчёта баллов.':''}</p>
      ${game.results.length?`<table class="game-results"><thead><tr><th>Участник</th><th>Ответов</th></tr></thead><tbody>${game.results.map(r=>`<tr><td>${escape(r.name)}</td><td>${r.answered_count} / ${s.question_count}</td></tr>`).join('')}</tbody></table>`:''}
      <button class="secondary" data-nav="account">На главную</button></section>`:''}`;
}
