import {newAnswer} from './quiz.js';
export function createTestController({state,authorized,run,emit,fail,now,origin,ApiError,testLink=''}) {
 let linkOpened=false;
 const authorUniversities=profile=>[...new Map((profile.university_position || []).filter(p=>['teacher','admin'].includes(p.role)).map(p=>[p.university_id,{id:p.university_id,name:p.university_name}])).values()];
 const mutable=()=>!state.busy && state.screen==='test' && state.testDraft?.status==='draft';
 const applyPlay=value=>{
  if(!value?.success || !value.attempt || !value.test) throw new ApiError('Не удалось загрузить прохождение.');
  if(state.testPlay?.current_question?.id!==value.current_question?.id) state.testChoices=[];
  state.testPlay=value;state.testReceivedAt=now();state.screen='test-play';
 };
 const controller={
  resetTestView() {
   Object.assign(state,{testDraft:null,testPlay:null,testChoices:[],testResults:null,testResultsId:null,
    tests:[],availableTests:[],testUniversities:[],testDirty:false,testErrors:[],testUploading:false,testShareUrl:'',testLink:''});
  },
  openTests(create=false) {
   if(!state.token || state.busy) return Promise.resolve(false);
   return run(async()=>{
    const universities=authorUniversities(await authorized('profile'));
    if(!universities.length) throw new ApiError('Создавать тесты могут преподаватели и администраторы выбранного вуза.');
    return {universities,tests:(await authorized('tests')).tests};
   },result=>{
    state.testUniversities=result.universities;state.tests=result.tests;state.testErrors=[];state.testDirty=false;state.screen=create?'test':'tests';
    if(create) controller.newTest();
   });
  },
  newTest() {
   if(!state.testUniversities?.length) return;
   state.testDraft=newTest(state.testUniversities.length===1?state.testUniversities[0].id:'');
   state.testDirty=true;state.testErrors=[];state.error='';state.success='';state.screen='test';emit();
  },
  loadTest(id) {
   if(state.busy || !state.token) return Promise.resolve(false);
   return run(()=>authorized('tests/'+encodeURIComponent(id)),result=>{
    state.testDraft=result.test;state.testDirty=false;state.testErrors=[];state.screen='test';
    state.testShareUrl=origin+'/?test='+encodeURIComponent(id);
   });
  },
  editTest(field,value,qi,ai) {
   if(!mutable()) return;
   const d=state.testDraft,q=d.questions[qi],target=ai===undefined?(q || d):q?.answers[ai];
   if(!target) return;
   if(field==='is_correct' && q.type==='single') q.answers.forEach(a=>{a.is_correct=false;});
   target[field]=value;
   if(field==='type' && value==='single') {const first=q.answers.findIndex(a=>a.is_correct);q.answers.forEach((a,i)=>{a.is_correct=i===first;});}
   state.testDirty=true;state.testErrors=[];state.success='';
   if(field==='type') emit();
  },
  changeTest(action,qi,ai) {
   if(!mutable()) return;
   const d=state.testDraft,q=d.questions[qi];
   if(action==='test-add-question') {
    if(d.questions.length>=100 || !d.questions.every(testQuestionComplete)) return;
    d.questions.push(newTestQuestion());
   }
   if(action==='test-remove-question' && d.questions.length>1) d.questions.splice(qi,1);
   if(action==='test-add-answer' && q?.answers.length<20) q.answers.push(newAnswer());
   if(action==='test-remove-answer' && q?.answers.length>2) q.answers.splice(ai,1);
   if(action==='test-remove-image') {const target=ai===undefined?q:q?.answers[ai];if(target) Object.assign(target,{image_id:null,image_url:'',upload_error:''});}
   state.testDirty=true;state.testErrors=[];state.success='';emit();
  },
  saveTest(status) {
   if(!mutable()) return Promise.resolve(false);
   const d=state.testDraft;
   if(!d.university_id) return fail('Выберите учебное заведение.');
   const time=Number(d.time_to_complete);
   if(!Number.isInteger(time) || time<=0 || time>2147483647) return fail('Укажите целое положительное время на весь тест в секундах.');
   if(!['draft','public','private'].includes(status)) return Promise.resolve(false);
   return run(()=>authorized('tests'+(d.id?'/'+d.id:''),testPayload(d,status),d.id?'PUT':'POST'),result=>{
    d.id=result.test_id;d.revision=result.revision;d.status=result.status;state.testDirty=false;
    state.testShareUrl=origin+'/?test='+encodeURIComponent(d.id);
    state.success=result.status==='draft'?'Черновик сохранён.':'Тест опубликован. Студенты могут начать прохождение.';
   });
  },
  uploadTestImage(file,qi,ai) {
   if(!mutable() || !file) return Promise.resolve(false);
   const target=ai===undefined?state.testDraft.questions[qi]:state.testDraft.questions[qi]?.answers[ai];
   if(!target) return Promise.resolve(false);
   if(file.size>5242880) return fail('Максимальный размер картинки — 5 МиБ.');
   if(!['image/png','image/jpeg','image/webp'].includes(file.type)) return fail('Поддерживаются PNG, JPEG и WebP.');
   state.testUploading=true;target.upload_error='';const form=new FormData();form.append('file',file);
   return run(()=>authorized('media/images',form),result=>{
    target.image_id=result.media_id;target.image_url=result.image_url;state.testDirty=true;
   }).then(ok=>{state.testUploading=false;if(!ok && state.screen==='test') target.upload_error=state.error;emit();return ok;});
  },
  openAvailableTests() {
   if(!state.token || state.busy) return Promise.resolve(false);
   return run(()=>authorized('tests/available'),result=>{
    state.availableTests=result.tests;state.testLink=testLink;state.screen='available-tests';
   });
  },
  resumeTestLink() {
   if(!testLink || linkOpened) return Promise.resolve(false);
   linkOpened=true;return controller.openAvailableTests();
  },
  startTest(id) {
   if(!state.token || state.busy) return Promise.resolve(false);
   const text=String(id || '').trim();
   let testId=text;
   if(/^https?:/.test(text)) {try {testId=new URL(text).searchParams.get('test') || '';} catch {testId='';}}
   if(!/^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/i.test(testId)) return fail('Вставьте ссылку на тест или его идентификатор.');
   return run(()=>authorized('tests/'+encodeURIComponent(testId)+'/start',{}),applyPlay);
  },
  refreshTest() {
   if(!state.token || state.busy || state.screen!=='test-play' || !state.testPlay) return Promise.resolve(false);
   return run(()=>authorized('tests/'+encodeURIComponent(state.testPlay.test.id)+'/progress'),applyPlay);
  },
  chooseTestAnswer(id,checked) {
   if(state.busy || state.screen!=='test-play' || state.testPlay?.attempt.status!=='in_progress' || testSeconds(state,now())<=0) return;
   const q=state.testPlay.current_question;
   if(!q.answers.some(a=>a.id===id)) return;
   state.testChoices=q.type==='single'?(checked?[id]:[]):checked?[...new Set([...(state.testChoices || []),id])]:(state.testChoices || []).filter(a=>a!==id);
  },
  submitTestAnswer() {
   if(state.busy || state.screen!=='test-play' || state.testPlay?.attempt.status!=='in_progress') return Promise.resolve(false);
   if(testSeconds(state,now())<=0) return controller.refreshTest();
   if(!state.testChoices?.length) return fail('Выберите ответ.');
   const d=state.testPlay;
   return run(()=>authorized('tests/'+encodeURIComponent(d.test.id)+'/answers',{question_id:d.current_question.id,answer_ids:[...state.testChoices]}),applyPlay)
    .then(async ok=>{if(!ok && state.screen==='test-play' && state.errorCode==='test_attempt_finished') await controller.refreshTest();return ok;});
  },
  openTestResults(id=state.testResultsId) {
   if(state.busy || !state.token || !id) return Promise.resolve(false);
   return run(()=>authorized('tests/'+encodeURIComponent(id)+'/results'),result=>{
    state.testResults=result.results;state.testResultsId=id;state.screen='test-results';
   });
  },
 };
 return controller;
}
export function testSeconds(state,now=Date.now()) {
 const d=state.testPlay;
 if(d?.attempt.status!=='in_progress') return 0;
 return Math.max(0,Math.ceil((d.attempt.deadline_at_ms-d.server_now_ms-Math.max(0,now-(state.testReceivedAt ?? now)))/1000));
}
const statusName=s=>({draft:'Черновик',public:'Public',private:'Private',in_progress:'В процессе',completed:'Завершён',expired:'Время истекло'})[s] || s;
function showImage(url) {return /^https:\/\//.test(url || '')?`<img src="${escape(url)}" alt="Картинка" referrerpolicy="no-referrer">`:'';}
export function renderTests(state,notice) {
 if(state.screen==='test') return renderTestEditor(state,notice);
 if(state.screen==='tests') return `<button class="text-button back" data-nav="account">← На главную</button><h2 tabindex="-1">Мои тесты</h2>${notice}<button class="primary" data-action="new-test">＋ Создать тест</button><p class="hint">Последние 100 тестов, сначала новые.</p><div class="quiz-list">${(state.tests || []).map(t=>`<button class="quiz-list-item" data-action="edit-test" data-id="${escape(t.id)}"><span><strong>${escape(t.name || 'Без названия')}</strong><small>${escape(t.university_name)}</small></span><span class="quiz-badge ${t.status==='draft'?'':'ready'}">${escape(statusName(t.status))}</span></button>`).join('') || '<p>Сохранённых тестов пока нет.</p>'}</div>`;
 if(state.screen==='available-tests') return `<button class="text-button back" data-nav="account">← На главную</button><h2 tabindex="-1">Доступные тесты</h2>${notice}<p class="hint">Одна попытка на тест. После начала время идёт непрерывно. Все принятые ответы сохраняются.</p><form data-form="test-link"><div class="field"><label for="test-address">Ссылка или идентификатор private-теста</label><input id="test-address" name="test-address" value="${escape(state.testLink)}" required></div><button class="primary" type="submit">Начать / продолжить</button></form><div class="quiz-list">${(state.availableTests || []).map(t=>`<button class="quiz-list-item" data-action="start-test" data-id="${escape(t.id)}"><span><strong>${escape(t.name)}</strong><small>${escape(t.university_name)} · ${escape(t.time_to_complete)} сек.</small><small>${escape(t.description)}</small></span><span class="quiz-badge">${t.attempt_status?escape(statusName(t.attempt_status)):'Начать'}</span></button>`).join('') || '<p>Публичных тестов вашего вуза пока нет.</p>'}</div>`;
 if(state.screen==='test-results') return `<button class="text-button back" data-action="edit-test" data-id="${escape(state.testResultsId)}">← К тесту</button><h2 tabindex="-1">Результаты учеников</h2>${notice}<button class="secondary" data-action="test-results">Обновить</button><div class="quiz-list">${(state.testResults || []).map(r=>`<div class="quiz-list-item"><span><strong>${escape(r.name || r.student_id)}</strong><small>${escape(statusName(r.status))} · Отвечено ${escape(r.answered_count)} из ${escape(r.question_count)}</small></span><strong>${escape(r.score)} / ${escape(r.question_count)}</strong></div>`).join('') || '<p>Прохождений пока нет.</p>'}</div>`;
 if(state.screen==='test-play') {
  const d=state.testPlay;if(!d) return notice;
  const q=d.current_question;
  return `<button class="text-button back" data-action="available-tests">← Доступные тесты</button><h2 tabindex="-1">${escape(d.test.name)}</h2>${notice}${q?`<p>Вопрос ${q.position+1} из ${d.question_count} · Осталось <span data-test-timer>${testSeconds(state)} с</span></p><p class="hint">Выход не останавливает таймер. К принятым ответам вернуться нельзя.</p><section class="question-card-editor"><h3>${escape(q.text)}</h3><div class="quiz-image">${showImage(q.image_url)}</div><fieldset class="quiz-fields" ${state.busy?'disabled':''}><legend>${q.type==='single'?'Один правильный ответ':'Несколько правильных ответов'}</legend>${q.answers.map(a=>`<label class="game-answer"><input type="${q.type==='single'?'radio':'checkbox'}" name="test-choice" data-test-choice="${escape(a.id)}" ${(state.testChoices || []).includes(a.id)?'checked':''}><span>${escape(a.text)}<span class="quiz-image">${showImage(a.image_url)}</span></span></label>`).join('')}<button class="primary" data-action="test-submit">Ответить${q.position+1===d.question_count?' и завершить':''}</button></fieldset></section>`:`<p>${d.attempt.status==='expired'?'Время истекло. Сохранённые ответы учтены.':'Тест завершён.'}</p><p>Результат: <strong>${escape(d.result?.score)} из ${escape(d.question_count)}</strong></p><p>Отвечено: ${escape(d.answered_count)} из ${escape(d.question_count)}.</p>`}<button class="secondary" data-action="test-refresh">Обновить состояние</button>`;
 }
 return notice;
}
const escape = value => String(value ?? '').replace(/[&<>"']/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
function errorLabel(path) {
 const question=/^questions\[(\d+)\]/.exec(path),answer=/\.answers\[(\d+)\]/.exec(path);
 if(question) return `Вопрос ${Number(question[1])+1}${answer?`, ответ ${Number(answer[1])+1}`:''}`;
 return ({name:'Название',description:'Описание',university_id:'Учебное заведение',time_to_complete:'Время',questions:'Вопросы',revision:'Версия теста'})[path] || 'Тест';
}

export const newTestQuestion = () => ({text:'',type:'single',image_id:null,image_url:'',answers:[newAnswer(),newAnswer()]});
export const newTest = university => ({university_id:university || '',name:'',description:'',time_to_complete:60,status:'draft',questions:[newTestQuestion()]});
export function testQuestionComplete(q) {
 const count=q.answers.filter(a=>a.is_correct).length;
 return !!q.text.trim() && q.answers.length>=2 && q.answers.every(a=>a.text.trim() || a.image_id) && (q.type==='single'?count===1:count>=1);
}
export function testPayload(d,status) {
 return {university_id:d.university_id,name:d.name,description:d.description,time_to_complete:Number(d.time_to_complete),status,
 ...(d.id?{revision:d.revision}:{}),questions:d.questions.map(q=>({text:q.text,type:q.type,image_id:q.image_id,answers:q.answers.map(a=>({text:a.text,is_correct:a.is_correct,image_id:a.image_id}))}))};
}
function imageField(item,q,a) {
 const attrs=`data-q="${q}" ${a===undefined?'':`data-a="${a}"`}`;
 const url=/^(https:\/\/|blob:)/.test(item.image_url || '') ? item.image_url:'';
 return `<div class="quiz-image">${url?`<img src="${escape(url)}" alt="Прикреплённая картинка" referrerpolicy="no-referrer">`:''}
 ${item.image_id?`<button type="button" class="text-button" data-action="test-remove-image" ${attrs}>Убрать картинку</button>`:''}
 <label class="image-picker">${item.image_id?'Заменить картинку':'＋ Добавить картинку'}<input type="file" accept="image/png,image/jpeg,image/webp" data-test-image ${attrs} aria-label="Картинка ${a===undefined?'вопроса':'ответа'} ${q+1}${a===undefined?'':'.'+(a+1)}"></label>
 ${item.upload_error?`<p class="quiz-field-error">${escape(item.upload_error)} Выберите файл ещё раз.</p>`:''}</div>`;
}

export function renderTestEditor(state,notice) {
 const d=state.testDraft,errors=state.testErrors || [];
 const fieldError=path=>errors.filter(e=>e.field===path).map(e=>`<p class="quiz-field-error" role="alert">${escape(e.message)}</p>`).join('');
 if(!d) return notice;
 const published=d.status!=='draft';
 const canAdd=d.questions.every(q=>testQuestionComplete(q)) && d.questions.length<100;
 return `<button type="button" class="text-button back" data-action="my-tests">← Мои тесты</button><div class="quiz-heading"><div><p class="step-label">Мастерская преподавателя</p><h2 tabindex="-1">${published?'Просмотр теста':d.id?'Редактирование теста':'Новый тест'}</h2></div><span class="quiz-badge ${d.status!=='draft'?'ready':''}">${d.status!=='draft'?'Опубликован':'Черновик'}</span></div><p class="subtitle">${published?'Тест опубликован. Редактирование и возврат в черновик недоступны.':'Начните с вопроса. Незавершённую работу можно сохранить в черновик.'}</p>${notice}
 ${errors.length?`<div class="notice error" role="alert">${errors.map(e=>`<div>${escape(errorLabel(e.field))}: ${escape(e.message)}</div>`).join('')}</div>`:''}
 ${published?`<div class="field"><label for="test-link">Ссылка для студентов своего вуза</label><input id="test-link" value="${escape(state.testShareUrl)}" readonly><button type="button" class="secondary" data-action="test-results" data-id="${escape(d.id)}">Результаты учеников</button></div>`:''}
 <form data-form="test" novalidate><fieldset class="quiz-fields" ${state.busy || published?'disabled':''}>
 <section class="quiz-settings"><div class="field"><label for="test-university">Учебное заведение</label><select id="test-university" data-test-field="university_id"><option value="">Выберите вуз</option>${(state.testUniversities || []).map(u=>`<option value="${escape(u.id)}" ${u.id===d.university_id?'selected':''}>${escape(u.name)}</option>`).join('')}</select>${fieldError('university_id')}</div>
 <div class="field"><label for="test-name">Название теста</label><input id="test-name" data-test-field="name" value="${escape(d.name)}" maxlength="500" placeholder="Например, география без границ">${fieldError('name')}</div>
 <div class="field"><label for="test-description">Описание <span class="hint">необязательно</span></label><textarea id="test-description" data-test-field="description" rows="3" maxlength="10000" placeholder="О чём этот тест?">${escape(d.description)}</textarea></div>
 <div class="field"><label for="test-time">Время на весь тест, сек.</label><input id="test-time" type="number" min="1" max="2147483647" step="1" data-test-field="time_to_complete" value="${escape(d.time_to_complete)}"><p class="hint">Таймер продолжает идти, даже если ученик выйдет из теста.</p>${fieldError('time_to_complete')}</div></section>
 <div class="quiz-questions">${d.questions.map((q,i)=>`<section class="question-card-editor"><div class="question-heading"><h3>Вопрос ${i+1}</h3>${d.questions.length>1?`<button type="button" class="text-button" data-action="test-remove-question" data-q="${i}">Удалить вопрос</button>`:''}</div>
 <div class="field"><label for="q-${i}-text">Текст вопроса</label><textarea id="q-${i}-text" data-q="${i}" data-test-field="text" rows="2" maxlength="10000" placeholder="Что вы хотите спросить?">${escape(q.text)}</textarea>${fieldError(`questions[${i}].text`)}</div>${imageField(q,i)}
 <div class="question-options"><div class="field"><label for="q-${i}-type">Правильные ответы</label><select id="q-${i}-type" data-q="${i}" data-test-field="type"><option value="single" ${q.type==='single'?'selected':''}>Один правильный</option><option value="multy" ${q.type==='multy'?'selected':''}>Несколько правильных</option></select></div></div>
 <p class="answer-hint">Отметьте правильные варианты слева</p><div class="answer-list">${q.answers.map((a,j)=>`<div class="answer-editor"><div class="answer-line"><input class="correct-choice" type="${q.type==='single'?'radio':'checkbox'}" name="correct-${i}" data-test-field="is_correct" data-q="${i}" data-a="${j}" ${a.is_correct?'checked':''} aria-label="Правильный ответ ${i+1}.${j+1}"><input aria-label="Ответ ${i+1}.${j+1}" data-test-field="text" data-q="${i}" data-a="${j}" value="${escape(a.text)}" maxlength="5000" placeholder="Вариант ${j+1}">${q.answers.length>2?`<button type="button" class="text-button" data-action="test-remove-answer" data-q="${i}" data-a="${j}" aria-label="Удалить ответ ${i+1}.${j+1}">×</button>`:''}</div>${imageField(a,i,j)}</div>`).join('')}</div>
 ${fieldError(`questions[${i}].answers`)}<button type="button" class="secondary" data-action="test-add-answer" data-q="${i}" ${q.answers.length>=20?'disabled':''}>＋ Добавить вариант</button></section>`).join('')}</div>
 <button type="button" class="secondary add-question" data-action="test-add-question" ${canAdd?'':'disabled'}>＋ Добавить вопрос</button><p class="hint" id="next-question-hint">${canAdd?'Можно добавить следующий вопрос.':'Заполните вопросы, минимум два ответа и отметьте правильные варианты.'}</p>
 ${published?'':`<div class="quiz-save-bar"><span class="hint" data-test-dirty>${state.testDirty?'Есть несохранённые изменения':'Изменения сохранены'}</span><button type="button" class="secondary" data-action="test-save-draft">Сохранить черновик</button><button type="submit" class="primary">Опубликовать public</button><button type="button" class="secondary" data-action="test-publish-private">Опубликовать private</button></div><p class="hint">После публикации изменить тест или вернуть его в черновик нельзя. Public виден в списке своего вуза; private доступен по ссылке.</p>`}
 </fieldset></form><p class="hint">Картинки: PNG, JPEG, WebP, до 5 МиБ. ${state.testUploading?'Картинка загружается…':''}</p>`;
}
