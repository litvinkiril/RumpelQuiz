const escape = value => String(value ?? '').replace(/[&<>"']/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
export const newAnswer = () => ({text:'',is_correct:false,image_id:null,image_url:''});
function errorLabel(path) {
 const question=/^questions\[(\d+)\]/.exec(path),answer=/\.answers\[(\d+)\]/.exec(path);
 if(question) return `Вопрос ${Number(question[1])+1}${answer?`, ответ ${Number(answer[1])+1}`:''}`;
 return ({name:'Название',description:'Описание',university_id:'Учебное заведение',default_time_seconds:'Время',questions:'Вопросы',revision:'Версия квиза'})[path] || 'Квиз';
}
export const newQuestion = () => ({text:'',type:'single',time_seconds:null,image_id:null,image_url:'',answers:[newAnswer(),newAnswer()]});
export const newQuiz = university => ({university_id:university || '',name:'',description:'',default_time_seconds:60,status:'draft',questions:[newQuestion()]});
const validTime = n => Number.isInteger(Number(n)) && Number(n)>0 && Number(n)<=2147483647;
export function questionComplete(q, defaultTime) {
 const count=q.answers.filter(a=>a.is_correct).length;
 return !!q.text.trim() && validTime(q.time_seconds ?? defaultTime) && q.answers.length>=2 && q.answers.every(a=>a.text.trim() || a.image_id) && (q.type==='single' ? count===1:count>=1);
}
export function quizPayload(draft,status) {
 return {university_id:draft.university_id,name:draft.name,description:draft.description,default_time_seconds:Number(draft.default_time_seconds),status,
  ...(draft.id ? {revision:draft.revision}:{}), questions:draft.questions.map(q=>({text:q.text,type:q.type,time_seconds:q.time_seconds===null ? null:Number(q.time_seconds),image_id:q.image_id,
   answers:q.answers.map(a=>({text:a.text,is_correct:a.is_correct,image_id:a.image_id}))}))};
}
function imageField(item,q,a) {
 const attrs=`data-q="${q}" ${a===undefined?'':`data-a="${a}"`}`;
 const url=/^(https:\/\/|blob:)/.test(item.image_url || '') ? item.image_url:'';
 return `<div class="quiz-image">${url?`<img src="${escape(url)}" alt="Прикреплённая картинка" referrerpolicy="no-referrer">`:''}
 ${item.image_id?`<button type="button" class="text-button" data-action="quiz-remove-image" ${attrs}>Убрать картинку</button>`:''}
 <label class="image-picker">${item.image_id?'Заменить картинку':'＋ Добавить картинку'}<input type="file" accept="image/png,image/jpeg,image/webp" data-quiz-image ${attrs} aria-label="Картинка ${a===undefined?'вопроса':'ответа'} ${q+1}${a===undefined?'':'.'+(a+1)}"></label>
 ${item.upload_error?`<p class="quiz-field-error">${escape(item.upload_error)} Выберите файл ещё раз.</p>`:''}</div>`;
}
export function renderQuiz(state,notice) {
 const d=state.quizDraft;
 const errors=state.quizErrors || [];
 const fieldError=path=>errors.filter(e=>e.field===path).map(e=>`<p class="quiz-field-error" role="alert">${escape(e.message)}</p>`).join('');
 if(state.screen==='quizzes') return `<button class="text-button back" data-nav="account">← На главную</button><p class="step-label">Мастерская преподавателя</p><h2 tabindex="-1">Мои квизы</h2><p class="subtitle">Продолжите черновик или подготовьте новый квиз.</p>${notice}<button class="primary" data-action="new-quiz">＋ Создать квиз</button><p class="hint">Показаны последние 100 квизов.</p><div class="quiz-list">${(state.quizzes || []).map(k=>`<button class="quiz-list-item" data-action="edit-quiz" data-id="${escape(k.id)}"><span><strong>${escape(k.name || 'Без названия')}</strong><small>${escape(k.university_name)}</small></span><span class="quiz-badge ${k.status==='ready'?'ready':''}">${k.status==='ready'?'Опубликован':'Черновик'}</span><span>→</span></button>`).join('') || '<p class="empty-list">Здесь появятся ваши сохранённые квизы.</p>'}</div>`;
 if(!d) return notice;
 const published=d.status==='ready';
 const canAdd=d.questions.every(q=>questionComplete(q,d.default_time_seconds)) && d.questions.length<100;
 return `<button type="button" class="text-button back" data-action="my-quizzes">← Мои квизы</button><div class="quiz-heading"><div><p class="step-label">Мастерская преподавателя</p><h2 tabindex="-1">${published?'Просмотр квиза':d.id?'Редактирование квиза':'Новый квиз'}</h2></div><span class="quiz-badge ${d.status==='ready'?'ready':''}">${d.status==='ready'?'Опубликован':'Черновик'}</span></div><p class="subtitle">${published?'Квиз опубликован. Редактирование и возврат в черновик недоступны.':'Начните с вопроса. Незавершённую работу можно сохранить в черновик.'}</p>${notice}
 ${errors.length?`<div class="notice error" role="alert">${errors.map(e=>`<div>${escape(errorLabel(e.field))}: ${escape(e.message)}</div>`).join('')}</div>`:''}
 ${published?'<div class="game-launch"><p>Проведите этот квиз: участники войдут по коду или QR.</p><button type="button" class="primary" data-action="game-create">Создать сессию</button></div>':''}
 <form data-form="quiz" novalidate><fieldset class="quiz-fields" ${state.busy || published?'disabled':''}>
 <section class="quiz-settings"><div class="field"><label for="quiz-university">Учебное заведение</label><select id="quiz-university" data-quiz-field="university_id"><option value="">Выберите вуз</option>${(state.quizUniversities || []).map(u=>`<option value="${escape(u.id)}" ${u.id===d.university_id?'selected':''}>${escape(u.name)}</option>`).join('')}</select>${fieldError('university_id')}</div>
 <div class="field"><label for="quiz-name">Название квиза</label><input id="quiz-name" data-quiz-field="name" value="${escape(d.name)}" maxlength="500" placeholder="Например, география без границ">${fieldError('name')}</div>
 <div class="field"><label for="quiz-description">Описание <span class="hint">необязательно</span></label><textarea id="quiz-description" data-quiz-field="description" rows="3" maxlength="10000" placeholder="О чём этот квиз?">${escape(d.description)}</textarea></div>
 <div class="field"><label for="quiz-time">Время на вопрос по умолчанию, сек.</label><input id="quiz-time" type="number" min="1" max="2147483647" step="1" data-quiz-field="default_time_seconds" value="${escape(d.default_time_seconds)}"><p class="hint">Каждый вопрос использует это время, пока вы не зададите ему своё.</p>${fieldError('default_time_seconds')}</div></section>
 <div class="quiz-questions">${d.questions.map((q,i)=>`<section class="question-card-editor"><div class="question-heading"><h3>Вопрос ${i+1}</h3>${d.questions.length>1?`<button type="button" class="text-button" data-action="quiz-remove-question" data-q="${i}">Удалить вопрос</button>`:''}</div>
 <div class="field"><label for="q-${i}-text">Текст вопроса</label><textarea id="q-${i}-text" data-q="${i}" data-quiz-field="text" rows="2" maxlength="10000" placeholder="Что вы хотите спросить?">${escape(q.text)}</textarea>${fieldError(`questions[${i}].text`)}</div>${imageField(q,i)}
 <div class="question-options"><div class="field"><label for="q-${i}-type">Правильные ответы</label><select id="q-${i}-type" data-q="${i}" data-quiz-field="type"><option value="single" ${q.type==='single'?'selected':''}>Один правильный</option><option value="multy" ${q.type==='multy'?'selected':''}>Несколько правильных</option></select></div><div class="field"><label for="q-${i}-time">Своё время, сек.</label><input id="q-${i}-time" type="number" min="1" step="1" data-q="${i}" data-quiz-field="time_seconds" value="${escape(q.time_seconds ?? '')}" placeholder="${escape(d.default_time_seconds)}"><p class="hint">Пустое поле — время по умолчанию.</p></div></div>
 <p class="answer-hint">Отметьте правильные варианты слева</p><div class="answer-list">${q.answers.map((a,j)=>`<div class="answer-editor"><div class="answer-line"><input class="correct-choice" type="${q.type==='single'?'radio':'checkbox'}" name="correct-${i}" data-quiz-field="is_correct" data-q="${i}" data-a="${j}" ${a.is_correct?'checked':''} aria-label="Правильный ответ ${i+1}.${j+1}"><input aria-label="Ответ ${i+1}.${j+1}" data-quiz-field="text" data-q="${i}" data-a="${j}" value="${escape(a.text)}" maxlength="5000" placeholder="Вариант ${j+1}">${q.answers.length>2?`<button type="button" class="text-button" data-action="quiz-remove-answer" data-q="${i}" data-a="${j}" aria-label="Удалить ответ ${i+1}.${j+1}">×</button>`:''}</div>${imageField(a,i,j)}</div>`).join('')}</div>
 ${fieldError(`questions[${i}].answers`)}<button type="button" class="secondary" data-action="quiz-add-answer" data-q="${i}" ${q.answers.length>=20?'disabled':''}>＋ Добавить вариант</button></section>`).join('')}</div>
 <button type="button" class="secondary add-question" data-action="quiz-add-question" ${canAdd?'':'disabled'}>＋ Добавить вопрос</button><p class="hint" id="next-question-hint">${canAdd?'Можно добавить следующий вопрос.':'Заполните вопросы, минимум два ответа и отметьте правильные варианты.'}</p>
 ${published?'':`<div class="quiz-save-bar"><span class="hint" data-quiz-dirty>${state.quizDirty?'Есть несохранённые изменения':'Изменения сохранены'}</span><button type="button" class="secondary" data-action="quiz-save-draft">Сохранить черновик</button><button type="submit" class="primary">Опубликовать</button></div><p class="hint">После публикации изменить квиз или вернуть его в черновик нельзя. Сессия не запускается.</p>`}
 </fieldset></form><p class="hint">Картинки: PNG, JPEG, WebP, до 5 МиБ. ${state.quizUploading?'Картинка загружается…':''}</p>`;
}
