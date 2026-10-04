import test from 'node:test';
import assert from 'node:assert/strict';
import {createController, renderView, ApiError} from '../app.js';

const quiz={id:'quiz',name:'<Квиз>',status:'ready'};
const session={session_id:'session',name:'<Сессия>',host_name:'<Ведущий>',status:'finished',created_at_ms:1000,participants_count:1,question_count:3};
const results=[{name:'<Студент>',answered_count:2,correct_count:1,score:1,question_count:3}];
function setup(api) {
  const c=createController({api,storage:null});
  Object.assign(c.state,{token:'token',screen:'quizzes',quizzes:[quiz,{id:'draft',status:'draft'}]});
  return c;
}

test('published quiz opens a local action dialog, drafts retain editing',async()=>{
  const calls=[];
  const c=setup(async path=>{calls.push(path);return {quiz:{...quiz,questions:[]}};});
  c.openQuizActions('quiz');
  let html=renderView(c.state);
  assert.match(html,/data-quiz-dialog/);
  assert.match(html,/Создать сессию/);
  assert.match(html,/Посмотреть старые сессии/);
  assert.match(html,/data-action="edit-quiz"\s+data-id="draft"/);
  assert.deepEqual(calls,[]);
  assert.ok(!html.includes('<Квиз>'));
  c.closeQuizActions();assert.equal(c.state.selectedQuiz,null);
  c.openQuizActions('draft');assert.equal(c.state.selectedQuiz,null);
  c.openQuizActions('quiz');
  await c.loadQuiz('quiz');
  assert.equal(c.state.screen,'quiz');assert.equal(c.state.selectedQuiz,null);
  assert.deepEqual(calls,['quizzes/quiz']);
  assert.match(renderView(c.state),/data-form="game-create"/);
});

test('history opens selected session results and supports return to list',async()=>{
  const calls=[];
  const c=setup(async path=>{calls.push(path);return path==='quizzes/quiz/sessions'?{success:true,sessions:[session]}:{success:true,session_id:'session',results};});
  c.openQuizActions('quiz');
  assert.equal(await c.openQuizSessions('quiz'),true);
  assert.equal(c.state.screen,'quiz-sessions');assert.equal(c.state.selectedQuiz,null);
  assert.match(renderView(c.state),/Завершена/);
  assert.ok(!renderView(c.state).includes('<Ведущий>'));
  assert.equal(await c.openSessionResults('session'),true);
  const html=renderView(c.state);
  assert.equal(c.state.screen,'quiz-results');
  assert.match(html,/Правильных/);assert.match(html,/Баллы/);assert.match(html,/2 \/ 3/);
  assert.ok(!html.includes('<Студент>'));
  await c.openQuizSessions();
  assert.deepEqual(calls,['quizzes/quiz/sessions','game/sessions/session/results','quizzes/quiz/sessions']);
});

test('history handles empty lists, failed results, retry and cancelled sessions',async()=>{
  let fail=true;
  const c=setup(async path=>{
    if(path.startsWith('quizzes/')) return {success:true,sessions:[]};
    if(fail) throw new ApiError('Не удалось загрузить');
    return {success:true,session_id:'session',results:[]};
  });
  await c.openQuizSessions('quiz');assert.match(renderView(c.state),/Старых сессий пока нет/);
  c.state.quizSessions=[{...session,status:'cancelled'}];
  assert.equal(await c.openSessionResults('session'),false);
  assert.match(renderView(c.state),/Повторить загрузку/);
  fail=false;assert.equal(await c.openSessionResults(),true);
  assert.match(renderView(c.state),/Сессия отменена/);
  assert.match(renderView(c.state),/нет участников/);
});

test('history ignores duplicate clicks and late responses after navigation',async()=>{
  let resolve;
  const c=setup(()=>new Promise(r=>{resolve=r;}));
  const pending=c.openQuizSessions('quiz');
  assert.equal(await c.openQuizSessions('quiz'),false);
  c.navigate('account');resolve({success:true,sessions:[session]});
  assert.equal(await pending,false);assert.equal(c.state.screen,'account');assert.equal(c.state.quizSessions,null);
  c.state.screen='quiz-sessions';c.state.quizSessions=[session];
  const result=c.openSessionResults('session');
  c.navigate('account');resolve({success:true,session_id:'session',results});
  assert.equal(await result,false);assert.equal(c.state.sessionResults,null);
});

test('malformed or unrelated results never populate a session',async()=>{
  const c=setup(async()=>({success:true,session_id:'another',results}));
  c.state.quizSessions=[session];
  assert.equal(await c.openSessionResults('session'),false);
  assert.equal(c.state.sessionResults,null);
});
