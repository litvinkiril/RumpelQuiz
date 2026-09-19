import test from 'node:test';
import assert from 'node:assert/strict';
import {createApi, createController, ApiError, renderView, safeAvatarUrl} from '../app.js';
const profile = {success:true,email:'test@example.com',first_name:'Кирилл',last_name:'Литвин',middle_name:'Андреевич',avatar_url:null,university_position:[{university_id:'hse',university_name:'ВШЭ',role:'admin',group_id:null,group_name:null},{university_id:'mirea',university_name:'МИРЭА',role:'student',group_id:'group',group_name:'ИКБО-01-24'}]};
test('profile uses user endpoint with bearer token',async()=>{
 let request; const api=createApi(async(path,opts)=>{request={path,...opts};return {ok:true,json:async()=>profile};});
 await api('profile',undefined,'token');
 assert.equal(request.path,'/v1/user/profile'); assert.equal(request.method,'GET'); assert.equal(request.headers.Authorization,'Bearer token');
});
test('profile navigation loads data and returns without logging out',async()=>{
 const calls=[]; const c=createController({api:async(path,...args)=>{calls.push([path,...args]);return path==='login'?{access_token:'jwt'}:path==='me'?{user_id:'u'}:profile;}});
 await c.login('test@example.com','test'); await c.openProfile();
 assert.equal(c.state.screen,'profile'); assert.deepEqual(c.state.profile,profile);
 assert.deepEqual(calls.at(-1),['profile',undefined,'jwt']);
 c.navigate('account'); assert.equal(c.state.token,'jwt'); assert.equal(c.state.screen,'account');
});
test('missing user/profile errors use requested message; server errors remain distinct',async()=>{
 for(const error of ['user_not_found','user_profile_not_found']){
 const api=createApi(async()=>({ok:false,status:404,json:async()=>({error})}));
 await assert.rejects(api('profile'),e=>e.message==='Пользователь удален');
 }
});
test('late profile response cannot reopen screen after leaving',async()=>{
 let resolve; const c=createController({api:()=>new Promise(r=>resolve=r)});
 c.state.token='jwt'; const pending=c.openProfile(); c.navigate('account');
 resolve(profile); await pending; assert.equal(c.state.screen,'account'); assert.equal(c.state.profile,null);
});
test('failed profile request supports retry',async()=>{
 let count=0; const c=createController({api:async()=>{if(!count++)throw new ApiError('Нет сети');return profile;}});
 c.state.token='jwt'; await c.openProfile(); assert.equal(c.state.error,'Нет сети');
 await c.openProfile(); assert.deepEqual(c.state.profile,profile); assert.equal(c.state.error,'');
});
test('profile renders roles, null avatar, escaped data and empty memberships',()=>{
 const html=renderView({screen:'profile',profile});
 assert.match(html,/Администратор/); assert.match(html,/Студент/); assert.match(html,/ИКБО-01-24/);
 assert.ok(!html.includes('<img'));
 const malicious=renderView({screen:'profile',profile:{...profile,first_name:'<script>x</script>',avatar_url:'javascript:alert(1)',university_position:[]}});
 assert.ok(!malicious.includes('<script>'));assert.ok(!malicious.includes('<img'));assert.match(malicious,/Пока нет привязок/);
 assert.equal(safeAvatarUrl('javascript:alert(1)'),'');
});
test('quiz placeholder is disabled',()=>{assert.match(renderView({screen:'account'}),/quiz-button" disabled/);});
