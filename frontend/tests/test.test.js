import test from 'node:test';
import assert from 'node:assert/strict';
import {createController, createApi, ApiError, renderView} from '../app.js';
import {newTest, testQuestionComplete, testPayload, renderTests, testSeconds} from '../test.js';
const uni='00000000-0000-4000-8000-000000000001';
const id='00000000-0000-4000-8000-000000000002';
const profile={university_position:[{role:'teacher',university_id:uni,university_name:'Вуз'}]};
const setup=(api,options={})=>{const c=createController({api,...options});c.state.token='token';return c;};
const play=()=>({success:true,test:{id,name:'Тест'},attempt:{id:'attempt',status:'in_progress',started_at_ms:100000,deadline_at_ms:160000},server_now_ms:110000,question_count:2,answered_count:0,
 current_question:{id:'q1',position:0,text:'Q',type:'single',answers:[{id:'a1',text:'A'},{id:'a2',text:'B'}]}});

test('test payload has one whole-test time and excludes question timers and UI data',()=>{
 const d=newTest(uni);assert.equal(d.time_to_complete,60);assert.ok(!('time_seconds' in d.questions[0]));
 const q=d.questions[0];q.text='Q';q.answers[0].text='A';q.answers[0].is_correct=true;q.answers[1].image_id=id;
 assert.equal(testQuestionComplete(q),true);q.time_seconds=1;q.image_url='https://example.com';d.id=id;d.revision=3;
 const p=testPayload(d,'private');assert.equal(p.time_to_complete,60);assert.equal(p.revision,3);assert.equal(p.status,'private');
 assert.ok(!('time_seconds' in p.questions[0]));assert.ok(!('image_url' in p.questions[0]));assert.ok(!('default_time_seconds' in p));
});

test('teacher saves draft, edits single/multy, publishes private and publication blocks changes',async()=>{
 const calls=[];const c=setup(async(path,data,token,method)=>{
  calls.push({path,data,method});if(path==='profile')return profile;if(!data)return {tests:[]};
  return {test_id:id,revision:data.revision?2:1,status:data.status};
 });
 await c.openTests(true);assert.equal(c.state.screen,'test');
 c.changeTest('test-add-question');assert.equal(c.state.testDraft.questions.length,1);
 await c.saveTest('draft');assert.equal(c.state.testDraft.id,id);assert.equal(c.state.testDirty,false);
 c.editTest('text','Q',0);c.editTest('text','A',0,0);c.editTest('text','B',0,1);
 c.editTest('type','multy',0);c.editTest('is_correct',true,0,0);c.editTest('is_correct',true,0,1);
 c.editTest('type','single',0);assert.equal(c.state.testDraft.questions[0].answers.filter(a=>a.is_correct).length,1);
 c.changeTest('test-add-question');assert.equal(c.state.testDraft.questions.length,2);
 await c.saveTest('private');assert.equal(calls.at(-1).method,'PUT');assert.equal(calls.at(-1).data.revision,1);
 const d=structuredClone(c.state.testDraft),count=calls.length;
 c.editTest('name','changed');c.changeTest('test-add-answer',0);
 assert.equal(await c.saveTest('draft'),false);assert.equal(await c.uploadTestImage(new Blob(['x'],{type:'image/png'}),0),false);
 assert.deepEqual(c.state.testDraft,d);assert.equal(calls.length,count);assert.match(c.state.testShareUrl,/\?test=/);
});

test('student cannot author and authoring errors preserve edits and field details',async()=>{
 const denied=setup(async()=>({university_position:[{role:'student'}]}));assert.equal(await denied.openTests(true),false);
 const c=setup(async(path,data)=>{if(path==='profile')return profile;if(!data)return {tests:[]};throw new ApiError('Conflict',409,'test_revision_conflict',[{field:'name',message:'bad'}]);});
 await c.openTests(true);c.editTest('name','keep');assert.equal(await c.saveTest('public'),false);
 assert.equal(c.state.testDraft.name,'keep');assert.equal(c.state.testDirty,true);assert.equal(c.state.testErrors[0].field,'name');
});

