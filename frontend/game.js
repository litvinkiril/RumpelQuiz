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
export async function consumeGameStream(response, onEvent) {
  if(!response.body?.getReader) throw new Error('Streaming response unavailable');
  const reader=response.body.getReader(), decoder=new TextDecoder();
  let buffer='';
  try {
    while(true) {
      const {value,done}=await reader.read();
      if(done) break;
      buffer+=decoder.decode(value,{stream:true});
      let end;
      while((end=buffer.indexOf('\n\n'))!==-1) {
        const block=buffer.slice(0,end);buffer=buffer.slice(end+2);
        let type='message';const data=[];
        for(const line of block.split('\n')) {
          if(line.startsWith('event:')) type=line.slice(6).trimStart();
          if(line.startsWith('data:')) data.push(line.slice(5).trimStart());
        }
        if(data.length && await onEvent(type,JSON.parse(data.join('\n')))===false) {
          await reader.cancel();
          return;
        }
      }
      if(buffer.length>1048576) throw new Error('Game event too large');
    }
  } finally { reader.releaseLock(); }
}
export function createGameController({state, authorized, run, emit, fail, storage, streamFetch=globalThis.fetch, now=Date.now, makeId=makeGameId, origin='http://localhost:8080', joinCode=''}) {
  let polling=false;
  let streamAbort=null,streamSession='',streamTask=null;
  let saved={};
  try { const value=JSON.parse(storage?.getItem(KEY) || '{}'); if(value && typeof value==='object') saved=value; } catch {}
  Object.assign(state,{game:null,gameSelected:[],gameOffset:0,gameOrigin:origin,gameJoinCode:/^\d{6}$/.test(joinCode)?joinCode:''});
  const remember = patch => {
    saved={...saved,...patch,owner:state.userId};
    try { storage?.setItem(KEY,JSON.stringify(saved)); } catch {}
  };
  const validate = value => {
    if(!value?.success || !UUID.test(value.session?.id || '') || !['waiting','running','finished','cancelled'].includes(value.session.status) || !Number.isFinite(value.server_time_ms))
      throw new Error('Invalid game state');
  };
  const accept = value => {
    validate(value);
    const previous=state.game?.current_question?.id;
    state.game=value; state.gameOffset=value.server_time_ms-now();
    const question=value.current_question;
    if(question?.submitted) state.gameSelected=[...question.selected_ids];
    else if(previous!==question?.id) state.gameSelected=[];
    state.screen='game';
    remember({sessionId:value.session.id,code:value.session.join_code,pending:null});
  };
  const acceptBackground = value => {
    validate(value);
    const previous=state.game;
    // Signed image URLs can change on every read. Only progress may change
    // without replacing the question; submission/deadline changes still render.
    const content = game => {
      const {server_time_ms,participants,session,current_question,...rest}=game;
      const {participants_count,...sessionContent}=session;
      let questionContent=null;
      if(current_question) {
        const {answered_count,image_url,answers,...question}=current_question;
        questionContent={...question,answers:answers.map(({image_url,...answer})=>answer)};
      }
      return JSON.stringify({...rest,session:sessionContent,current_question:questionContent});
    };
    if(previous && !finished(value) && content(previous)===content(value)) {
      const changed=previous.session.participants_count!==value.session.participants_count
        || previous.current_question?.answered_count!==value.current_question?.answered_count
        || JSON.stringify(previous.participants)!==JSON.stringify(value.participants);
      state.game={...previous,server_time_ms:value.server_time_ms,
        session:{...previous.session,participants_count:value.session.participants_count},
        participants:value.participants,
        current_question:previous.current_question?{...previous.current_question,answered_count:value.current_question.answered_count}:null};
      state.gameOffset=value.server_time_ms-now();
      return changed?'game-progress':false;
    }
    accept(value);
  };
  const get = id => authorized('game/sessions/'+encodeURIComponent(id));
  const load = async id => {
    const value=await get(id);
    return value;
  };
  const controller = {
    resetGameView() { state.game=null; state.gameSelected=[]; controller.syncGameEvents(); },
    syncGameEvents() {
      const id=state.screen==='game' && state.token && state.game && !finished(state.game)?state.game.session.id:'';
      if(id===streamSession && streamTask) return;
      streamAbort?.abort();streamAbort=null;streamTask=null;streamSession=id;
      if(!id) return;
      const abort=new AbortController();streamAbort=abort;
      const active=()=>!abort.signal.aborted && state.screen==='game' && state.token && state.game?.session.id===id && !finished(state.game);
      const task=(async()=>{
        while(active()) {
          try {
            const response=await streamFetch('/v1/game/sessions/'+encodeURIComponent(id)+'/events',{
              headers:{Authorization:'Bearer '+state.token,Accept:'text/event-stream'},signal:abort.signal,
            });
            if(response.status===401) {
              try { await authorized('me'); }
              catch {
                state.token='';state.refreshToken='';state.sessionId='';state.userId='';
                state.game=null;state.screen='login';
                state.error='Сессия входа истекла. Войдите снова.';emit();abort.abort();break;
              }
              continue;
            }
            if(response.status===403) {
              state.error='Нет доступа к этой игровой сессии.';emit();abort.abort();break;
            }
            if(!response.ok) throw new Error('Game event stream failed');
            await consumeGameStream(response,async(type,value)=>{
              while(active() && state.busy) await new Promise(resolve=>setTimeout(resolve,50));
              if(!active()) return;
              if(type==='snapshot' || type==='results') {
                if(!value?.success || value.session?.id!==id || !Number.isFinite(value.server_time_ms)) return;
                if(type==='results' && (!finished(value) || !Array.isArray(value.results))) return;
                if(state.game?.server_time_ms>value.server_time_ms) return;
                const change=acceptBackground(value),hadError=!!state.error;
                state.error='';
                if(hadError || change!==false) emit(hadError?undefined:change);
                if(finished(value)) return false;
              } else if(type==='question') {
                const q=value?.current_question,old=state.game?.current_question;
                if(!q || !Number.isInteger(q.position) || !Number.isFinite(value.server_time_ms)) return;
                if(old && q.position<=old.position) return;
                accept({...state.game,server_time_ms:value.server_time_ms,
                  session:{...state.game.session,status:'running'},current_question:q});
                state.error='';emit();
              } else if(type==='resync') {
                await controller.pollGame();
              }
            });
          } catch(error) {
            if(!active()) break;
          }
          if(active()) await new Promise(resolve=>setTimeout(resolve,1000));
        }
      })();
      streamTask=task;
      void task.finally(()=>{if(streamAbort===abort) {streamAbort=null;streamTask=null;streamSession='';}});
    },
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
    createGame(name) {
      if(state.busy || !state.token || state.quizDraft?.status!=='ready') return Promise.resolve(false);
      name=typeof name==='string'?name.replace(/^[\p{White_Space}\uFEFF]+|[\p{White_Space}\uFEFF]+$/gu,''):'';
      if(!name || [...name].length>200 || name.includes('\0')) return fail('Введите название сессии от 1 до 200 символов.');
      const quiz=state.quizDraft;
      return run(async()=>{
        if(saved.owner!==state.userId || saved.pending?.quiz_id!==quiz.id || saved.pending?.name!==name) {
          remember({sessionId:null,code:null,pending:{quiz_id:quiz.id,session_id:makeId(),name}});
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
      if(polling || state.screen!=='game' || state.busy || !state.game) return false;
      polling=true;
      try {
        return await run(()=>load(state.game.session.id),acceptBackground,{background:true});
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
          return {saved:true};
        } catch(error) {
          if(!['answer_already_saved','question_closed','session_closed'].includes(error.code)) throw error;
          return {state:await load(game.session.id)};
        }
      },result=>{
        if(result.saved && state.game?.current_question?.id===q.id) {
          state.game={...state.game,current_question:{...state.game.current_question,
            submitted:true,selected_ids:choices}};
          state.gameSelected=choices;
        } else if(result.state) accept(result.state);
      });
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
const renderParticipants = participants => participants.map(p=>`<span>${escape(p.name)}</span>`).join('') || '<p class="hint">Участники появятся здесь после подключения.</p>';
export function renderResults(results, questionCount) {
  return results.length?`<div class="results-scroll"><table class="game-results"><thead><tr><th>Участник</th><th>Ответов</th><th>Правильных</th><th>Баллы</th></tr></thead><tbody>${results.map(r=>`<tr><td>${escape(r.name)}</td><td>${escape(r.answered_count)} / ${escape(r.question_count ?? questionCount)}</td><td>${escape(r.correct_count)}</td><td>${escape(r.score)}</td></tr>`).join('')}</tbody></table></div>`:'<p class="empty-list">В этой сессии нет участников.</p>';
}
export function updateGameProgress(root, game, previous) {
  const answered=root.querySelector('[data-game-answered]');
  if(answered) answered.textContent=`Ответили ${game.current_question.answered_count} из ${game.session.participants_count}`;
  const count=root.querySelector('[data-game-participants-count]');
  if(count) count.textContent=`Участники · ${game.session.participants_count}`;
  if(JSON.stringify(previous.participants)!==JSON.stringify(game.participants)) {
    const list=root.querySelector('.game-people-list');
    if(list) list.innerHTML=renderParticipants(game.participants);
  }
}
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
    ${!ended && q?`<section class="game-question"><div class="game-question-top"><span ${s.is_host?'data-game-answered':''}>${s.is_host?`Ответили ${q.answered_count} из ${s.participants_count}`:'Выберите ответ'}</span><strong data-game-timer role="timer">${seconds>0?seconds+' с':'Время истекло'}</strong></div>
      <h3>${escape(q.text)}</h3>${gameImage(q.image_url)}
      <div class="game-answers">${q.answers.map((a,i)=>`<label class="game-answer ${state.gameSelected.includes(a.id)?'selected':''}"><span class="game-answer-index">${i+1}</span>
      ${!s.is_host?`<input type="${q.type==='single'?'radio':'checkbox'}" name="game-answer" data-game-answer="${escape(a.id)}" ${state.gameSelected.includes(a.id)?'checked':''} ${q.submitted || seconds===0 || state.busy?'disabled':''}>`:''}
      <span>${escape(a.text)}${gameImage(a.image_url)}</span></label>`).join('')}</div>
      ${!s.is_host?`<p class="hint" data-game-answer-status>${q.submitted?'Ответ сохранён. Ждём следующий вопрос.':seconds===0?'Приём ответов завершён.':q.type==='multy'?'Можно выбрать несколько вариантов. После отправки ответ нельзя изменить.':'После отправки ответ нельзя изменить.'}</p>
      <button class="primary" data-action="game-submit" ${q.submitted || seconds===0 || !state.gameSelected.length || state.busy?'disabled':''}>${q.submitted?'Ответ сохранён':'Отправить ответ'}</button>`:''}</section>`:''}
    ${!q && !ended && !s.is_host?'<div class="game-wait"><span aria-hidden="true">✦</span><h3>Вы в игре</h3><p>Преподаватель скоро откроет первый вопрос.</p></div>':''}
    ${s.is_host && !ended?`<section class="game-people"><h3 data-game-participants-count>Участники · ${s.participants_count}</h3><div class="game-people-list">${renderParticipants(game.participants)}</div></section>
      <div class="game-controls"><button class="primary" data-action="game-next" ${state.busy || q?.has_next===false?'disabled':''}>Следующий вопрос</button><button class="secondary game-close" data-action="game-close" ${state.busy?'disabled':''}>Закрыть сессию</button></div>
      <p class="hint">${q?.has_next===false?'Это последний вопрос. Закройте сессию, когда будете готовы.':!q?'«Следующий вопрос» запустит первый вопрос.':'Переход завершает приём ответов на текущий вопрос.'}</p>`:''}
    ${ended?`<section class="game-finished"><div class="game-finished-icon" aria-hidden="true">✓</div><h3>${s.status==='cancelled'?'Игра не была начата':'Результаты участников'}</h3><p class="subtitle">Участников: ${s.participants_count}. 1 балл за полностью правильный ответ.</p>
      ${renderResults(game.results,s.question_count)}
      <button class="secondary" data-nav="account">На главную</button></section>`:''}`;
}
