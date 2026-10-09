import test from 'node:test';
import assert from 'node:assert/strict';
import {createController, ApiError, renderView} from '../app.js';

const profile = roles => ({success:true,university_position:roles.map(role=>({role,university_id:'uni'}))});
const actions = state => [...renderView({screen:'account',...state}).matchAll(/data-action="([^"]+)"/g)].map(match=>match[1]);
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

test('student home has exactly the two requested actions, including with a saved game',()=>{
 const state={accountRoles:['student'],game:{session:{id:'game'}}};
 assert.deepEqual(actions(state),['game-join','available-tests']);
 const html=renderView({screen:'account',...state});
 assert.match(html,/Подключиться к сессии/);assert.match(html,/Посмотреть назначенные тесты/);
 assert.ok(!html.includes('Создать'));assert.ok(!html.includes('game-resume'));
});

for(const roles of [['teacher'],['admin'],['student','teacher'],['student','admin']]) {
 test('author home has exactly four actions for '+roles.join(', '),()=>{
  const state={accountRoles:roles,game:{session:{id:'game'}}};
  assert.deepEqual(actions(state),['create-quiz','my-quizzes','create-test','my-tests']);
  const html=renderView({screen:'account',...state});
  for(const label of ['Создать квиз','Посмотреть квизы','Создать тест','Посмотреть тесты']) assert.ok(html.includes(label));
  assert.ok(!html.includes('Подключиться к сессии'));assert.ok(!html.includes('назначенные тесты'));assert.ok(!html.includes('game-resume'));
 });
}

test('unknown, missing and loading roles never show student or author actions',()=>{
 assert.deepEqual(actions({accountRoles:null,busy:true}),[]);
 assert.deepEqual(actions({accountRoles:[]}),[]);
 assert.deepEqual(actions({accountRoles:['unknown']}),[]);
 assert.deepEqual(actions({accountRoles:null,accountRolesError:'No network'}),['account-roles']);
 assert.match(renderView({screen:'account',accountRoles:[]}),/Роль в учебном заведении пока не назначена/);
 assert.match(renderView({screen:'account',accountRoles:null,busy:true}),/Загружаем роль/);
});

test('login fetches roles using the verified token and logout clears them',async()=>{
 const {c,calls}=signIn(['student']);
 assert.equal(await c.login('student@example.com','password'),true);
 assert.deepEqual(calls.map(c=>c.path),['login','me','profile']);
 assert.equal(calls.at(-1).token,'token');assert.deepEqual(c.state.accountRoles,['student']);
 assert.deepEqual(actions(c.state),['game-join','available-tests']);
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
 assert.equal(c.state.accountRolesError,'');assert.deepEqual(actions(c.state),['game-join','available-tests']);
});

test('inactive and unsupported roles are excluded; fresh profile updates the home',async()=>{
 const c=createController({api:async()=>({success:true,university_position:[{role:'teacher',status:'inactive'},{role:'student',status:'active'},{role:'unknown'}]})});
 c.state.token='token';c.state.screen='account';c.state.accountRoles=['teacher'];
 await c.openProfile();assert.deepEqual(c.state.accountRoles,['student']);
 c.navigate('account');assert.deepEqual(actions(c.state),['game-join','available-tests']);
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
