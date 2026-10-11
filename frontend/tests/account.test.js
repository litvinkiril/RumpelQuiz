import test from 'node:test';
import assert from 'node:assert/strict';
import {createController, ApiError, renderView} from '../app.js';

const profile = roles => ({success:true,university_position:roles.map(role=>({role,university_id:'uni'}))});
const actions = (state,screen='quiz-section') => [...renderView({...state,screen}).matchAll(/data-action="([^"]+)"/g)].map(match=>match[1]);
const signIn = (roles, options={}) => {
 const calls=[];
 const c=createController({...options,api:async(path,data,token)=>{
  calls.push({path,data,token});
  if(path==='login' || path==='verify-email') return {access_token:'token'};
  if(path==='me') return {user_id:'user'};
  if(path==='profile') return profile(roles);
  if(path==='logout') return {success:true};
  throw new Error(path);
 }});
 return {c,calls};
};

test('student sections offer quiz and test participation, including with a saved game',()=>{
 const state={accountRoles:['student'],game:{session:{id:'game'}}};
 assert.deepEqual(actions(state),['game-join']);
 assert.deepEqual(actions(state,'test-section'),['available-tests']);
 const html=renderView({screen:'account',...state});
 assert.match(html,/Добро пожаловать в/);assert.match(html,/Подключиться/);assert.match(html,/data-action="profile"/);
 assert.ok(!html.includes('Создать'));assert.ok(!html.includes('game-resume'));
});

for(const roles of [['teacher'],['admin'],['student','teacher'],['student','admin']]) {
 test('author sections offer creation and the quiz catalog for '+roles.join(', '),()=>{
  const state={accountRoles:roles,game:{session:{id:'game'}}};
  assert.deepEqual(actions(state),['create-quiz','quiz-catalog']);
  assert.deepEqual(actions(state,'test-section'),['create-test','test-catalog']);
  const tests=renderView({screen:'test-section',...state});
  assert.match(tests,/data-action="test-catalog"/);
  assert.doesNotMatch(tests,/Скоро|Поиск по тестам скоро появится/);
  const html=renderView({screen:'account',...state});
  assert.match(html,/data-nav="quiz-section"/);assert.match(html,/data-action="profile"/);
  assert.ok(!html.includes('Подключиться к сессии'));assert.ok(!html.includes('назначенные тесты'));assert.ok(!html.includes('game-resume'));
 });
}

test('unknown, missing and loading roles never show student or author actions',()=>{
 assert.deepEqual(actions({accountRoles:null,busy:true}),[]);
 assert.deepEqual(actions({accountRoles:[]}),['profile']);
 assert.deepEqual(actions({accountRoles:['unknown']}),['profile']);
 assert.deepEqual(actions({accountRoles:null,accountRolesError:'No network'}),['account-roles']);
 assert.match(renderView({screen:'quiz-section',accountRoles:[]}),/Роль в учебном заведении пока не назначена/);
 assert.match(renderView({screen:'quiz-section',accountRoles:null,busy:true}),/Загружаем роль/);
});

test('login fetches roles using the verified token and logout clears them',async()=>{
 const {c,calls}=signIn(['student']);
 assert.equal(await c.login('student@example.com','password'),true);
 assert.deepEqual(calls.map(c=>c.path),['login','me','profile']);
 assert.equal(calls.at(-1).token,'token');assert.deepEqual(c.state.accountRoles,['student']);
 assert.deepEqual(actions(c.state),['game-join']);
 await c.logout();assert.equal(c.state.accountRoles,null);
});

test('registration verification loads teacher roles before opening the home',async()=>{
 const {c}=signIn(['teacher']);c.state.verificationId='verification';
 assert.equal(await c.verify('123456'),true);
 assert.equal(c.state.screen,'account');assert.deepEqual(c.state.accountRoles,['teacher']);
});

test('restored sessions load roles from the server and ignore stored role claims',async()=>{
 const storage={getItem:()=>JSON.stringify({token:'stored-token',accountRoles:['teacher']}),setItem:()=>{}};
 const {c,calls}=signIn(['student'],{storage});
 assert.equal(c.state.accountRoles,null);assert.equal(await c.start(),true);
 assert.deepEqual(calls.map(c=>c.path),['me','profile']);
 assert.deepEqual(c.state.accountRoles,['student']);
});

test('a failed role request preserves login, hides menus and supports a retry',async()=>{
 let failed=true;
 const c=createController({api:async(path)=>{
  if(path==='login') return {access_token:'token'};
  if(path==='me') return {user_id:'user'};
  if(path==='profile') {if(failed) throw new ApiError('No network');return profile(['student']);}
 }});
 assert.equal(await c.login('user@example.com','password'),true);
 assert.equal(c.state.token,'token');assert.equal(c.state.screen,'account');
 assert.deepEqual(actions(c.state),['account-roles']);
 failed=false;assert.equal(await c.reloadAccountRoles(),true);
 assert.equal(c.state.accountRolesError,'');assert.deepEqual(actions(c.state),['game-join']);
});

test('inactive and unsupported roles are excluded; fresh profile updates the home',async()=>{
 const c=createController({api:async()=>({success:true,university_position:[{role:'teacher',status:'inactive'},{role:'student',status:'active'},{role:'unknown'}]})});
 c.state.token='token';c.state.screen='account';c.state.accountRoles=['teacher'];
 await c.openProfile();assert.deepEqual(c.state.accountRoles,['student']);
 c.navigate('account');assert.deepEqual(actions(c.state),['game-join']);
});

test('late role reload cannot change a signed-out or different account',async()=>{
 let resolve;
 const c=createController({api:()=>new Promise(r=>{resolve=r;})});
 c.state.token='token';c.state.screen='account';c.state.accountRoles=['teacher'];
 const pending=c.reloadAccountRoles();c.navigate('login');resolve(profile(['teacher']));
 assert.equal(await pending,false);assert.equal(c.state.screen,'login');assert.equal(c.state.accountRoles,null);
});

test('expired authorization during role loading clears the existing session',async()=>{
 const c=createController({api:async()=>{throw new ApiError('Expired',401);}});
 c.state.token='old';c.state.screen='account';c.state.accountRoles=['teacher'];
 await c.reloadAccountRoles();assert.equal(c.state.screen,'login');assert.equal(c.state.token,'');assert.equal(c.state.accountRoles,null);
});
