import test from 'node:test';
import assert from 'node:assert/strict';
import {createController, createApi, ApiError, validatePassword, renderView, escapeHtml} from '../app.js';

const storage = (value = {}) => {
  const values = new Map(Object.entries(value));
  return {getItem: key => values.get(key), setItem: (k,v) => values.set(k,v), removeItem: k => values.delete(k), values};
};
const setup = (responses = []) => {
  const calls = [];
  const store = storage();
  let clock = 100000;
  const api = async (...args) => {
    calls.push(args);
    const result = responses.shift();
    if (result instanceof Error) throw result;
    return result;
  };
  const controller = createController({api, storage: store, now: () => clock});
  return {controller, calls, store, advance: ms => clock += ms};
};

test('registration → code → account; secrets are not persisted', async () => {
  const {controller: c, calls, store} = setup([
    {verification_id:'v1'}, {access_token:'jwt'}, {user_id:'u1'},
  ]);
  assert.equal(await c.register(' user@example.com ', 'password', 'password'), true);
  assert.equal(c.state.screen, 'code');
  assert.equal(c.remaining(), 60);
  assert.deepEqual(calls[0], ['register', {email:'user@example.com', password:'password', password_confirmation:'password'}]);
  assert.equal(await c.verify('123456'), true);
  assert.equal(c.state.screen, 'account');
  assert.equal(c.state.userId, 'u1');
  assert.equal(c.state.verificationId, '');
  const saved = JSON.parse(store.getItem('rumpelquiz.auth'));
  assert.equal(saved.token, 'jwt');
  assert.equal(saved.password, undefined);
  assert.equal(saved.resetToken, undefined);
  assert.equal(saved.code, undefined);
});
test('login loads protected account and logout clears credentials', async () => {
  const {controller:c,calls} = setup([{access_token:'jwt'}, {user_id:'u1'}, {success:true,university_position:[{role:'student'}]}]);
  await c.login('user@example.com', 'password');
  assert.deepEqual(calls[1], ['me', undefined, 'jwt']);
  await c.logout();
  assert.deepEqual(calls[2], ['profile', undefined, 'jwt']);
  assert.deepEqual(calls[3], ['logout', {}, 'jwt']);
  assert.equal(c.state.screen,'login');
  assert.equal(c.state.token,'');
  assert.equal(c.state.userId,'');
});
test('complete password reset uses the dedicated endpoints', async () => {
  const {controller:c,calls,store} = setup([
    {verification_id:'v1'}, {reset_token:'reset-secret'}, {success:true},
  ]);
  await c.requestReset('user@example.com');
  assert.equal(c.state.purpose,'reset');
  await c.verify('654321');
  assert.equal(c.state.screen,'password');
  assert.ok(!store.getItem('rumpelquiz.auth').includes('reset-secret'));
  await c.updatePassword('new-password','new-password');
  assert.deepEqual(calls.map(c=>c[0]), ['forgot-password/email-check','forgot-password/verify-code','forgot-password/update-password']);
  assert.equal(calls[2][1].reset_token,'reset-secret');
  assert.equal(c.state.screen,'login');
  assert.equal(c.state.resetToken,'');
  assert.match(c.state.success,/Пароль изменён/);
});
test('resend cooldown and id rotation', async () => {
  const {controller:c,calls,advance} = setup([{verification_id:'v1'},{verification_id:'v2'}]);
  await c.register('a@b.com','password','password');
  assert.equal(await c.resend(),false);
  assert.equal(calls.length,1);
  advance(60000);
  await c.resend();
  assert.equal(c.state.verificationId,'v2');
  assert.deepEqual(calls[1],['resend-code',{verification_id:'v1'}]);
  assert.equal(c.remaining(),60);
});
test('reset resend asks for another email code', async () => {
  const {controller:c,calls,advance} = setup([{verification_id:'v1'},{verification_id:'v2'}]);
  await c.requestReset('a@b.com'); advance(60000); await c.resend();
  assert.equal(calls[1][0],'forgot-password/email-check');
  assert.equal(c.state.purpose,'reset');
});
for (const [password,confirmation] of [['',''], ['a','b'], ['x'.repeat(73),'x'.repeat(73)], ['я'.repeat(37),'я'.repeat(37)], ['a\0b','a\0b']]) {
  test('invalid password prevents a network request: '+JSON.stringify(password), async () => {
    const {controller:c,calls} = setup();
    assert.equal(await c.register('a@b.com',password,confirmation),false);
    assert.equal(calls.length,0);
    assert.ok(c.state.error);
  });
}
test('72 byte passwords, including Unicode, are supported', () => {
  assert.equal(validatePassword('я'.repeat(36),'я'.repeat(36)),'');
  assert.equal(validatePassword('x'.repeat(72)),'');
});
test('duplicate submit is ignored', async () => {
  let resolve;
  let count=0;
  const c = createController({api: () => {count++; return new Promise(r=>resolve=r);}});
  const pending = c.register('a@b.com','password','password');
  assert.equal(await c.register('c@d.com','password','password'),false);
  resolve({verification_id:'v1'}); await pending;
  assert.equal(count,1);
  assert.equal(c.state.email,'a@b.com');
});
test('navigation ignores late responses from previous forms', async () => {
  let resolve;
  const c=createController({api:()=>new Promise(r=>resolve=r)});
  const pending=c.register('a@b.com','password','password');
  c.navigate('login');
  resolve({verification_id:'stale'}); await pending;
  assert.equal(c.state.screen,'login');
  assert.equal(c.state.verificationId,'');
  assert.equal(c.state.busy,false);
});
test('failed code can be retried without losing its id',async()=>{
  const {controller:c}=setup([{verification_id:'v1'},new ApiError('Неверный код',400)]);
  await c.requestReset('a@b.com'); await c.verify('123456');
  assert.equal(c.state.screen,'code');
  assert.equal(c.state.verificationId,'v1');
  assert.equal(c.state.error,'Неверный код');
  assert.equal(c.state.busy,false);
});
test('expired reset token returns to recovery start',async()=>{
  const {controller:c}=setup([{verification_id:'v1'},{reset_token:'expired'},new ApiError('Истёк',400,'invalid_or_expired_token')]);
  await c.requestReset('a@b.com'); await c.verify('123456'); await c.updatePassword('new','new');
  assert.equal(c.state.screen,'forgot');
  assert.equal(c.state.resetToken,'');
});
test('server cooldown is reflected in UI',async()=>{
  const {controller:c,advance}=setup([{verification_id:'v1'},new ApiError('Подождите',429,'resend_too_soon')]);
  await c.requestReset('a@b.com'); advance(60000); await c.resend();
  assert.equal(c.remaining(),60);
});
test('stored session is checked against backend',async()=>{
  const store=storage({'rumpelquiz.auth':JSON.stringify({token:'jwt',email:'a@b.com'})});
  const c=createController({storage:store,api:async()=>({user_id:'u1'})});
  await c.start(); assert.equal(c.state.screen,'account');
});
test('expired stored session returns to login',async()=>{
  const store=storage({'rumpelquiz.auth':JSON.stringify({token:'old'})});
  const c=createController({storage:store,api:async()=>{throw new ApiError('Истекла сессия',401);}});
  await c.start(); assert.equal(c.state.screen,'login'); assert.equal(c.state.token,'');
});
test('pending code survives reload, reset secret does not',()=>{
  const store=storage({'rumpelquiz.auth':JSON.stringify({email:'a@b.com',verificationId:'v1',purpose:'reset',resendAt:120000})});
  const c=createController({storage:store,now:()=>100000});
  assert.equal(c.state.screen,'code'); assert.equal(c.remaining(),20); assert.equal(c.state.resetToken,'');
});
test('disabled or corrupt browser storage does not block login',async()=>{
  for(const store of [storage({'rumpelquiz.auth':'invalid'}),{getItem(){throw Error()},setItem(){throw Error()}}]){
    const c=createController({storage:store,api:async path=>path==='login'?{access_token:'t'}:{user_id:'u'}});
    await c.login('a@b.com','password'); assert.equal(c.state.screen,'account');
  }
});
test('malformed successful response cannot authenticate',async()=>{
  const {controller:c}=setup([{}]);
  assert.equal(await c.login('a@b.com','password'),false);
  assert.equal(c.state.token,'');
  assert.ok(c.state.error);
});
test('request client sends JSON and bearer header',async()=>{
  let request;
  const api=createApi(async(path,options)=>{request={path,...options};return {ok:true,json:async()=>({success:true})};});
  await api('login',{email:'a@b.com'},'token');
  assert.equal(request.path,'/v1/auth/login');
  assert.equal(request.headers.Authorization,'Bearer token');
  assert.equal(request.headers['Content-Type'],'application/json');
  assert.equal(request.method,'POST');
  assert.deepEqual(JSON.parse(request.body),{email:'a@b.com'});
});
test('request client maps known errors and hides server internals',async()=>{
  const api=createApi(async()=>({ok:false,status:500,json:async()=>({details:'secret database connection'})}));
  await assert.rejects(api('login',{}), e=>e instanceof ApiError && !e.message.includes('secret') && e.status===500);
  const bad=createApi(async()=>({ok:false,status:400,json:async()=>({error:'passwords_do_not_match'})}));
  await assert.rejects(bad('register',{}),/Пароли не совпадают/);
});
test('request client handles non-JSON errors, disconnect and timeout',async()=>{
  const nonjson=createApi(async()=>({ok:false,status:502,json:async()=>{throw Error()}}));
  await assert.rejects(nonjson('me'),/Сервис временно/);
  const offline=createApi(async()=>{throw TypeError('Failed to fetch')});
  await assert.rejects(offline('me'),/связаться с сервером/);
  const timeout=createApi(async()=>{throw Object.assign(Error(),{name:'AbortError'})});
  await assert.rejects(timeout('me'),/не ответил вовремя/);
});
test('all screens render labels and never raw HTML or tokens',()=>{
  const state={email:'<img src=x onerror=alert(1)>',error:'<script>bad</script>',token:'secret-jwt',resetToken:'secret-reset',userId:'u',purpose:'reset'};
  for(const screen of ['login','register','forgot','code','password','account']){
    const html=renderView({...state,screen});
    assert.ok(html.includes('<h2'));
    assert.ok(!html.includes('<img src=x'));
    assert.ok(!html.includes('<script>bad'));
    assert.ok(!html.includes('secret-jwt'));
    assert.ok(!html.includes('secret-reset'));
    assert.ok(html.includes('role="alert"'));
  }
  assert.equal(escapeHtml('"><&'), '&quot;&gt;&lt;&amp;');
});
test('invalid code format is rejected locally', async()=>{
  const {controller:c,calls}=setup();
  for(const code of ['','12345','1234567','abcdef']) await c.verify(code);
  assert.equal(calls.length,0);
});

test('switching from account to recovery clears the previous tab session', async () => {
  const {controller:c, store} = setup([{access_token:'jwt'}, {user_id:'u1'}]);
  await c.login('a@b.com','password');
  c.navigate('forgot');
  assert.equal(c.state.token,'');
  assert.equal(c.state.userId,'');
  assert.equal(JSON.parse(store.getItem('rumpelquiz.auth')).token,'');
});
