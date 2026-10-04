import test from 'node:test';
import assert from 'node:assert/strict';
import {createController, createApi, ApiError} from '../app.js';
import {consumeGameStream, gameSeconds, joinLink, qrSvg, renderGame} from '../game.js';
import {newQuiz, renderQuiz} from '../quiz.js';

const sid='11111111-1111-4111-8111-111111111111',qid='22222222-2222-4222-8222-222222222222',quiz='33333333-3333-4333-8333-333333333333';
const a='44444444-4444-4444-8444-444444444444',b='55555555-5555-4555-8555-555555555555';
const snapshot=(host=true,question=null,status='waiting')=>({
  success:true,server_time_ms:10000,session:{id:sid,quiz_id:quiz,name:'Квиз',join_code:'012345',status,is_host:host,question_count:2,participants_count:0},
  current_question:question,participants:[],results:[],
});
const question=()=>({id:qid,position:0,text:'Вопрос',type:'single',opened_at_ms:10000,deadline_at_ms:70000,has_next:true,accepting_answers:true,submitted:false,selected_ids:[],answered_count:0,answers:[{id:a,text:'A'},{id:b,text:'B'}]});
function setup(api, options={}) {
  const map=options.map || new Map();
  const c=createController({api,storage:{getItem:key=>map.get(key),setItem:(key,value)=>map.set(key,value),removeItem:key=>map.delete(key)},now:()=>1000,makeGameId:()=>sid,...options});
  c.state.token='token'; c.state.userId='host';
  c.state.quizDraft={id:quiz,status:'ready'}; c.state.screen='quiz';
  return {c,map};
}

test('game API paths use v1/game and bearer auth',async()=>{
  let request;
  await createApi(async(url,options)=>{request={url,options};return {ok:true,json:async()=>({})};})('game/sessions/'+sid,undefined,'token');
  assert.equal(request.url,'/v1/game/sessions/'+sid);
  assert.equal(request.options.headers.Authorization,'Bearer token');
});
test('SSE parser handles split frames and skips heartbeat comments',async()=>{
  const frames=[];
  const body=new ReadableStream({start(controller){
    controller.enqueue(new TextEncoder().encode(': ping\n\nevent: question\nda'));
    controller.enqueue(new TextEncoder().encode('ta: {"id":"q"}\n\n'));
    controller.close();
  }});
  await consumeGameStream({body},(type,data)=>frames.push([type,data]));
  assert.deepEqual(frames,[['question',{id:'q'}]]);
});
test('SSE sends a new question without a state GET',async()=>{
  const calls=[],requested=[];
  const payload='event: snapshot\ndata: '+JSON.stringify(snapshot(false))+'\n\n'
    +'event: question\ndata: '+JSON.stringify({server_time_ms:11000,current_question:question()})+'\n\n';
  const streamFetch=async(url,options)=>{
    requested.push({url,authorization:options.headers.Authorization});
    return {ok:true,status:200,body:new ReadableStream({start(stream){stream.enqueue(new TextEncoder().encode(payload));stream.close();}})};
  };
  const {c}=setup(async(...args)=>{calls.push(args);return snapshot(false);},{streamFetch});
  c.state.game=snapshot(false);c.state.screen='game';c.syncGameEvents();
  await new Promise(resolve=>setImmediate(resolve));
  assert.equal(c.state.game.current_question.id,qid);
  assert.deepEqual(calls,[]);
  assert.deepEqual(requested[0],{url:'/v1/game/sessions/'+sid+'/events',authorization:'Bearer token'});
  c.navigate('account');c.syncGameEvents();
});

