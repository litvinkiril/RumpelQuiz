import {newQuiz, newQuestion, newAnswer, questionComplete, quizPayload, renderQuiz} from './quiz.js';
import {createGameController, renderGame, gameSeconds, updateGameProgress} from './game.js';
export const messages = {
  session_not_found: 'Сессия не найдена или нет доступа к её результатам.',
  session_not_finished: 'Сессия ещё не завершена. Результаты появятся после её закрытия.',
  invalid_session_id: 'Некорректный идентификатор сессии.',
  game_access_denied: 'Нет доступа к этой сессии. Проверьте аккаунт и роль в вузе.',
  game_university_mismatch: 'Подключиться может студент того же вуза, в котором проводится квиз.',
  game_not_found: 'Активная сессия с таким кодом не найдена.',
  quiz_not_ready: 'Сначала опубликуйте квиз.',
  request_id_conflict: 'Не удалось восстановить создание сессии. Обновите страницу.',
  join_code_unavailable: 'Не удалось получить свободный код. Попробуйте ещё раз.',
  session_closed: 'Сессия уже закрыта.',
  session_state_changed: 'Вопрос уже сменился. Обновите состояние игры.',
  no_more_questions: 'Это последний вопрос. Можно закрыть сессию.',
  question_closed: 'Приём ответов на этот вопрос завершён.',
  answer_already_saved: 'Ваш ответ уже сохранён.',
  quiz_access_denied: 'Создавать квизы могут преподаватели и администраторы выбранного вуза.',
  quiz_not_found: 'Квиз не найден или доступ к нему изменился.',
  quiz_revision_conflict: 'Квиз изменён в другой вкладке. Скопируйте свои изменения и заново откройте квиз из списка.',
  quiz_published: 'Квиз опубликован. Редактирование и возврат в черновик недоступны.',
  quiz_validation_failed: 'Проверьте отмеченные поля. Незавершённый квиз можно сохранить как черновик.',
  image_storage_unavailable: 'Хранилище картинок недоступно. Попробуйте загрузить файл ещё раз.',
  invalid_image: 'Не удалось прочитать картинку. Выберите другой файл.',
  unsupported_image_format: 'Поддерживаются PNG, JPEG и WebP.',
  image_dimensions_exceeded: 'Картинка слишком большая: до 8192 пикселей по стороне и 12 мегапикселей.',
  upload_access_denied: 'Загрузка доступна преподавателям и администраторам.',
  invalid_search_params: 'Введите от 2 до 100 символов имени, фамилии или отчества.',
  university_access_denied: 'Нет доступа к этому вузу. Возможно, права администратора изменились.',
  invalid_university_id: 'Не удалось определить вуз. Вернитесь в профиль и попробуйте снова.',
  user_not_found: 'Пользователь удален',
  user_profile_not_found: 'Пользователь удален',
  invalid_credentials: 'Проверьте почту и пароль. Подтвердите почту, если ещё не сделали этого. После пяти ошибок вход временно ограничен: повторите через 15 минут.',
  email_already_exists: 'Эта почта уже зарегистрирована. Войдите или восстановите пароль.',
  passwords_do_not_match: 'Пароли не совпадают.',
  invalid_password: 'Пароль должен содержать от 1 до 72 байт и не содержать нулевых символов.',
  invalid_request: 'Проверьте заполненные поля.',
  invalid_or_expired_code: 'Код неверный, истёк или уже использован. Проверьте его или запросите новый.',
  invalid_or_expired_token: 'Время на смену пароля истекло. Запросите новый код.',
  email_not_found: 'Аккаунт с такой почтой не найден.',
  email_not_verified: 'Сначала подтвердите почту: вернитесь к регистрации.',
  verification_not_found: 'Этот запрос уже недействителен. Начните регистрацию заново.',
  resend_too_soon: 'Повторный код можно запросить через минуту.',
};
export class ApiError extends Error {
  constructor(message, status = 0, code = '', details = []) { super(message); this.status = status; this.code = code; this.details = details; }
}
export function createApi(fetcher = globalThis.fetch) {
  return async (path, data, token, method) => {
    const abort = new AbortController();
    const timeout = setTimeout(() => abort.abort(), 15000);
    try {
      const url = path === 'profile' ? '/v1/user/profile'
        : /^(education\/|quizzes(?:\/|$)|media\/|game\/)/.test(path) ? '/v1/' + path : '/v1/auth/' + path;
      const multipart = typeof FormData !== 'undefined' && data instanceof FormData;
      const response = await fetcher(url, {
        method: method || (data === undefined ? 'GET' : 'POST'),
        headers: { ...(data === undefined || multipart ? {} : {'Content-Type': 'application/json'}),
          ...(token ? {Authorization: 'Bearer ' + token} : {}) },
        ...(data === undefined ? {} : {body: multipart ? data : JSON.stringify(data)}),
        signal: abort.signal,
      });
      const body = await response.json().catch(() => ({}));
      if (!response.ok) {
        throw new ApiError(messages[body.error] || (response.status >= 500
          ? 'Сервис временно недоступен. Попробуйте ещё раз.'
          : 'Не удалось выполнить запрос. Проверьте данные.'), response.status, body.error, body.details || []);
      }
      return body;
    } catch (error) {
      if (error instanceof ApiError) throw error;
      throw new ApiError(error.name === 'AbortError'
        ? 'Сервер не ответил вовремя. Попробуйте ещё раз.'
        : 'Не удалось связаться с сервером. Проверьте подключение.');
    } finally { clearTimeout(timeout); }
  };
}
export function validatePassword(password, confirmation) {
  if (!password || new TextEncoder().encode(password).length > 72 || password.includes('\0'))
    return messages.invalid_password;
  if (confirmation !== undefined && password !== confirmation) return messages.passwords_do_not_match;
  return '';
}
const KEY = 'rumpelquiz.auth';
export function createController({api = createApi(), streamFetch = globalThis.fetch, storage, onChange = () => {}, now = Date.now, makeGameId, origin = globalThis.location?.origin || 'http://localhost:8080', joinCode = ''} = {}) {
  let sequence = 0;
  const state = {screen: 'login', email: '', token: '', refreshToken: '', sessionId: '', userId: '', verificationId: '',
    purpose: 'register', resetToken: '', resendAt: 0, busy: false, error: '', success: '', profile: null,
    selectedUniversity: null, admins: null, selectedAdmin: null, people: null, peopleQuery: '', nextOffset: null};
  const clearPeople = () => { state.people = null; state.peopleQuery = ''; state.nextOffset = null; };
  const clearEducation = () => {
    clearPeople();
    state.selectedUniversity = null; state.admins = null; state.selectedAdmin = null;
  };
  try {
    const saved = JSON.parse(storage?.getItem(KEY) || '{}');
    if (typeof saved.token === 'string') state.token = saved.token;
    if (typeof saved.refreshToken === 'string') state.refreshToken = saved.refreshToken;
    if (typeof saved.sessionId === 'string') state.sessionId = saved.sessionId;
    if (typeof saved.email === 'string') state.email = saved.email;
    if (typeof saved.verificationId === 'string' && saved.verificationId) {
      state.verificationId = saved.verificationId;
      state.purpose = saved.purpose === 'reset' ? 'reset' : 'register';
      state.resendAt = Number.isFinite(saved.resendAt) ? saved.resendAt : 0;
      state.screen = 'code';
    }
  } catch { /* A disabled storage or old session must not block sign-in. */ }
  const persist = () => {
    try { storage?.setItem(KEY, JSON.stringify({
      token: state.token, email: state.email, verificationId: state.verificationId,
      refreshToken: state.refreshToken, sessionId: state.sessionId,
      purpose: state.purpose, resendAt: state.resendAt,
    })); } catch {}
  };
  const emit = change => { persist(); onChange({...state}, change); };
  const run = async (work, apply, {background=false} = {}) => {
    if (state.busy) return false;
    const current = ++sequence;
    let changed = true;
    let change;
    if (!background) { state.busy = true; state.error = ''; state.success = ''; emit(); }
    try {
      const result = await work();
      if (current !== sequence) return false;
      change = apply(result);
      changed = change !== false;
      return true;
    } catch (error) {
      if (current === sequence) {
        state.error = error instanceof ApiError ? error.message : 'Не удалось выполнить запрос. Попробуйте снова.';
        if (state.screen === 'quiz') state.quizErrors = error.details || [];
        if (error.status === 403 && state.screen === 'people') {
          state.people = null; state.nextOffset = null; state.selectedAdmin = null;
        }
        if (error.status === 401 && state.token) {
          state.token = ''; state.refreshToken = ''; state.sessionId = ''; state.userId = '';
          state.screen = 'login'; state.profile = null; clearEducation();
        }
        if (error.code === 'resend_too_soon') state.resendAt = now() + 60000;
        if (error.code === 'invalid_or_expired_token') { state.resetToken = ''; state.screen = 'forgot'; }
      }
      return false;
    } finally {
      if (current === sequence) {
        if (!background) state.busy = false;
        if (changed) emit(change === 'game-progress' ? change : undefined);
      }
    }
  };
  const fail = (message) => { state.error = message; state.success = ''; emit(); return Promise.resolve(false); };
  const setCode = (result, purpose) => {
    if (!result.verification_id) throw new ApiError('Сервер не вернул запрос подтверждения.');
    state.verificationId = result.verification_id; state.purpose = purpose;
    state.resendAt = now() + 60000; state.screen = 'code'; state.resetToken = '';
  };
  const authenticate = async (result) => {
    if (!result.access_token) throw new ApiError('Сервер не вернул токен входа.');
    const user = await api('me', undefined, result.access_token);
    if (!user.user_id) throw new ApiError('Не удалось загрузить аккаунт.');
    return {token: result.access_token, refreshToken: result.refresh_token || '',
      sessionId: result.session_id || '', userId: user.user_id};
  };
  let refreshPending = null;
  const authorized = async (path, data, method) => {
    const current = sequence;
    const accessToken = state.token;
    try { return await api(path, data, accessToken, ...(method ? [method] : [])); }
    catch (error) {
      if (error.status !== 401 || !state.refreshToken || !state.sessionId || current !== sequence) throw error;
      if (accessToken !== state.token) return api(path, data, state.token, ...(method ? [method] : []));
      const refreshToken = state.refreshToken, sessionId = state.sessionId;
      if (!refreshPending || refreshPending.token !== refreshToken || refreshPending.session !== sessionId) {
        const pending = {token:refreshToken, session:sessionId};
        pending.promise = (async()=>{
          const tokens = await api('refresh', {session_id:sessionId, refresh_token:refreshToken});
          if (state.refreshToken !== refreshToken || state.sessionId !== sessionId) throw new ApiError('Запрос отменён.');
          if (!tokens.access_token || !tokens.refresh_token || !tokens.session_id)
            throw new ApiError('Сервер не вернул токены сессии.', 401);
          state.token = tokens.access_token; state.refreshToken = tokens.refresh_token; state.sessionId = tokens.session_id;
          // Persist a consumed refresh token's replacement even if navigation cancelled its request.
          persist();
        })().finally(()=>{ if (refreshPending === pending) refreshPending = null; });
        refreshPending = pending;
      }
      await refreshPending.promise;
      if (current !== sequence) throw new ApiError('Запрос отменён.');
      return api(path, data, state.token, ...(method ? [method] : []));
    }
  };
  const currentAccount = async () => {
    const user = await authorized('me');
    if (!user.user_id) throw new ApiError('Не удалось загрузить аккаунт.');
    return {token: state.token, refreshToken: state.refreshToken, sessionId: state.sessionId, userId: user.user_id};
  };
  const setAccount = ({token, refreshToken, sessionId, userId}) => {
    game.resetGameView();
    state.token = token; state.refreshToken = refreshToken; state.sessionId = sessionId;
    state.userId = userId; state.screen = 'account'; state.profile = null;
    clearEducation();
    state.verificationId = ''; state.resetToken = ''; state.resendAt = 0;
  };
  const game = createGameController({state, authorized, run, emit, fail, storage, streamFetch, now, makeId:makeGameId, origin, joinCode});
  return {
    ...game,
    state,
    openQuizzes(create = false) {
      if (!state.token || state.busy) return Promise.resolve(false);
      return run(async () => {
        const profile = await authorized('profile');
        const universities = [...new Map((profile.university_position || []).filter(p => ['teacher','admin'].includes(p.role)).map(p => [p.university_id,{id:p.university_id,name:p.university_name}])).values()];
        if (!universities.length) throw new ApiError(messages.quiz_access_denied);
        const list = await authorized('quizzes');
        return {universities,quizzes:list.quizzes};
      }, result => {
        state.quizUniversities=result.universities; state.quizzes=result.quizzes; state.quizErrors=[]; state.quizDirty=false;
        state.selectedQuiz=null; state.historyQuiz=null; state.quizSessions=null; state.sessionResults=null;
        state.screen=create?'quiz':'quizzes';
        if(create) {state.quizDraft=newQuiz(result.universities.length===1?result.universities[0].id:'');state.quizDirty=true;}
      });
    },
    openQuizActions(id) {
      if(state.busy || !state.token || state.screen!=='quizzes') return;
      state.selectedQuiz=state.quizzes?.find(quiz=>quiz.id===id && quiz.status==='ready') || null;
      state.error='';state.success='';emit();
    },
    closeQuizActions() { state.selectedQuiz=null;emit(); },
    openQuizSessions(id=state.historyQuiz?.id) {
      if(state.busy || !state.token) return Promise.resolve(false);
      const quiz=state.quizzes?.find(quiz=>quiz.id===id);
      if(!quiz) return Promise.resolve(false);
      state.selectedQuiz=null;state.historyQuiz=quiz;state.quizSessions=null;
      state.sessionResults=null;state.screen='quiz-sessions';
      return run(()=>authorized('quizzes/'+encodeURIComponent(id)+'/sessions'),result=>{
        if(!result?.success || !Array.isArray(result.sessions)) throw new ApiError('Не удалось загрузить сессии. Попробуйте ещё раз.');
        state.quizSessions=result.sessions;
      });
    },
    openSessionResults(id=state.resultsSession?.session_id) {
      if(state.busy || !state.token) return Promise.resolve(false);
      const session=state.quizSessions?.find(item=>item.session_id===id);
      if(!session) return Promise.resolve(false);
      state.resultsSession=session;state.sessionResults=null;state.screen='quiz-results';
      return run(()=>authorized('game/sessions/'+encodeURIComponent(id)+'/results'),result=>{
        if(!result?.success || result.session_id!==id || !Array.isArray(result.results)) throw new ApiError('Не удалось загрузить результаты. Попробуйте ещё раз.');
        state.sessionResults=result.results;
      });
    },
    newQuiz() {
      if(state.busy || !state.quizUniversities?.length) return;
      state.quizDraft=newQuiz(state.quizUniversities.length===1?state.quizUniversities[0].id:'');state.quizDirty=true;state.quizErrors=[];state.error='';state.success='';state.screen='quiz';emit();
    },
    loadQuiz(id) {
      if(state.busy || !state.token) return Promise.resolve(false);
      return run(()=>authorized('quizzes/'+encodeURIComponent(id)),result=>{
        state.selectedQuiz=null;state.quizDraft=result.quiz;state.quizDirty=false;state.quizErrors=[];state.screen='quiz';
      });
    },
    editQuiz(field,value,qi,ai) {
      if(state.busy || state.screen!=='quiz') return;
      if(state.quizDraft.status==='ready') return;
      const d=state.quizDraft,q=d.questions[qi];
      const target=ai===undefined ? (q || d):q?.answers[ai];
      if(!target) return;
      if(field==='is_correct' && q.type==='single') q.answers.forEach(a=>{a.is_correct=false;});
      target[field]=field==='time_seconds' ? (value===''?null:value):value;
      if(field==='type' && value==='single') {const first=q.answers.findIndex(a=>a.is_correct);q.answers.forEach((a,i)=>{a.is_correct=i===first;});}
      state.quizDirty=true;state.success='';state.quizErrors=[];
      if(field==='type') emit();
    },
    changeQuiz(action,qi,ai) {
      if(state.busy || state.screen!=='quiz') return;
      if(state.quizDraft.status==='ready') return;
      const d=state.quizDraft,q=d.questions[qi];
      if(action==='quiz-add-question') {
        if(d.questions.length>=100 || !d.questions.every(q=>questionComplete(q,d.default_time_seconds))) return;
        d.questions.push(newQuestion());
      }
      if(action==='quiz-remove-question' && d.questions.length>1) d.questions.splice(qi,1);
      if(action==='quiz-add-answer' && q.answers.length<20) q.answers.push(newAnswer());
      if(action==='quiz-remove-answer' && q.answers.length>2) q.answers.splice(ai,1);
      if(action==='quiz-remove-image') {const target=ai===undefined?q:q.answers[ai];target.image_id=null;target.image_url='';target.upload_error='';}
      state.quizDirty=true;state.quizErrors=[];state.success='';emit();
    },
    saveQuiz(status) {
      if(state.busy || state.screen!=='quiz') return Promise.resolve(false);
      if(state.quizDraft.status==='ready') return Promise.resolve(false);
      const d=state.quizDraft;
      if(!d.university_id) return fail('Выберите учебное заведение.');
      if(!(Number(d.default_time_seconds)>0) || d.questions.some(q=>q.time_seconds!==null && !(Number(q.time_seconds)>0))) return fail('Время должно быть больше нуля. Пустое время вопроса означает время по умолчанию.');
      state.quizErrors=[];
      return run(()=>authorized('quizzes'+(d.id?'/'+d.id:''),quizPayload(d,status),d.id?'PUT':'POST'),result=>{
        d.id=result.quiz_id;d.revision=result.revision;d.status=result.status;state.quizDirty=false;
        state.success=result.status==='draft'?'Черновик сохранён. Его можно открыть в «Мои квизы».':'Квиз опубликован. Редактирование больше недоступно.';
      });
    },
    uploadQuizImage(file,qi,ai) {
      if(state.busy || state.screen!=='quiz' || !file) return Promise.resolve(false);
      if(state.quizDraft.status==='ready') return Promise.resolve(false);
      const q=state.quizDraft.questions[qi],target=ai===undefined?q:q.answers[ai];
      if(file.size>5242880) return fail('Максимальный размер картинки — 5 МиБ.');
      if(!['image/png','image/jpeg','image/webp'].includes(file.type)) return fail(messages.unsupported_image_format);
      target.upload_error='';state.quizUploading=true;
      const form=new FormData();form.append('file',file);
      return run(()=>authorized('media/images',form),result=>{
        target.image_id=result.media_id;target.image_url=result.image_url;state.quizDirty=true;
      }).then(ok=>{state.quizUploading=false;if(!ok && state.screen==='quiz') target.upload_error=state.error;emit();return ok;});
    },
    remaining: () => Math.max(0, Math.ceil((state.resendAt - now()) / 1000)),
    navigate(screen) {
      if (screen === 'profile' && state.token && state.profile) {
        ++sequence; state.busy = false; state.screen = 'profile';
        state.error = ''; state.success = ''; clearEducation(); emit(); return;
      }
      if (screen === 'university' && state.token && state.selectedUniversity) {
        clearPeople();
        ++sequence; state.busy = false; state.screen = 'university';
        state.error = ''; state.success = ''; state.admins = null; state.selectedAdmin = null; emit(); return;
      }
      if (screen === 'account' && state.token) {
        ++sequence; state.busy = false; state.screen = 'account';
        state.error = ''; state.success = ''; state.profile = null; clearEducation(); emit(); return;
      }
      if (!['login', 'register', 'forgot'].includes(screen)) return;
      ++sequence; state.busy = false; state.screen = screen; state.error = ''; state.success = '';
      state.token = ''; state.refreshToken = ''; state.sessionId = ''; state.userId = ''; state.profile = null;
      clearEducation();
      state.verificationId = ''; state.resetToken = ''; state.resendAt = 0; emit();
    },
    async start() {
      if (state.token) {const ok=await run(currentAccount, setAccount); if(ok) await game.resumeGame(); return ok;}
      emit(); return true;
    },
    openProfile() {
      if (state.busy || !state.token) return Promise.resolve(false);
      state.screen = 'profile'; state.profile = null;
      clearEducation();
      return run(() => authorized('profile'), result => {
        if (!result || result.success !== true || !Array.isArray(result.university_position))
          throw new ApiError('Не удалось загрузить профиль. Попробуйте ещё раз.');
        state.profile = result;
      });
    },
    openUniversity(universityId) {
      if (state.busy || !state.token) return false;
      const position = state.profile?.university_position.find(
        item => item.role === 'admin' && item.university_id === universityId);
      if (!position || !universityId) return false;
      ++sequence; clearEducation();
      state.selectedUniversity = {id: position.university_id, name: position.university_name,
        adminScope: position.admin_scope ?? null};
      state.screen = 'university'; state.error = ''; state.success = ''; emit(); return true;
    },
    openPeople() {
      if (state.busy || !state.token || !state.selectedUniversity) return false;
      ++sequence; clearPeople(); state.selectedAdmin = null;
      state.screen = 'people'; state.error = ''; state.success = ''; emit(); return true;
    },
    searchPeople(query, more = false) {
      if (state.busy || state.screen !== 'people' || !state.selectedUniversity) return Promise.resolve(false);
      const normalized = (more ? state.peopleQuery : query).trim();
      if ([...normalized].length < 2 || [...normalized].length > 100 || normalized.includes('\0'))
        return fail(messages.invalid_search_params);
      if (more && state.nextOffset === null) return Promise.resolve(false);
      const offset = more ? state.nextOffset : 0;
      if (!more) { state.people = null; state.nextOffset = null; }
      state.peopleQuery = normalized; state.selectedAdmin = null;
      const params = new URLSearchParams({q: normalized, limit: '20', offset: String(offset)});
      return run(() => authorized('education/universities/' + encodeURIComponent(state.selectedUniversity.id) + '/people?' + params), result => {
        if (result?.success !== true || !Array.isArray(result.people) || typeof result.has_more !== 'boolean'
          || (result.has_more && (!Number.isInteger(result.next_offset) || result.next_offset <= offset || result.next_offset > 10000)))
          throw new ApiError('Не удалось загрузить результаты. Попробуйте ещё раз.');
        state.people = [...new Map([...(more ? state.people || [] : []), ...result.people].map(person => [person.user_id, person])).values()];
        state.nextOffset = result.has_more ? result.next_offset : null;
      });
    },
    openPerson(userId) {
      if (state.busy || state.screen !== 'people') return;
      state.selectedAdmin = state.people?.find(person => person.user_id === userId) || null; emit();
    },
    openAdmins() {
      if (state.busy || !state.token || !state.selectedUniversity) return Promise.resolve(false);
      const universityId = state.selectedUniversity.id;
      state.screen = 'admins'; state.admins = null; state.selectedAdmin = null;
      return run(() => authorized('education/universities/' + encodeURIComponent(universityId) + '/admins'), result => {
        if (result?.success !== true || !Array.isArray(result.admins))
          throw new ApiError('Не удалось загрузить администраторов. Попробуйте ещё раз.');
        state.admins = result.admins;
      });
    },
    openAdmin(membershipId) {
      if (state.busy || state.screen !== 'admins') return;
      state.selectedAdmin = state.admins?.find(item => item.membership_id === membershipId) || null;
      emit();
    },
    closeAdmin() { state.selectedAdmin = null; emit(); },
    login(email, password) {
      if (state.busy) return Promise.resolve(false);
      const error = validatePassword(password);
      if (error) return fail(error);
      state.email = email.trim();
      return run(async () => authenticate(await api('login', {email: state.email, password})), setAccount).then(async ok=>{if(ok) await game.resumeGame(); return ok;});
    },
    register(email, password, confirmation) {
      if (state.busy) return Promise.resolve(false);
      const error = validatePassword(password, confirmation);
      if (error) return fail(error);
      state.email = email.trim();
      return run(() => api('register', {email: state.email, password, password_confirmation: confirmation}),
        result => setCode(result, 'register'));
    },
    requestReset(email) {
      if (state.busy) return Promise.resolve(false);
      state.email = email.trim();
      return run(() => api('forgot-password/email-check', {email: state.email}), result => setCode(result, 'reset'));
    },
    verify(code) {
      if (!/^\d{6}$/.test(code)) return fail('Введите шестизначный код.');
      if (!state.verificationId) return fail('Запросите новый код.');
      const payload = {verification_id: state.verificationId, code};
      if (state.purpose === 'reset') {
        return run(() => api('forgot-password/verify-code', payload), result => {
          if (!result.reset_token) throw new ApiError('Сервер не вернул разрешение на смену пароля.');
          state.resetToken = result.reset_token; state.verificationId = ''; state.screen = 'password';
        });
      }
      return run(async () => authenticate(await api('verify-email', payload)), setAccount).then(async ok=>{if(ok) await game.resumeGame(); return ok;});
    },
    resend() {
      if (state.resendAt > now()) return Promise.resolve(false);
      return run(() => state.purpose === 'reset'
        ? api('forgot-password/email-check', {email: state.email})
        : api('resend-code', {verification_id: state.verificationId}), result => {
          setCode(result, state.purpose); state.success = 'Новый код готов. Предыдущий больше не действует.';
        });
    },
    updatePassword(password, confirmation) {
      const error = validatePassword(password, confirmation);
      if (error) return fail(error);
      if (!state.resetToken) { this.navigate('forgot'); return fail('Запросите новый код для смены пароля.'); }
      return run(() => api('forgot-password/update-password', {
        reset_token: state.resetToken, password, password_confirmation: confirmation,
      }), () => {
        state.screen = 'login'; state.resetToken = ''; state.token = ''; state.refreshToken = ''; state.sessionId = ''; state.userId = '';
        state.verificationId = ''; state.success = 'Пароль изменён. Войдите с новым паролем.';
      });
    },
    refresh() { return run(currentAccount, result => {
      setAccount(result); state.success = 'Сессия активна. Доступ к аккаунту подтверждён.';
    }); },
    logout() {
      return run(async () => {
        try { await authorized('logout', {}); }
        catch (error) { if (error.status !== 401) throw error; }
      }, () => {
      Object.assign(state, {screen: 'login', token: '', refreshToken: '', sessionId: '', userId: '', verificationId: '', resetToken: '', profile: null,
        resendAt: 0, busy: false, error: '', success: 'Вы вышли из аккаунта.'});
      clearEducation();
      game.resetGameView();
      try { storage?.removeItem(KEY); } catch {}
      emit();
      });
    },
  };
}
export const escapeHtml = (value) => String(value).replace(/[&<>"']/g, c => ({
  '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;',
}[c]));
const passwordField = (name, label, autocomplete = 'new-password') => `
<div class="field"><label for="${name}">${label}</label><div class="input-wrap">
<input id="${name}" name="${name}" type="password" autocomplete="${autocomplete}" required data-password placeholder="${name === 'password_confirmation' ? 'Ещё раз тот же пароль' : 'Ваш пароль'}">
<button class="show-password" type="button" data-toggle="${name}" aria-label="Показать: ${label}" aria-pressed="false">Показать</button></div></div>`;
const emailField = (email) => `<div class="field"><label for="email">Электронная почта</label><input id="email" name="email" type="email" autocomplete="email" maxlength="254" required placeholder="you@example.com" value="${escapeHtml(email)}"></div>`;
const primary = (label, busy) => `<button type="submit" class="primary" ${busy ? 'disabled' : ''}><strong>${busy ? 'Подождите…' : label}</strong><span aria-hidden="true">↗</span></button>`;
const roleNames = {admin: 'Администратор', student: 'Студент', teacher: 'Преподаватель'};
export function safeAvatarUrl(value) {
  if (typeof value !== 'string' || !value) return '';
  try {
    const url = new URL(value, 'http://localhost');
    return ['https:', 'http:'].includes(url.protocol) && !url.username && !url.password ? value : '';
  } catch { return ''; }
}
const personName = person => [person.last_name, person.first_name, person.middle_name].filter(Boolean).join(' ') || 'Имя не указано';
const personAvatar = person => {
  const url = safeAvatarUrl(person.avatar_url);
  const initials = ((person.first_name || '').slice(0, 1) + (person.last_name || '').slice(0, 1)) || '?';
  return `<span class="avatar person-avatar" aria-hidden="true"><span>${escapeHtml(initials)}</span>
    ${url ? `<img src="${escapeHtml(url)}" alt="" referrerpolicy="no-referrer" data-avatar>` : ''}</span>`;
};
const membershipCard = position => {
  const clickable = position.role === 'admin' && position.university_id;
  const contents = `<span class="university-icon" aria-hidden="true">▥</span><span class="membership-info">
    <span class="membership-name">${escapeHtml(position.university_name)}</span>
    <span class="role-badge">${escapeHtml(position.role === 'admin' && position.admin_scope === 'faculties'
      ? 'Администратор факультетов' : roleNames[position.role] || position.role)}</span>
    ${position.role === 'student' ? `<span class="membership-group">Группа: ${escapeHtml(position.group_name || 'Не назначена')}</span>` : ''}
    </span>${clickable ? '<span class="membership-arrow" aria-hidden="true">→</span>' : ''}`;
  return clickable
    ? `<button type="button" class="membership membership-button" data-action="university" data-university-id="${escapeHtml(position.university_id)}">${contents}</button>`
    : `<article class="membership">${contents}</article>`;
};
export function renderView(state) {
  const {screen, busy, email} = state;
  const notice = state.error ? `<div class="notice error" role="alert">${escapeHtml(state.error)}</div>`
    : state.success ? `<div class="notice success" role="status">${escapeHtml(state.success)}</div>` : '';
  const back = (target = 'login') => `<button type="button" class="text-button back" data-nav="${target}">← Вернуться ко входу</button>`;
  let content;
  if (screen === 'game' || screen === 'game-join') {
    content = renderGame(state, notice);
  } else if (['quiz','quizzes','quiz-sessions','quiz-results'].includes(screen)) {
    content = renderQuiz(state, notice);
  } else if (screen === 'login' || screen === 'register') {
    const register = screen === 'register';
    content = `<div class="tabs" role="tablist" aria-label="Вход или регистрация">
      <button role="tab" type="button" aria-selected="${!register}" data-nav="login">Войти</button>
      <button role="tab" type="button" aria-selected="${register}" data-nav="register">Регистрация</button></div>
      <h2 tabindex="-1">${register ? 'Давайте знакомиться' : 'С возвращением!'}</h2>
      <p class="subtitle">${register ? 'Пара деталей — и у вас будет свой аккаунт.' : 'Войдите, чтобы продолжить свою историю.'}</p>
      ${notice}<form data-form="${screen}">${emailField(email)}
      ${!register ? '<div class="field-head"><span></span><button class="text-button" type="button" data-nav="forgot">Не помню пароль</button></div>' : ''}
      ${passwordField('password', 'Пароль', register ? 'new-password' : 'current-password')}
      ${register ? passwordField('password_confirmation', 'Повторите пароль') + '<p class="hint">До 72 байт. Для кириллицы один символ занимает несколько байт.</p><br>' : ''}
      ${primary(register ? 'Создать аккаунт' : 'Войти в аккаунт', busy)}</form>
      <p class="switch-prompt">${register ? 'Уже знакомы?' : 'Пока нет аккаунта?'} <button type="button" class="text-button" data-nav="${register ? 'login' : 'register'}">${register ? 'Войти' : 'Зарегистрироваться'}</button></p>`;
  } else if (screen === 'forgot') {
    content = `${back()}<p class="step-label">Восстановление · 1 из 3</p><h2 tabindex="-1">Забыли пароль?</h2>
      <p class="subtitle">Укажите почту вашего аккаунта. Получите код и задайте новый пароль.</p>
      ${notice}<form data-form="forgot">${emailField(email)}${primary('Получить код', busy)}</form>
      <p class="switch-prompt">Доступно для подтверждённой почты.</p>`;
  } else if (screen === 'code') {
    content = `${back()}<div class="progress" aria-hidden="true"><span class="done"></span><span class="done"></span><span></span></div>
      <p class="step-label">${state.purpose === 'reset' ? 'Восстановление · 2 из 3' : 'Подтверждение почты'}</p>
      <h2 tabindex="-1">Остался только код</h2><p class="subtitle">Введите 6 цифр для<br><strong>${escapeHtml(email)}</strong></p>
      ${notice}<form data-form="code"><div class="field"><label for="code">Код подтверждения</label>
      <input class="code-input" id="code" name="code" inputmode="numeric" autocomplete="one-time-code" pattern="[0-9]{6}" maxlength="6" required placeholder="000000"></div>
      ${primary('Подтвердить код', busy)}</form>
      <div class="resend"><button type="button" class="text-button" data-action="resend" ${busy ? 'disabled' : ''}>Получить новый код</button></div>
      <p class="dev-note">Локальный режим: код отображается в консоли запущенного сервера. Отправка через Postbox пока отключена.</p>`;
  } else if (screen === 'password') {
    content = `${back()}<p class="step-label">Восстановление · 3 из 3</p><h2 tabindex="-1">Новый пароль</h2>
      <p class="subtitle">Почта подтверждена. Придумайте новый пароль для вашего аккаунта.</p>
      ${notice}<form data-form="password">${passwordField('password', 'Новый пароль')}${passwordField('password_confirmation', 'Повторите новый пароль')}
      ${primary('Сохранить пароль', busy)}</form>`;
  } else if (screen === 'profile') {
    const p = state.profile;
    const name = p ? [p.last_name, p.first_name, p.middle_name].filter(Boolean).join(' ') : '';
    const avatar = safeAvatarUrl(p?.avatar_url);
    content = `<button type="button" class="text-button back" data-nav="account">← На главную</button>
      <p class="step-label">Личный кабинет</p><h2 tabindex="-1">Мой профиль</h2>
      ${notice}${busy ? '<p class="subtitle" role="status">Загружаем профиль…</p>' : ''}
      ${p ? `<div class="profile-heading"><div class="avatar">
        <span aria-hidden="true">${escapeHtml(((p.first_name || '').slice(0, 1) + (p.last_name || '').slice(0, 1)) || 'К')}</span>
        ${avatar ? `<img src="${escapeHtml(avatar)}" alt="Фото профиля" referrerpolicy="no-referrer" data-avatar>` : ''}
        </div><div><h3>${escapeHtml(name || 'Имя не указано')}</h3><p>${escapeHtml(p.email)}</p></div></div>
        <dl class="profile-details"><div><dt>Имя</dt><dd>${escapeHtml(p.first_name || 'Не указано')}</dd></div>
        <div><dt>Фамилия</dt><dd>${escapeHtml(p.last_name || 'Не указана')}</dd></div>
        <div><dt>Отчество</dt><dd>${escapeHtml(p.middle_name || 'Не указано')}</dd></div></dl>
        <h3 class="section-title">Мои учебные заведения</h3><div class="membership-list">
        ${p.university_position.length ? p.university_position.map(membershipCard).join('') : '<p class="subtitle">Пока нет привязок к учебным заведениям.</p>'}
        </div>` : !busy ? '<button type="button" class="secondary" data-action="profile">Попробовать снова</button>' : ''}`;
  } else if (screen === 'university') {
    content = `<button type="button" class="text-button back" data-nav="profile">← Назад в профиль</button>
      <p class="step-label">${state.selectedUniversity?.adminScope === 'faculties' ? 'Администрирование факультетов' : state.selectedUniversity?.adminScope === 'university' ? 'Управление вузом' : 'Учебное заведение'}</p><h2 tabindex="-1">${escapeHtml(state.selectedUniversity?.name || 'Учебное заведение')}</h2>
      <p class="subtitle">Выберите раздел для просмотра и управления.</p>${notice}
      <button type="button" class="secondary" data-action="people">Найти человека в вузе →</button>
      <div class="university-sections">
        <button type="button" class="section-card" data-action="admins"><span class="section-icon" aria-hidden="true">♙</span>
          <span><strong>Посмотреть администраторов</strong><span class="section-description">Сотрудники и контакты</span></span><span class="section-arrow" aria-hidden="true">→</span></button>
        ${[['Посмотреть преподавателей', 'Преподаватели и их группы'], ['Посмотреть студентов', 'Студенты вашего вуза'], ['Посмотреть группы', 'Учебные группы']].map(([title, description]) => `
          <button type="button" class="section-card" disabled><span class="section-icon" aria-hidden="true">▥</span>
          <span><strong>${title}</strong><span class="section-description">${description}</span></span><span class="section-soon">Скоро</span></button>`).join('')}
      </div>`;
  } else if (screen === 'people') {
    content = `<button type="button" class="text-button back" data-nav="university">← К разделам вуза</button>
      <p class="step-label university-label">${escapeHtml(state.selectedUniversity?.name || '')}</p>
      <h2 tabindex="-1">Поиск людей</h2><p class="subtitle">Введите имя, фамилию или отчество в любом порядке. Нажмите на человека, чтобы посмотреть почту.</p>
      <form data-form="people" role="search"><label for="people-query">Кого ищем?</label><div class="people-search">
        <input id="people-query" name="query" type="search" required aria-describedby="search-hint" placeholder="Например, Литвин Кирилл" value="${escapeHtml(state.peopleQuery || '')}">
        <button type="submit" class="primary">Найти</button></div><p id="search-hint" class="hint">От 2 до 100 символов. Поиск только в выбранном вузе.</p></form>
      ${notice}${busy ? '<p role="status" class="list-loading">Ищем людей…</p>' : ''}
      ${state.people === null ? (!busy && !state.error ? '<p class="empty-list">Начните с имени или фамилии — результаты появятся здесь.</p>' : '') : `
        <p class="list-count" role="status">По запросу «${escapeHtml(state.peopleQuery)}» показано: ${state.people.length}</p>
        <div class="admin-list">${state.people.map(person => `<button type="button" class="admin-card" data-action="person-contact" data-membership-id="${escapeHtml(person.user_id)}" aria-haspopup="dialog">
          ${personAvatar(person)}<span class="person-info"><span class="admin-name">${escapeHtml(personName(person))}</span>
          <span class="person-roles">${(person.roles || []).map(role => escapeHtml(roleNames[role] || role)).join(' · ')}</span>
          ${person.roles?.includes('student') ? `<span class="person-study">Группа: ${escapeHtml(person.student_details?.group?.name || 'Не назначена')}<br>Факультеты: ${escapeHtml(person.student_details?.faculties?.map(f => f.name).join(', ') || 'Не указаны')}</span>` : ''}
          </span><span class="contact-arrow" aria-hidden="true">↗</span></button>`).join('')}</div>
        ${state.people.length ? '' : '<p class="empty-list">Никого не нашли. Попробуйте другую часть имени или проверьте написание.</p>'}
        ${state.nextOffset !== null ? '<button type="button" class="secondary" data-action="more-people">Показать ещё</button>' : ''}`}
      ${state.selectedAdmin ? `<dialog class="contact-dialog" aria-labelledby="contact-title">
        <button type="button" class="dialog-close" data-action="close-contact" aria-label="Закрыть окно" autofocus>×</button>
        ${personAvatar(state.selectedAdmin)}<h3 id="contact-title">${escapeHtml(personName(state.selectedAdmin))}</h3>
        <p class="contact-label">Электронная почта</p><a class="contact-email" href="mailto:${escapeHtml(encodeURIComponent(state.selectedAdmin.email))}">${escapeHtml(state.selectedAdmin.email)}</a></dialog>` : ''}`;
  } else if (screen === 'admins') {
    content = `<button type="button" class="text-button back" data-nav="university">← К разделам вуза</button>
      <p class="step-label university-label">${escapeHtml(state.selectedUniversity?.name || '')}</p>
      <h2 tabindex="-1">Администраторы</h2><p class="subtitle">Нажмите на сотрудника, чтобы посмотреть почту.</p>
      ${notice}${busy ? '<p class="list-loading" role="status">Загружаем администраторов…</p>' : ''}
      ${state.admins ? `<p class="list-count">Всего: ${state.admins.length}</p><div class="admin-list">
        ${state.admins.map(admin => `<button type="button" class="admin-card" data-action="admin-contact" data-membership-id="${escapeHtml(admin.membership_id)}" aria-haspopup="dialog">
          ${personAvatar(admin)}<span class="admin-name">${escapeHtml(personName(admin))}</span><span class="contact-arrow" aria-hidden="true">↗</span></button>`).join('')}
        </div>${state.admins.length ? '' : '<p class="empty-list">В этом вузе пока нет активных администраторов.</p>'}`
        : !busy ? '<button type="button" class="secondary" data-action="admins">Попробовать снова</button>' : ''}
      ${state.selectedAdmin ? `<dialog class="contact-dialog" aria-labelledby="contact-title">
        <button type="button" class="dialog-close" data-action="close-contact" aria-label="Закрыть окно" autofocus>×</button>
        ${personAvatar(state.selectedAdmin)}<h3 id="contact-title">${escapeHtml(personName(state.selectedAdmin))}</h3>
        <p class="contact-label">Электронная почта</p><a class="contact-email" href="mailto:${escapeHtml(encodeURIComponent(state.selectedAdmin.email))}">${escapeHtml(state.selectedAdmin.email)}</a>
        </dialog>` : ''}`;
  } else {
    content = `<div class="quiz-welcome"><p class="step-label">Всё начинается с вопроса</p>
      <h2 tabindex="-1">Готовы проверить<br>свои знания?</h2>
      <p class="subtitle">Создайте свой квиз и сохраните вопросы.<br>Всё начинается с любопытства.</p>
      ${notice}<div class="quiz-symbol" aria-hidden="true">?</div>
      <button type="button" class="primary quiz-button" data-action="create-quiz">＋ Создать квиз</button>
      <button type="button" class="secondary quiz-button" data-action="my-quizzes">Мои квизы →</button>
      <button type="button" class="primary quiz-button" data-action="game-join">Присоединиться по коду</button>
      ${state.game?'<button type="button" class="text-button" data-action="game-resume">Вернуться в сессию →</button>':''}
      <p class="hint">Чтобы провести игру, откройте опубликованный квиз в «Мои квизы».</p></div>`;
  }
  return `<div class="auth-card" aria-busy="${busy}">${content}</div>`;
}
export function mountApp(root, options = {}) {
  let lastScreen = '';
  let lastGame = null;
  let lastContactId = null;
  let lastQuizId = null;
  let controller;
  const render = (state, change) => {
    if(change === 'game-progress' && lastScreen === 'game' && state.screen === 'game'
      && lastGame?.session.id === state.game?.session.id
      && lastGame?.session.status === state.game?.session.status
      && lastGame?.current_question?.id === state.game?.current_question?.id) {
      updateGameProgress(root, state.game, lastGame, state.gameAlerts);
      lastGame = state.game;
      tick();
      controller?.syncGameEvents();
      return;
    }
    // Keep values across the busy/error render, never in storage.
    const values = new Map([...root.querySelectorAll('input')].map(el => [el.name, el.value]));
    const focusName = root.ownerDocument.activeElement?.getAttribute('name');
    const focusId = root.ownerDocument.activeElement?.id;
    root.innerHTML = renderView(state);
    const signedIn = ['account', 'profile', 'university', 'admins', 'people', 'quiz', 'quizzes', 'quiz-sessions', 'quiz-results', 'game', 'game-join'].includes(state.screen);
    root.ownerDocument.body.classList.toggle('signed-in', signedIn);
    const nav = root.ownerDocument.getElementById('account-nav');
    if (nav) {
      nav.hidden = !signedIn;
      nav.querySelectorAll('button').forEach(button => { button.disabled = state.busy; });
      nav.querySelector('[data-action="profile"]')?.setAttribute('aria-current', ['profile', 'university', 'admins', 'people'].includes(state.screen) ? 'page' : 'false');
    }
    root.closest('.workspace')?.setAttribute('aria-label', signedIn ? 'Личный кабинет' : 'Авторизация');
    root.querySelectorAll('[data-avatar]').forEach(img => img.addEventListener('error', event => { event.target.remove(); }));
    if (lastScreen === state.screen && state.screen !== 'quiz') {
      for (const el of root.querySelectorAll('input')) if (values.has(el.name)) el.value = values.get(el.name);
      const focus = [...root.querySelectorAll('input')].find(el => el.name === focusName);
      focus?.focus();
    } else if (lastScreen === 'quiz' && state.screen === 'quiz') {
      const gameName = root.querySelector('[name="game-name"]');
      if (gameName && values.has('game-name')) gameName.value = values.get('game-name');
      if (focusId) root.ownerDocument.getElementById(focusId)?.focus({preventScroll:true});
    } else {
      root.querySelector('h2')?.focus();
    }
    lastScreen = state.screen;
    lastGame = state.game;
    for (const el of root.querySelectorAll('input,button')) if (state.busy && !el.dataset.nav) el.disabled = true;
    const dialog = root.querySelector('dialog');
    if (dialog) {
      const closeDialog=()=>dialog.hasAttribute('data-quiz-dialog')?controller.closeQuizActions():controller.closeAdmin();
      dialog.addEventListener('cancel', event => { event.preventDefault(); closeDialog(); });
      dialog.addEventListener('click', event => {
        if (event.target !== dialog) return;
        const bounds = dialog.getBoundingClientRect();
        if (event.clientX < bounds.left || event.clientX > bounds.right || event.clientY < bounds.top || event.clientY > bounds.bottom)
          closeDialog();
      });
      dialog.showModal();
    } else if (lastContactId && ['admins', 'people'].includes(state.screen)) {
      [...root.querySelectorAll('[data-membership-id]')].find(el => el.dataset.membershipId === lastContactId)?.focus();
    } else if (lastQuizId && state.screen==='quizzes') {
      [...root.querySelectorAll('[data-action="quiz-actions"]')].find(el=>el.dataset.id===lastQuizId)?.focus();
    }
    lastContactId = state.selectedAdmin?.membership_id || state.selectedAdmin?.user_id || null;
    lastQuizId = state.selectedQuiz?.id || null;
    tick();
    controller?.syncGameEvents();
  };
  controller = createController({...options, joinCode:new URL(root.ownerDocument.defaultView.location.href).searchParams.get('join') || '', onChange: render});
  const tick = () => {
    controller.syncGamePresence();
    const secondsLeft=gameSeconds(controller.state);
    const timer=root.querySelector('[data-game-timer]');
    if(timer) timer.textContent=secondsLeft>0?secondsLeft+' с':'Время истекло';
    if(timer && secondsLeft===0) {
      root.querySelectorAll('[data-game-answer], [data-action="game-submit"]').forEach(el=>{el.disabled=true;});
    }
    const button = root.querySelector('[data-action="resend"]');
    if (!button) return;
    const seconds = controller.remaining();
    button.disabled = controller.state.busy || seconds > 0;
    button.textContent = seconds > 0 ? `Новый код через ${seconds} с` : 'Получить новый код';
  };
  const interval = setInterval(tick, 1000);
  const canLeave = () => !controller.state.quizDirty || controller.state.screen!=='quiz' || root.ownerDocument.defaultView.confirm('Есть несохранённые изменения. Выйти из редактора?');
  const beforeUnload = event => { if(controller.state.screen==='quiz' && controller.state.quizDirty) {event.preventDefault();event.returnValue='';} };
  root.ownerDocument.defaultView.addEventListener('beforeunload',beforeUnload);
  const pageHide = () => controller.suspendGamePresence();
  const pageShow = () => controller.syncGamePresence();
  root.ownerDocument.defaultView.addEventListener('pagehide',pageHide);
  root.ownerDocument.defaultView.addEventListener('pageshow',pageShow);
  root.addEventListener('input', event => {
    const el=event.target,field=el.dataset.quizField;
    if(!field || el.tagName==='SELECT' || ['radio','checkbox'].includes(el.type)) return;
    controller.editQuiz(field,el.value,el.dataset.q===undefined?undefined:Number(el.dataset.q),el.dataset.a===undefined?undefined:Number(el.dataset.a));
    const d=controller.state.quizDraft;
    const add=root.querySelector('[data-action="quiz-add-question"]');
    const complete=d.questions.length<100 && d.questions.every(q=>questionComplete(q,d.default_time_seconds));
    if(add) add.disabled=!complete;
    const hint=root.querySelector('#next-question-hint');if(hint) hint.textContent=complete?'Можно добавить следующий вопрос.':'Заполните вопросы, минимум два ответа и отметьте правильные варианты.';
    const dirty=root.querySelector('[data-quiz-dirty]');if(dirty) dirty.textContent='Есть несохранённые изменения';
  });
  root.addEventListener('change', event => {
    if(event.target.dataset.gameAnswer) {
      controller.chooseGameAnswer(event.target.dataset.gameAnswer,event.target.checked); return;
    }
    const el=event.target,qi=el.dataset.q===undefined?undefined:Number(el.dataset.q),ai=el.dataset.a===undefined?undefined:Number(el.dataset.a);
    if(el.hasAttribute('data-quiz-image')) {void controller.uploadQuizImage(el.files?.[0],qi,ai);return;}
    if(el.dataset.quizField && (el.tagName==='SELECT' || ['radio','checkbox'].includes(el.type))) {
      controller.editQuiz(el.dataset.quizField,['radio','checkbox'].includes(el.type)?el.checked:el.value,qi,ai);
      if(el.dataset.quizField!=='type') render(controller.state);
    }
  });
  root.addEventListener('submit', event => {
    event.preventDefault();
    if (controller.state.busy) return;
    const form = event.target;
    if(form.dataset.form==='game-create') {if(form.reportValidity()) void controller.createGame(new FormData(form).get('game-name'));return;}
    if(form.dataset.form==='game-join') {if(form.reportValidity()) void controller.joinGame(new FormData(form).get('game-code'));return;}
    if(form.dataset.form==='game-origin') {if(form.reportValidity()) controller.setGameOrigin(new FormData(form).get('game-origin'));return;}
    if(form.dataset.form==='quiz') {void controller.saveQuiz('ready');return;}
    if (!form.reportValidity()) return;
    const data = Object.fromEntries(new FormData(form));
    const kind = form.dataset.form;
    if (kind === 'people') void controller.searchPeople(data.query);
    if (kind === 'login') void controller.login(data.email, data.password);
    if (kind === 'register') void controller.register(data.email, data.password, data.password_confirmation);
    if (kind === 'forgot') void controller.requestReset(data.email);
    if (kind === 'code') void controller.verify(data.code);
    if (kind === 'password') void controller.updatePassword(data.password, data.password_confirmation);
  });
  root.addEventListener('click', event => {
    const button = event.target.closest('button');
    if (!button || button.disabled) return;
    if (button.dataset.nav) { if(canLeave()) controller.navigate(button.dataset.nav); return; }
    if (controller.state.busy) return;
    const action=button.dataset.action,qi=Number(button.dataset.q),ai=button.dataset.a===undefined?undefined:Number(button.dataset.a);
    if(action==='game-next') void controller.nextGameQuestion();
    if(action==='game-close') void controller.closeGame();
    if(action==='game-join') controller.openGameJoin();
    if(action==='game-resume') void controller.openLastGame();
    if(action==='game-refresh') void controller.refreshGame();
    if(action==='game-submit') void controller.submitGameAnswer();
    if(['create-quiz','my-quizzes','new-quiz','edit-quiz','profile','logout'].includes(action) && !canLeave()) return;
    if(action==='create-quiz') void controller.openQuizzes(true);
    if(action==='my-quizzes') void controller.openQuizzes();
    if(action==='quiz-actions') controller.openQuizActions(button.dataset.id);
    if(action==='close-quiz-actions') controller.closeQuizActions();
    if(action==='quiz-sessions') void controller.openQuizSessions(button.dataset.id);
    if(action==='session-results') void controller.openSessionResults(button.dataset.id);
    if(action==='new-quiz') controller.newQuiz();
    if(action==='edit-quiz') void controller.loadQuiz(button.dataset.id);
    if(action==='quiz-save-draft') void controller.saveQuiz('draft');
    if(['quiz-add-question','quiz-remove-question','quiz-add-answer','quiz-remove-answer','quiz-remove-image'].includes(action)) controller.changeQuiz(action,qi,ai);
    if (button.dataset.action === 'logout') controller.logout();
    if (button.dataset.action === 'refresh') void controller.refresh();
    if (button.dataset.action === 'resend') void controller.resend();
    if (button.dataset.action === 'profile') void controller.openProfile();
    if (button.dataset.action === 'university') controller.openUniversity(button.dataset.universityId);
    if (button.dataset.action === 'admins') void controller.openAdmins();
    if (button.dataset.action === 'people') controller.openPeople();
    if (button.dataset.action === 'more-people') void controller.searchPeople('', true);
    if (button.dataset.action === 'person-contact') controller.openPerson(button.dataset.membershipId);
    if (button.dataset.action === 'admin-contact') controller.openAdmin(button.dataset.membershipId);
    if (button.dataset.action === 'close-contact') controller.closeAdmin();
    if (button.dataset.toggle) {
      const input = root.querySelector('#' + button.dataset.toggle);
      const show = input.type === 'password';
      input.type = show ? 'text' : 'password';
      button.textContent = show ? 'Скрыть' : 'Показать';
      button.setAttribute('aria-pressed', String(show));
    }
  });
  void controller.start();
  return {controller, canLeave, destroy: () => {clearInterval(interval);controller.resetGameView();root.ownerDocument.defaultView.removeEventListener('beforeunload',beforeUnload);root.ownerDocument.defaultView.removeEventListener('pagehide',pageHide);root.ownerDocument.defaultView.removeEventListener('pageshow',pageShow);}};
}
if (typeof document !== 'undefined') {
  const root = document.getElementById('app');
  if (root) {
    let storage;
    try { storage = window.sessionStorage; } catch {}
    const app = mountApp(root, {storage});


    document.getElementById('account-nav')?.addEventListener('click', event => {
      if (app.controller.state.busy) return;
      if (!app.canLeave()) return;
      const action = event.target.closest('button')?.dataset.action;
      if (action === 'profile') void app.controller.openProfile();
      if (action === 'logout') void app.controller.logout();
    });
    document.querySelector('.brand')?.addEventListener('click', event => {
      event.preventDefault();
      if (!app.controller.state.busy && app.canLeave()) app.controller.navigate(app.controller.state.token ? 'account' : 'login');
    });
  }
}
