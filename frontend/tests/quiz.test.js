import test from 'node:test';
import assert from 'node:assert/strict';
import {newQuiz, questionComplete, quizPayload, renderQuiz} from '../quiz.js';
import {createController, createApi, ApiError} from '../app.js';
const uni='00000000-0000-4000-8000-000000000001';
function setup(api) {const c=createController({api});c.state.token='token';return c;}
const profile={university_position:[{role:'teacher',university_id:uni,university_name:'Вуз'}]};
test('draft defaults, inherited time and payload omit UI fields',()=>{
 const d=newQuiz(uni);assert.equal(d.default_time_seconds,60);assert.equal(d.questions.length,1);
 const q=d.questions[0];assert.equal(questionComplete(q,60),false);
 q.text='Q';q.answers[0].text='A';q.answers[0].is_correct=true;q.answers[1].image_id='image';
 assert.equal(questionComplete(q,60),true);assert.equal(questionComplete(q,0),false);
 q.time_seconds=90;assert.equal(questionComplete(q,0),true);
 d.id='quiz';d.revision=3;q.image_url='https://example.test';
 const p=quizPayload(d,'ready');assert.equal(p.revision,3);assert.equal(p.questions[0].time_seconds,90);assert.ok(!('image_url' in p.questions[0]));
});
test('teacher creates draft, updates same quiz, switches single/multy and enforces next question',async()=>{
 const calls=[];
 const c=setup(async(path,data,token,method)=>{calls.push({path,data,method});if(path==='profile')return profile;if(!data)return {quizzes:[]};return {quiz_id:'q1',revision:data.revision?2:1,status:data.status};});
 await c.openQuizzes(true);assert.equal(c.state.screen,'quiz');
 c.changeQuiz('quiz-add-question');assert.equal(c.state.quizDraft.questions.length,1);
 await c.saveQuiz('draft');assert.equal(c.state.quizDraft.id,'q1');assert.equal(c.state.quizDirty,false);
 c.editQuiz('text','Question',0);c.editQuiz('text','A',0,0);c.editQuiz('text','B',0,1);
 c.editQuiz('type','multy',0);c.editQuiz('is_correct',true,0,0);c.editQuiz('is_correct',true,0,1);
 c.editQuiz('type','single',0);assert.equal(c.state.quizDraft.questions[0].answers.filter(a=>a.is_correct).length,1);
 c.changeQuiz('quiz-add-question');assert.equal(c.state.quizDraft.questions.length,2);
 await c.saveQuiz('draft');assert.equal(calls.at(-1).method,'PUT');assert.equal(calls.at(-1).data.revision,1);
});
test('student cannot open editor and conflict preserves edits',async()=>{
 const denied=setup(async()=>({university_position:[{role:'student'}]}));assert.equal(await denied.openQuizzes(true),false);
 const c=setup(async(path,data)=>{if(path==='profile')return profile;if(!data)return {quizzes:[]};throw new ApiError('Conflict',409,'quiz_revision_conflict');});
 await c.openQuizzes(true);c.editQuiz('name','Keep me');await c.saveQuiz('draft');assert.equal(c.state.quizDraft.name,'Keep me');assert.equal(c.state.quizDirty,true);
});
test('editor escapes all author strings and uses circles/squares',()=>{
 const d=newQuiz(uni);d.name='<script>x</script>';d.questions[0].type='multy';
 const html=renderQuiz({screen:'quiz',quizDraft:d,quizUniversities:[]},'');assert.ok(!html.includes('<script>'));assert.match(html,/type="checkbox"/);assert.match(html,/Сохранить черновик/);
});
test('API sends PUT JSON and uploads FormData without overriding boundary',async()=>{
 const requests=[];const api=createApi(async(url,options)=>{requests.push({url,...options});return {ok:true,json:async()=>({})};});
 await api('quizzes/q',{},'token','PUT');assert.equal(requests[0].url,'/v1/quizzes/q');assert.equal(requests[0].method,'PUT');
 await api('media/images',new FormData(),'token');assert.equal(requests[1].headers['Content-Type'],undefined);
});

test('published quiz is read-only after loading and after publication',async()=>{
 const calls=[];
 const d=newQuiz(uni);d.id='q1';d.status='ready';d.revision=1;
 const c=setup(async(path,data)=>{
  calls.push(path);
  if(path==='profile')return profile;
  if(path==='quizzes/q1')return {quiz:structuredClone(d)};
  return {quiz_id:'q1',revision:1,status:'ready'};
 });
 await c.loadQuiz('q1');
 const before=structuredClone(c.state.quizDraft),count=calls.length;
 c.editQuiz('name','Changed');c.changeQuiz('quiz-add-answer',0);
 assert.equal(await c.saveQuiz('draft'),false);
 assert.equal(await c.uploadQuizImage(new Blob(['image'],{type:'image/png'}),0),false);
 assert.deepEqual(c.state.quizDraft,before);assert.equal(calls.length,count);
 const html=renderQuiz(c.state,'');
 assert.match(html,/Просмотр квиза/);assert.match(html,/<fieldset class="quiz-fields" disabled>/);
 assert.ok(!html.includes('quiz-save-draft'));assert.ok(!html.includes('type="submit"'));
 await c.openQuizzes(true);
 await c.saveQuiz('ready');
 assert.equal(c.state.quizDraft.status,'ready');
 assert.equal(await c.saveQuiz('draft'),false);
});