test('test timers must be whole positive seconds before saving',async()=>{
 let calls=0;const c=setup(async()=>{calls++;return {};});c.state.screen='test';c.state.testDraft=newTest(uni);
 for(const time of [0,-1,'',1.5,2147483648,'oops']) {c.state.testDraft.time_to_complete=time;assert.equal(await c.saveTest('draft'),false);}
 assert.equal(calls,0);
});

test('test API uses protected /v1/tests routes and teacher/editor rendering escapes data',async()=>{
 const requests=[];const api=createApi(async(url,options)=>{requests.push({url,...options});return {ok:true,json:async()=>({})};});
 await api('tests',{},'token','POST');await api('tests/'+id+'/answers',{},'token');await api('tests/available',undefined,'token');
 assert.deepEqual(requests.map(r=>r.url),['/v1/tests','/v1/tests/'+id+'/answers','/v1/tests/available']);
 assert.equal(requests[1].headers.Authorization,'Bearer token');
 const d=newTest(uni);d.name='<script>x</script>';d.questions[0].type='multy';
 let html=renderTests({screen:'test',testDraft:d},'');assert.ok(!html.includes('<script>'));assert.match(html,/type="checkbox"/);
 assert.ok(!html.includes('time_seconds'));assert.ok(!html.includes('game-create'));assert.match(html,/Опубликовать private/);
 d.status='private';html=renderTests({screen:'test',testDraft:d,testShareUrl:'https://example.com/?test='+id},'');
 assert.match(html,/<fieldset class="quiz-fields" disabled>/);assert.ok(!html.includes('test-save-draft'));assert.match(html,/Результаты учеников/);
 assert.match(renderView({screen:'test-section',accountRoles:['teacher']}),/Создать тест/);assert.match(renderView({screen:'test-section',accountRoles:['student']}),/Пройти тест/);
});

test('student starts/resumes by link, keeps server deadline and sends selected answers',async()=>{
 const calls=[];let server=play(),time=9000000;
 const c=setup(async(path,data)=>{
  calls.push({path,data});if(path==='tests/available')return {tests:[]};
  if(path.endsWith('/answers'))server={...server,answered_count:1,current_question:{...server.current_question,id:'q2',position:1}};
  return structuredClone(server);
 },{now:()=>time});
 await c.openAvailableTests();await c.startTest('https://example.test/?test='+id);assert.equal(c.state.screen,'test-play');
 assert.equal(testSeconds(c.state,time),50);time+=10000;assert.equal(testSeconds(c.state,time),40);
 c.chooseTestAnswer('a1',true);assert.equal(await c.submitTestAnswer(),true);
 assert.deepEqual(calls.at(-1).data,{question_id:'q1',answer_ids:['a1']});assert.equal(c.state.testPlay.current_question.id,'q2');assert.deepEqual(c.state.testChoices,[]);
 assert.equal(c.state.testPlay.attempt.deadline_at_ms,160000);
});

test('lost answer response preserves choices so retry uses the same answer',async()=>{
 let sends=0;const submitted=[];
 const c=setup(async(path,data)=>{
  if(path.endsWith('/answers')) {submitted.push(data);if(++sends===1)throw new ApiError('Network');return {...play(),answered_count:1,current_question:{...play().current_question,id:'q2'}};}
  return play();
 },{now:()=>0});
 await c.startTest(id);c.chooseTestAnswer('a1',true);assert.equal(await c.submitTestAnswer(),false);
 assert.deepEqual(c.state.testChoices,['a1']);assert.equal(await c.submitTestAnswer(),true);assert.deepEqual(submitted[0],submitted[1]);
});