test('final SSE results stop the stream without a GET, reconnection or later events',async()=>{
  for(const status of ['finished','cancelled']) {
    let requests=0,cancelled=false;
    const final=snapshot(false,null,status);
    final.results=[{name:'Первый',answered_count:2,correct_count:1,score:1},{name:'Второй',answered_count:0,correct_count:0,score:0}];
    const streamFetch=async()=>{
      requests++;
      return {ok:true,status:200,body:new ReadableStream({start(stream){
        stream.enqueue(new TextEncoder().encode('event: results\ndata: '+JSON.stringify(final)+'\n\nevent: question\ndata: '+JSON.stringify({server_time_ms:12000,current_question:question()})+'\n\n'));
      },cancel(){cancelled=true;}})};
    };
    const {c}=setup(async()=>{assert.fail('Results must arrive over SSE');},{streamFetch});
    c.state.screen='game';c.state.game=snapshot(false);
    c.syncGameEvents();await new Promise(resolve=>setImmediate(resolve));
    c.syncGameEvents();await new Promise(resolve=>setImmediate(resolve));
    assert.deepEqual(c.state.game,final);assert.equal(requests,1);assert.equal(cancelled,true);
    assert.match(renderGame(c.state,''),/Второй/);
    assert.match(renderGame(c.state,''),/Баллы/);
  }
});
test('successful answer POST updates the student locally without a state GET',async()=>{
  const calls=[];
  const {c}=setup(async(path,body)=>{calls.push({path,body});return {success:true};});
  c.state.game=snapshot(false,question(),'running');c.state.screen='game';c.state.gameOffset=9000;
  c.chooseGameAnswer(a);
  assert.equal(await c.submitGameAnswer(),true);
  assert.equal(calls.length,1);
  assert.equal(calls[0].path,'game/sessions/'+sid+'/answers');
  assert.equal(c.state.game.current_question.submitted,true);
});
test('SSE authorization failure clears the expired login',async()=>{
  const {c}=setup(async()=>{throw new ApiError('expired',401);},
    {streamFetch:async()=>({status:401,ok:false})});
  c.state.game=snapshot(false);c.state.screen='game';c.syncGameEvents();
  await new Promise(resolve=>setImmediate(resolve));
  assert.equal(c.state.screen,'login');
  assert.equal(c.state.token,'');
});
test('create uses stable UUID after a lost response or failed state load',async()=>{
  const calls=[];let phase=0;
  const {c}=setup(async(path,body)=>{
    calls.push({path,body:structuredClone(body)});
    if(body) {
      if(phase++===0) throw new ApiError('offline');
      return {success:true,session:{id:sid,join_code:'012345'}};
    }
    if(phase===2) throw new ApiError('state offline');
    return snapshot();
  });
  assert.equal(await c.createGame('Практика 3'),false);
  assert.equal(await c.createGame('Практика 3'),false);
  assert.equal(await c.createGame('Практика 3'),true);
  assert.deepEqual(calls.filter(x=>x.body).map(x=>x.body.session_id),[sid,sid,sid]);
  assert.equal(c.state.screen,'game');
});
test('draft cannot create a game and published quiz shows launch outside disabled form',async()=>{
  let calls=0;const {c}=setup(async()=>{calls++;});
  c.state.quizDraft.status='draft';assert.equal(await c.createGame('Практика 3'),false);assert.equal(calls,0);
  const draft=newQuiz('uni');draft.id=quiz;draft.status='ready';
  const html=renderQuiz({screen:'quiz',quizDraft:draft},'');
  assert.ok(html.indexOf('game-create')<html.indexOf('<fieldset'));
});
test('duplicate clicks are ignored and stale next request refreshes without advancing again',async()=>{
  const calls=[];let release;
  const {c}=setup(async(path,body)=>{
    calls.push({path,body});
    if(body) {await new Promise(resolve=>{release=resolve;});throw new ApiError('stale',409,'session_state_changed');}
    return snapshot(true,question(),'running');
  });
  c.state.game=snapshot();c.state.screen='game';
  const first=c.nextGameQuestion();
  assert.equal(await c.nextGameQuestion(),false);
  release();assert.equal(await first,true);
  assert.equal(calls.length,2);assert.deepEqual(calls[0].body,{expected_question_id:null});
  assert.equal(c.state.game.current_question.id,qid);
});
test('next is disabled on last question and closed sessions cannot be advanced',async()=>{
  let calls=0;const {c}=setup(async()=>{calls++;});
  c.state.game=snapshot(true,{...question(),has_next:false},'running');
  assert.equal(await c.nextGameQuestion(),false);
  c.state.game=snapshot(true,null,'finished');
  assert.equal(await c.closeGame(),false);assert.equal(calls,0);
});
test('close refreshes final state and keeps results',async()=>{
  const calls=[];const {c}=setup(async(path,body)=>{calls.push({path,body});return body?{success:true}:snapshot(true,null,'finished');});
  c.state.game=snapshot();c.state.screen='game';
  assert.equal(await c.closeGame(),true);
  assert.deepEqual(calls[0],{path:'game/sessions/'+sid+'/close',body:{}});
  assert.equal(c.state.game.session.status,'finished');
});
test('game restores from server after reload only for the same account',async()=>{
  const map=new Map([['rumpelquiz.game',JSON.stringify({owner:'host',sessionId:sid,code:'012345'})]]);
  let calls=0;const {c}=setup(async()=>{calls++;return snapshot(true,question(),'running');},{map});
  assert.equal(await c.resumeGame(),true);assert.equal(c.state.game.current_question.id,qid);
  c.state.userId='another';c.resetGameView();c.state.screen='account';
  await c.resumeGame();assert.equal(calls,1);assert.equal(c.state.game,null);
});
test('late game response cannot reopen screen after navigation',async()=>{
  let release;const {c}=setup(()=>new Promise(resolve=>{release=resolve;}));
  c.state.game=snapshot();c.state.screen='game';
  const pending=c.refreshGame();c.navigate('account');release(snapshot());
  assert.equal(await pending,false);assert.equal(c.state.screen,'account');
});
test('background polling leaves controls enabled and does not redraw unchanged questions',async()=>{
  let release, renders=0;
  const {c}=setup(()=>new Promise(resolve=>{release=resolve;}),{onChange:()=>renders++});
  c.state.game=snapshot(false,question(),'running');c.state.screen='game';
  const pending=c.pollGame();
  assert.equal(c.state.busy,false);assert.equal(renders,0);
  assert.equal(await c.pollGame(),false);
  c.chooseGameAnswer(a);assert.deepEqual(c.state.gameSelected,[a]);
  const before=renders;
  release({...snapshot(false,question(),'running'),server_time_ms:11000});
  assert.equal(await pending,true);assert.equal(renders,before);
  assert.deepEqual(c.state.gameSelected,[a]);assert.equal(c.state.gameOffset,10000);
});

