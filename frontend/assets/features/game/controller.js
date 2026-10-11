import { KEY, UUID, finished, makeGameId, joinLink, gameSeconds } from './model.js';
import { consumeGameStream } from './stream.js';
export function createGameController({
  state,
  authorized,
  run,
  emit,
  fail,
  storage,
  streamFetch = globalThis.fetch,
  now = Date.now,
  makeId = makeGameId,
  origin = 'http://localhost:8080',
  joinCode = '',
}) {
  let polling = false;
  let streamAbort = null,
    streamSession = '',
    streamTask = null;
  let saved = {};
  let presenceSession = '',
    presenceToken = '',
    presenceClient = '',
    presenceSequence = 0,
    presenceAt = 0;
  const sendPresence = (online) => {
    if (!presenceSession || !presenceToken || !presenceClient) return;
    const body = { client_id: presenceClient, sequence: ++presenceSequence, online };
    // keepalive also permits the final authenticated report during pagehide.
    try {
      void Promise.resolve(
        streamFetch('/v1/game/sessions/' + encodeURIComponent(presenceSession) + '/presence', {
          method: 'POST',
          headers: { Authorization: 'Bearer ' + presenceToken, 'Content-Type': 'application/json' },
          body: JSON.stringify(body),
          keepalive: true,
        }),
      ).catch(() => {});
    } catch {}
  };
  const acceptPresence = (presence) => {
    if (!state.game?.session.is_host || !Array.isArray(presence)) return false;
    const valid = presence.filter(
      (p) => typeof p?.user_id === 'string' && typeof p.online === 'boolean',
    );
    const previous = new Map((state.game.presence || []).map((p) => [p.user_id, p.online]));
    const alerts = [...(state.gameAlerts || [])];
    for (const p of valid) {
      const participant = state.game.participants.find((person) => person.user_id === p.user_id);
      if (!participant || previous.get(p.user_id) === p.online) continue;
      if (!p.online || previous.get(p.user_id) === false)
        alerts.unshift({ name: participant.name, online: p.online, time: now() });
    }
    const changed = JSON.stringify(state.game.presence || []) !== JSON.stringify(valid);
    state.game = { ...state.game, presence: valid };
    state.gameAlerts = alerts.slice(0, 20);
    return changed;
  };
  try {
    const value = JSON.parse(storage?.getItem(KEY) || '{}');
    if (value && typeof value === 'object') saved = value;
  } catch {}
  Object.assign(state, {
    game: null,
    gameSelected: [],
    gameAlerts: [],
    gameOffset: 0,
    gameOrigin: origin,
    gameJoinCode: /^\d{6}$/.test(joinCode) ? joinCode : '',
  });
  const remember = (patch) => {
    saved = { ...saved, ...patch, owner: state.userId };
    try {
      storage?.setItem(KEY, JSON.stringify(saved));
    } catch {}
  };
  const validate = (value) => {
    if (
      !value?.success ||
      !UUID.test(value.session?.id || '') ||
      !['waiting', 'running', 'finished', 'cancelled'].includes(value.session.status) ||
      !Number.isFinite(value.server_time_ms)
    )
      throw new Error('Invalid game state');
  };
  const accept = (value) => {
    validate(value);
    const previous = state.game?.current_question?.id;
    if (state.game?.session.id !== value.session.id) state.gameAlerts = [];
    if (value.session.is_host) {
      const old = state.game;
      state.game = { ...value, presence: old?.session.id === value.session.id ? old.presence : [] };
      acceptPresence(value.presence || []);
    }
    state.game = value;
    state.gameOffset = value.server_time_ms - now();
    const question = value.current_question;
    if (question?.submitted) state.gameSelected = [...question.selected_ids];
    else if (previous !== question?.id) state.gameSelected = [];
    state.screen = 'game';
    remember({ sessionId: value.session.id, code: value.session.join_code, pending: null });
  };
  const acceptBackground = (value) => {
    validate(value);
    const previous = state.game;
    // Signed image URLs can change on every read. Only progress may change
    // without replacing the question; submission/deadline changes still render.
    const content = (game) => {
      const { server_time_ms, participants, presence, session, current_question, ...rest } = game;
      const { participants_count, ...sessionContent } = session;
      let questionContent = null;
      if (current_question) {
        const { answered_count, image_url, answers, ...question } = current_question;
        questionContent = {
          ...question,
          answers: answers.map(({ image_url, ...answer }) => answer),
        };
      }
      return JSON.stringify({
        ...rest,
        session: sessionContent,
        current_question: questionContent,
      });
    };
    if (previous && !finished(value) && content(previous) === content(value)) {
      const changed =
        previous.session.participants_count !== value.session.participants_count ||
        previous.current_question?.answered_count !== value.current_question?.answered_count ||
        JSON.stringify(previous.participants) !== JSON.stringify(value.participants);
      state.game = {
        ...previous,
        server_time_ms: value.server_time_ms,
        session: { ...previous.session, participants_count: value.session.participants_count },
        participants: value.participants,
        current_question: previous.current_question
          ? { ...previous.current_question, answered_count: value.current_question.answered_count }
          : null,
      };
      const presenceChanged = acceptPresence(value.presence || []);
      state.gameOffset = value.server_time_ms - now();
      return changed || presenceChanged ? 'game-progress' : false;
    }
    accept(value);
  };
  const get = (id) => authorized('game/sessions/' + encodeURIComponent(id));
  const load = async (id) => {
    const value = await get(id);
    return value;
  };
  const controller = {
    resetGameView() {
      state.game = null;
      state.gameSelected = [];
      state.gameAlerts = [];
      controller.syncGameEvents();
    },
    syncGamePresence() {
      const id =
        state.screen === 'game' &&
        state.token &&
        state.game &&
        !state.game.session.is_host &&
        !finished(state.game)
          ? state.game.session.id
          : '';
      if (id !== presenceSession) {
        if (presenceSession) sendPresence(false);
        presenceSession = id;
        presenceClient = id ? makeId() : '';
        presenceSequence = 0;
        presenceAt = 0;
      }
      presenceToken = state.token;
      if (id && now() >= presenceAt) {
        sendPresence(true);
        presenceAt = now() + 10000;
      }
    },
    suspendGamePresence() {
      sendPresence(false);
      presenceAt = 0;
    },
    syncGameEvents() {
      controller.syncGamePresence();
      const id =
        state.screen === 'game' && state.token && state.game && !finished(state.game)
          ? state.game.session.id
          : '';
      if (id === streamSession && streamTask) return;
      streamAbort?.abort();
      streamAbort = null;
      streamTask = null;
      streamSession = id;
      if (!id) return;
      const abort = new AbortController();
      streamAbort = abort;
      const active = () =>
        !abort.signal.aborted &&
        state.screen === 'game' &&
        state.token &&
        state.game?.session.id === id &&
        !finished(state.game);
      const task = (async () => {
        while (active()) {
          try {
            const response = await streamFetch(
              '/v1/game/sessions/' + encodeURIComponent(id) + '/events',
              {
                headers: { Authorization: 'Bearer ' + state.token, Accept: 'text/event-stream' },
                signal: abort.signal,
              },
            );
            if (response.status === 401) {
              try {
                await authorized('me');
              } catch {
                state.token = '';
                state.refreshToken = '';
                state.sessionId = '';
                state.userId = '';
                state.game = null;
                state.screen = 'login';
                state.error = 'Сессия входа истекла. Войдите снова.';
                emit();
                abort.abort();
                break;
              }
              continue;
            }
            if (response.status === 403) {
              state.error = 'Нет доступа к этой игровой сессии.';
              emit();
              abort.abort();
              break;
            }
            if (!response.ok) throw new Error('Game event stream failed');
            await consumeGameStream(response, async (type, value) => {
              while (active() && state.busy)
                await new Promise((resolve) => setTimeout(resolve, 50));
              if (!active()) return;
              if (type === 'heartbeat') {
                controller.syncGamePresence();
              } else if (type === 'snapshot' || type === 'results') {
                if (
                  !value?.success ||
                  value.session?.id !== id ||
                  !Number.isFinite(value.server_time_ms)
                )
                  return;
                if (type === 'results' && (!finished(value) || !Array.isArray(value.results)))
                  return;
                if (state.game?.server_time_ms > value.server_time_ms) return;
                const change = acceptBackground(value),
                  hadError = !!state.error;
                state.error = '';
                if (hadError || change !== false) emit(hadError ? undefined : change);
                if (finished(value)) return false;
              } else if (type === 'question') {
                const q = value?.current_question,
                  old = state.game?.current_question;
                if (!q || !Number.isInteger(q.position) || !Number.isFinite(value.server_time_ms))
                  return;
                if (old && q.position <= old.position) return;
                accept({
                  ...state.game,
                  server_time_ms: value.server_time_ms,
                  session: { ...state.game.session, status: 'running' },
                  current_question: q,
                });
                state.error = '';
                emit();
              } else if (type === 'resync') {
                await controller.pollGame();
              } else if (type === 'presence' && state.game.session.is_host) {
                if (acceptPresence(value)) emit('game-progress');
              }
            });
          } catch (error) {
            if (!active()) break;
          }
          if (active()) await new Promise((resolve) => setTimeout(resolve, 1000));
        }
      })();
      streamTask = task;
      void task.finally(() => {
        if (streamAbort === abort) {
          streamAbort = null;
          streamTask = null;
          streamSession = '';
        }
      });
    },
    async resumeGame() {
      if (!state.token || state.busy) return false;
      if (
        state.gameJoinCode &&
        (saved.owner !== state.userId || saved.code !== state.gameJoinCode)
      ) {
        state.screen = 'game-join';
        emit();
        return true;
      }
      if (saved.owner === state.userId && UUID.test(saved.sessionId || '')) {
        state.screen = 'game';
        emit();
        return run(() => load(saved.sessionId), accept);
      }
      return true;
    },
    openGameJoin() {
      if (state.busy || !state.token) return;
      state.screen = 'game-join';
      state.error = '';
      state.success = '';
      emit();
    },
    openLastGame() {
      return controller.resumeGame();
    },
    createGame(name) {
      if (state.busy || !state.token || state.quizDraft?.status !== 'ready')
        return Promise.resolve(false);
      name =
        typeof name === 'string'
          ? name.replace(/^[\p{White_Space}\uFEFF]+|[\p{White_Space}\uFEFF]+$/gu, '')
          : '';
      if (!name || [...name].length > 200 || name.includes('\0'))
        return fail('Введите название сессии от 1 до 200 символов.');
      const quiz = state.quizDraft;
      return run(async () => {
        if (
          saved.owner !== state.userId ||
          saved.pending?.quiz_id !== quiz.id ||
          saved.pending?.name !== name
        ) {
          remember({
            sessionId: null,
            code: null,
            pending: { quiz_id: quiz.id, session_id: makeId(), name },
          });
        }
        const result = await authorized('game/sessions', saved.pending);
        if (!result?.success || !UUID.test(result.session?.id || ''))
          throw new Error('Invalid game creation');
        remember({ sessionId: result.session.id, code: result.session.join_code });
        return load(result.session.id);
      }, accept);
    },
    joinGame(code) {
      if (state.busy || !state.token) return Promise.resolve(false);
      code = code.trim();
      if (!/^\d{6}$/.test(code)) return fail('Введите шестизначный код сессии.');
      state.gameJoinCode = code;
      return run(async () => {
        const result = await authorized('game/sessions/join', { code });
        if (!result?.success || !UUID.test(result.session_id || ''))
          throw new Error('Invalid join');
        remember({ sessionId: result.session_id, code, pending: null });
        return load(result.session_id);
      }, accept);
    },
    refreshGame() {
      if (state.busy || !state.token || state.screen !== 'game') return Promise.resolve(false);
      const id = state.game?.session.id || (saved.owner === state.userId ? saved.sessionId : null);
      if (!UUID.test(id || '')) return Promise.resolve(false);
      return run(() => load(id), accept);
    },
    async pollGame() {
      if (polling || state.screen !== 'game' || state.busy || !state.game) return false;
      polling = true;
      try {
        return await run(() => load(state.game.session.id), acceptBackground, { background: true });
      } finally {
        polling = false;
      }
    },
    nextGameQuestion() {
      const game = state.game;
      if (
        state.busy ||
        !state.token ||
        !game?.session.is_host ||
        finished(game) ||
        game.current_question?.has_next === false
      )
        return Promise.resolve(false);
      return run(async () => {
        try {
          await authorized('game/sessions/' + game.session.id + '/next', {
            expected_question_id: game.current_question?.id || null,
          });
        } catch (error) {
          if (
            !['session_state_changed', 'session_closed', 'no_more_questions'].includes(error.code)
          )
            throw error;
        }
        return load(game.session.id);
      }, accept);
    },
    closeGame() {
      const game = state.game;
      if (state.busy || !state.token || !game?.session.is_host || finished(game))
        return Promise.resolve(false);
      return run(async () => {
        await authorized('game/sessions/' + game.session.id + '/close', {});
        return load(game.session.id);
      }, accept);
    },
    chooseGameAnswer(id, checked = true) {
      const q = state.game?.current_question;
      if (
        state.busy ||
        state.game?.session.is_host ||
        !q ||
        q.submitted ||
        gameSeconds(state, now()) === 0 ||
        !q.answers.some((a) => a.id === id)
      )
        return;
      state.gameSelected =
        q.type === 'single'
          ? [id]
          : checked
            ? [...new Set([...state.gameSelected, id])]
            : state.gameSelected.filter((a) => a !== id);
      emit();
    },
    submitGameAnswer() {
      const game = state.game,
        q = game?.current_question;
      if (
        state.busy ||
        !state.token ||
        game?.session.is_host ||
        !q ||
        q.submitted ||
        !state.gameSelected.length ||
        gameSeconds(state, now()) === 0
      )
        return Promise.resolve(false);
      const choices = [...state.gameSelected];
      return run(
        async () => {
          try {
            await authorized('game/sessions/' + game.session.id + '/answers', {
              question_id: q.id,
              answer_ids: choices,
            });
            return { saved: true };
          } catch (error) {
            if (!['answer_already_saved', 'question_closed', 'session_closed'].includes(error.code))
              throw error;
            return { state: await load(game.session.id) };
          }
        },
        (result) => {
          if (result.saved && state.game?.current_question?.id === q.id) {
            state.game = {
              ...state.game,
              current_question: {
                ...state.game.current_question,
                submitted: true,
                selected_ids: choices,
              },
            };
            state.gameSelected = choices;
          } else if (result.state) accept(result.state);
        },
      );
    },
    setGameOrigin(value) {
      try {
        joinLink(value, state.game?.session.join_code);
        state.gameOrigin = new URL(value).origin;
        state.error = '';
        emit();
      } catch {
        return fail('Укажите адрес сайта с http:// или https://, без логина и пароля.');
      }
    },
  };
  return controller;
}