test('expiry refreshes terminal result instead of accepting another answer',async()=>{
 let time=0;const paths=[];const expired={...play(),attempt:{...play().attempt,status:'expired'},current_question:null,result:{score:0}};
 const c=setup(async(path)=>{paths.push(path);return path.endsWith('/progress')?expired:play();},{now:()=>time});
 await c.startTest(id);c.chooseTestAnswer('a1',true);time=50000;
 assert.equal(testSeconds(c.state,time),0);await c.submitTestAnswer();assert.ok(!paths.some(p=>p.endsWith('/answers')));
 assert.equal(c.state.testPlay.attempt.status,'expired');assert.match(renderTests(c.state,''),/Время истекло/);
});

test('server deadline race updates terminal state after a conflict',async()=>{
 const c=setup(async(path)=>{
  if(path.endsWith('/answers'))throw new ApiError('finished',409,'test_attempt_finished');
  if(path.endsWith('/progress'))return {...play(),attempt:{...play().attempt,status:'expired'},current_question:null,result:{score:0}};
  return play();
 },{now:()=>0});
 await c.startTest(id);c.chooseTestAnswer('a1',true);await c.submitTestAnswer();assert.equal(c.state.testPlay.attempt.status,'expired');
});

test('duplicate requests are ignored and late responses cannot reopen the test',async()=>{
 let resolve,calls=0;const c=setup(()=>{calls++;return new Promise(r=>{resolve=r;});});
 const pending=c.startTest(id);assert.equal(await c.startTest(id),false);assert.equal(calls,1);
 c.navigate('account');resolve(play());assert.equal(await pending,false);assert.equal(c.state.screen,'account');assert.equal(c.state.testPlay,undefined);
});

test('private link intent survives login without starting the timer',async()=>{
 const calls=[];const c=createController({testLink:id,api:async(path)=>{
  calls.push(path);if(path==='login')return {access_token:'token'};if(path==='me')return {user_id:'student'};if(path==='tests/available')return {tests:[]};throw new Error(path);
 }});
 await c.login('student@example.com','password');assert.equal(c.state.screen,'available-tests');assert.equal(c.state.testLink,id);
 assert.ok(!calls.some(p=>p.endsWith('/start')));
});

test('multiple selections and progress refresh preserve only current-question choices',async()=>{
 const server=play();server.current_question.type='multy';const c=setup(async()=>structuredClone(server),{now:()=>0});
 await c.startTest(id);c.chooseTestAnswer('a1',true);c.chooseTestAnswer('a2',true);c.chooseTestAnswer('foreign',true);
 assert.deepEqual(c.state.testChoices,['a1','a2']);await c.refreshTest();assert.deepEqual(c.state.testChoices,['a1','a2']);
 c.chooseTestAnswer('a1',false);assert.deepEqual(c.state.testChoices,['a2']);
});

test('results escape participant names and remain isolated from game sessions',async()=>{
 const c=setup(async()=>({results:[{student_id:id,name:'<img src=x onerror=alert(1)>',status:'completed',score:1,question_count:2,answered_count:2}]}));
 await c.openTestResults(id);assert.equal(c.state.screen,'test-results');const html=renderTests(c.state,'');
 assert.ok(!html.includes('<img src=x'));assert.match(html,/1 \/ 2/);assert.equal(c.state.game,null);
});

test('signing out and expired authentication clear test content and results',async()=>{
 const c=setup(async()=>({}));
 Object.assign(c.state,{testDraft:newTest(uni),testPlay:play(),testResults:[{student_id:id}],testUniversities:profile.university_position});
 await c.logout();assert.equal(c.state.testDraft,null);assert.equal(c.state.testPlay,null);assert.equal(c.state.testResults,null);assert.deepEqual(c.state.testUniversities,[]);
 const expired=setup(async()=>{throw new ApiError('expired',401);});
 expired.state.testPlay=play();expired.state.testResults=[{student_id:id}];
 await expired.openAvailableTests();assert.equal(expired.state.screen,'login');assert.equal(expired.state.testPlay,null);assert.equal(expired.state.testResults,null);
});