test('background progress preserves images and selections despite renewed signed URLs',async()=>{
  const changes=[];
  const original=snapshot(true,{...question(),image_url:'https://images.test/question?old',
    answers:[{id:a,text:'A',image_url:'https://images.test/answer?old'},{id:b,text:'B'}]},'running');
  const next=structuredClone(original);
  next.server_time_ms=11000;
  next.current_question.image_url='https://images.test/question?new';
  next.current_question.answers[0].image_url='https://images.test/answer?new';
  next.current_question.answered_count=1;
  next.session.participants_count=2;
  next.participants=[{id:'student',name:'<Студент>'}];
  const {c}=setup(async()=>next,{onChange:(state,change)=>changes.push(change)});
  c.state.game=original;c.state.screen='game';c.state.gameSelected=[a];
  assert.equal(await c.pollGame(),true);
  assert.deepEqual(changes,['game-progress']);
  assert.equal(c.state.game.current_question.answered_count,1);
  assert.equal(c.state.game.session.participants_count,2);
  assert.deepEqual(c.state.game.participants,next.participants);
  assert.equal(c.state.game.current_question.image_url,original.current_question.image_url);
  assert.equal(c.state.game.current_question.answers,original.current_question.answers);
  assert.deepEqual(c.state.gameSelected,[a]);
  assert.equal(c.state.game.server_time_ms,11000);
  assert.equal(c.state.gameOffset,10000);
  next.server_time_ms=12000;
  next.current_question.image_url='https://images.test/question?renewed-again';
  assert.equal(await c.pollGame(),true);
  assert.deepEqual(changes,['game-progress']);
  assert.equal(c.state.game.server_time_ms,12000);
});

