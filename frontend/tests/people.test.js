import test from 'node:test';
import assert from 'node:assert/strict';
import {createController, ApiError, renderView} from '../app.js';

const person = {user_id: '1', first_name: 'Кирилл', last_name: 'Литвин', email: 'test@example.invalid', roles: ['student', 'teacher'], student_details: {group: {name: 'ПИ-24'}, faculties: [{name: 'ФКН'}, {name: 'ФЭН'}]}};
const page = {success: true, people: [person], has_more: false, next_offset: null};
function setup(api) {
  const c = createController({api});
  Object.assign(c.state, {token: 'jwt', selectedUniversity: {id: 'hse', name: 'ВШЭ'}});
  c.openPeople(); return c;
}
test('opening search does not load a roster; invalid queries do not send requests', async () => {
  const c = setup(() => { throw new Error('Unexpected request'); });
  assert.match(renderView(c.state), /Начните с имени/);
  for (const query of ['', ' ', 'Я', 'я'.repeat(101)]) assert.equal(await c.searchPeople(query), false);
});
test('search encodes query, paginates and resets results for another name', async () => {
  const calls = [];
  const c = setup(async path => { calls.push(new URL(path, 'https://example.invalid')); return calls.length === 1 ? {...page, has_more: true, next_offset: 20} : {...page, people: [{...person, user_id: '2'}]}; });
  await c.searchPeople(' Литвин Кирилл ');
  assert.equal(calls[0].searchParams.get('q'), 'Литвин Кирилл');
  assert.equal(calls[0].searchParams.get('limit'), '20');
  await c.searchPeople('', true);
  assert.equal(calls[1].searchParams.get('offset'), '20');
  assert.equal(c.state.people.length, 2);
  await c.searchPeople('Другое имя');
  assert.equal(calls[2].searchParams.get('offset'), '0');
  assert.equal(c.state.people.length, 1);
});
test('contact opens locally; student details and roles appear only in results', async () => {
  const c = setup(async () => page); await c.searchPeople('Кирилл');
  const html = renderView(c.state);
  for (const value of ['Студент', 'Преподаватель', 'ПИ-24', 'ФКН, ФЭН']) assert.ok(html.includes(value));
  assert.ok(!html.includes(person.email));
  c.openPerson('1'); assert.match(renderView(c.state), /mailto:test%40example.invalid/);
  c.closeAdmin(); assert.equal(c.state.selectedAdmin, null);
});
test('late search cannot restore results after navigation', async () => {
  let resolve; const c = setup(() => new Promise(r => resolve = r));
  const pending = c.searchPeople('Кирилл'); c.navigate('university'); resolve(page); await pending;
  assert.equal(c.state.screen, 'university'); assert.equal(c.state.people, null);
});
test('failed continuation preserves results for retry; access revocation clears them', async () => {
  let error;
  const c = setup(async () => { if (error) throw error; return {...page, has_more: true, next_offset: 20}; });
  await c.searchPeople('Кирилл'); error = new ApiError('Нет сети');
  await c.searchPeople('', true); assert.equal(c.state.people.length, 1); assert.equal(c.state.nextOffset, 20);
  error = new ApiError('Нет доступа', 403); await c.searchPeople('', true);
  assert.equal(c.state.people, null); assert.equal(c.state.nextOffset, null);
});
test('empty results and HTML in names and faculties are rendered safely', async () => {
  const c = setup(async () => ({...page, people: []})); await c.searchPeople('Кирилл');
  assert.match(renderView(c.state), /Никого не нашли/);
  c.state.people = [{...person, first_name: '<script>bad</script>', student_details: {group: null, faculties: [{name: '<img src=x>'}]}}];
  const html = renderView(c.state); assert.ok(!html.includes('<script>')); assert.ok(!html.includes('<img src=x>'));
  assert.match(html, /Не назначена/);
});