test('lobby joins use progress updates before a question is opened',async()=>{
  const next=snapshot();next.session.participants_count=1;next.participants=[{name:'Студент'}];
  const changes=[];
  const {c}=setup(async()=>next,{onChange:(state,change)=>changes.push(change)});
  c.state.game=snapshot();c.state.screen='game';
  await c.pollGame();
  assert.deepEqual(changes,['game-progress']);
  assert.equal(c.state.game.current_question,null);
  assert.deepEqual(c.state.game.participants,next.participants);
});

test('question, closure and student submission changes still accept the full state',async()=>{
  for(const update of [
    value=>{value.current_question.id=b;value.current_question.position=1;},
    value=>{value.session.status='finished';value.results=[{name:'Студент',answered_count:1}];},
    value=>{value.session.status='cancelled';},
    value=>{value.current_question.submitted=true;value.current_question.selected_ids=[b];},
    value=>{value.current_question.accepting_answers=false;},
  ]) {
    const next=snapshot(false,question(),'running');update(next);
    const changes=[];
    const {c}=setup(async()=>next,{onChange:(state,change)=>changes.push(change)});
    c.state.game=snapshot(false,question(),'running');c.state.screen='game';
    await c.pollGame();
    assert.deepEqual(changes,[undefined]);
    assert.equal(c.state.game,next);
    if(next.current_question.submitted) assert.deepEqual(c.state.gameSelected,[b]);
  }
});

test('SSE reconnect snapshots use the same progress path as resync',async()=>{
  const next=snapshot(true,{...question(),answered_count:1},'running');
  next.server_time_ms=11000;
  const changes=[];
  const streamFetch=async()=>({ok:true,status:200,body:new ReadableStream({start(stream){
    stream.enqueue(new TextEncoder().encode('event: snapshot\ndata: '+JSON.stringify(next)+'\n\n'));
    stream.close();
  }})});
  const {c}=setup(async()=>next,{streamFetch,onChange:(state,change)=>changes.push(change)});
  c.state.game=snapshot(true,question(),'running');c.state.screen='game';
  c.syncGameEvents();
  await new Promise(resolve=>setImmediate(resolve));
  c.resetGameView();
  assert.deepEqual(changes,['game-progress']);
});
test('answer submission supersedes a pending poll without losing the accepted answer',async()=>{
  let releasePoll,reads=0;
  const {c}=setup(async(path,body)=>{
    if(body) return {success:true};
    if(reads++===0) return new Promise(resolve=>{releasePoll=resolve;});
    return snapshot(false,{...question(),submitted:true,selected_ids:[a]},'running');
  });
  c.state.game=snapshot(false,question(),'running');c.state.screen='game';c.state.gameSelected=[a];
  const pending=c.pollGame();
  assert.equal(await c.submitGameAnswer(),true);
  releasePoll(snapshot(false,question(),'running'));
  assert.equal(await pending,false);assert.equal(c.state.game.current_question.submitted,true);
  assert.deepEqual(c.state.gameSelected,[a]);
});
test('poll and answer share one refresh when access expires during interaction',async()=>{
  let releaseRefresh,refreshes=0;
  const {c,map}=setup(async(path,body,token)=>{
    if(path==='refresh') {
      refreshes++;
      return new Promise(resolve=>{releaseRefresh=resolve;});
    }
    if(token==='token') throw new ApiError('expired',401);
    return body?{success:true}:snapshot(false,{...question(),submitted:true,selected_ids:[a]},'running');
  });
  c.state.refreshToken='old-refresh';c.state.sessionId='auth-session';
  c.state.game=snapshot(false,question(),'running');c.state.screen='game';c.state.gameSelected=[a];
  const polling=c.pollGame();
  await new Promise(resolve=>setImmediate(resolve));
  const submission=c.submitGameAnswer();
  await new Promise(resolve=>setImmediate(resolve));
  assert.equal(refreshes,1);
  releaseRefresh({access_token:'new-access',refresh_token:'new-refresh',session_id:'auth-session'});
  assert.equal(await submission,true);assert.equal(await polling,false);
  assert.equal(c.state.game.current_question.submitted,true);
  assert.equal(JSON.parse(map.get('rumpelquiz.auth')).refreshToken,'new-refresh');
});
test('QR join intent survives authentication and join validates six digits',async()=>{
  const calls=[];const {c}=setup(async(path,body)=>{
    calls.push({path,body});return body?{success:true,session_id:sid}:snapshot(false);
  },{joinCode:'012345'});
  await c.resumeGame();assert.equal(c.state.screen,'game-join');
  assert.equal(await c.joinGame('123'),false);assert.equal(calls.length,0);
  assert.equal(await c.joinGame('012345'),true);
  assert.deepEqual(calls[0].body,{code:'012345'});assert.equal(c.state.game.session.is_host,false);
});
test('single/multiple answers, deadline and submission restore',async()=>{
  const calls=[];const q=question();
  const {c}=setup(async(path,body)=>{calls.push({path,body});return body?{success:true}:snapshot(false,{...q,submitted:true,selected_ids:[b]},'running');});
  c.state.game=snapshot(false,q,'running');c.state.gameOffset=9000;c.state.screen='game';
  c.chooseGameAnswer(a);c.chooseGameAnswer(b);assert.deepEqual(c.state.gameSelected,[b]);
  assert.equal(await c.submitGameAnswer(),true);
  assert.deepEqual(calls[0].body,{question_id:qid,answer_ids:[b]});
  c.chooseGameAnswer(a);assert.deepEqual(c.state.gameSelected,[b]);
  assert.equal(await c.submitGameAnswer(),false);
  c.state.game.current_question={...q,type:'multy'};c.state.gameSelected=[];
  c.chooseGameAnswer(a);c.chooseGameAnswer(b);assert.deepEqual(c.state.gameSelected,[a,b]);
  c.chooseGameAnswer(a,false);assert.deepEqual(c.state.gameSelected,[b]);
  c.state.game.current_question.deadline_at_ms=9999;
  assert.equal(await c.submitGameAnswer(),false);
});
test('countdown is based on server offset and never negative',()=>{
  const state={game:snapshot(false,question(),'running'),gameOffset:9000};
  assert.equal(gameSeconds(state,1000),60);
  assert.equal(gameSeconds(state,62000),0);
});
test('QR is local SVG, link is encoded and dangerous origins are rejected',()=>{
  const link=joinLink('http://192.168.1.50:8080/something','012345');
  assert.equal(link,'http://192.168.1.50:8080/?join=012345');
  const svg=qrSvg(link);assert.match(svg,/<svg/);assert.match(svg,/<path/);
  assert.doesNotMatch(svg,/<script|https?:\/\/[^"]+\.png/);
  for(const origin of ['javascript:alert(1)','ftp://example.com','https://user:secret@example.com'])
    assert.throws(()=>joinLink(origin,'012345'));
});
test('game view escapes names and hides host controls from students',()=>{
  const game=snapshot(false,{...question(),text:'<script>bad</script>'},'running');
  const html=renderGame({screen:'game',game,gameSelected:[],gameOffset:0},'');
  assert.ok(!html.includes('<script>'));assert.ok(!html.includes('data-action="game-next"'));
  assert.ok(!html.includes('data-action="game-close"'));assert.match(html,/game-submit/);
});


test('session name is required and validated before a request',async()=>{
  let calls=0;const {c}=setup(async()=>{calls++;});
  for(const name of [undefined,null,123,'','   ','\u00a0\u2003','a'.repeat(201),'a\0b']) {
    assert.equal(await c.createGame(name),false);
  }
  assert.equal(calls,0);
});
test('creation trims name, preserves it on retries and replaces ID for a changed name',async()=>{
  const calls=[];let id=0;
  const {c}=setup(async(path,body)=>{calls.push(structuredClone(body));throw new ApiError('offline');},{makeGameId:()=>String(++id)});
  await c.createGame('  Практика 3  ');
  await c.createGame('Практика 3');
  await c.createGame('Практика 4');
  assert.deepEqual(calls.map(x=>x.name),['Практика 3','Практика 3','Практика 4']);
  assert.deepEqual(calls.map(x=>x.session_id),['1','1','2']);
});
test('200 Unicode characters are allowed',async()=>{
  let sent;
  const {c}=setup(async(path,body)=>{sent=body;throw new ApiError('offline');});
  await c.createGame('😀'.repeat(200));
  assert.equal(sent.name,'😀'.repeat(200));
});
