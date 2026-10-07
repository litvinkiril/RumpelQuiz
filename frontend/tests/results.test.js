import test from 'node:test';
import assert from 'node:assert/strict';
import {renderResults} from '../game.js';

const person=(rank,score,user_id='a',name='Анна Иванова')=>({rank,score,user_id,name,correct_count:score,answered_count:8,question_count:10});

test('results display server ranks, shared places and the current participant',()=>{
  const html=renderResults([person(1,9),person(2,8,'b'),person(2,8,'c'),person(4,5,'d')],10,{userId:'d'});
  assert.match(html,/Разделили место/);
  assert.match(html,/При равных баллах участники делят место/);
  assert.match(html,/aria-label="4 место"/);
  assert.doesNotMatch(html,/aria-label="3 место"/);
  assert.match(html,/result-self/);assert.match(html,/result-you">Вы/);
  assert.match(html,/value="90"/);
  assert.doesNotMatch(html,/style=/,'Progress must work with the strict CSP');
});

test('cancelled and empty sessions have no podium or assigned places',()=>{
  const html=renderResults([person(1,0)],10,{status:'cancelled'});
  assert.doesNotMatch(html,/results-leaders|aria-label="1 место"/);
  assert.match(html,/места не присуждаются/);
  assert.match(renderResults([],10),/В этой сессии нет участников/);
  assert.doesNotMatch(renderResults([person(1,0)],10),/results-leaders/);
});

test('large ties preserve every participant in the table and escape names',()=>{
  const rows=Array.from({length:5},(_,i)=>person(1,8,String(i),'<Участник '+i+'>'));
  const html=renderResults(rows,10);
  assert.match(html,/Ещё участников: 2/);
  assert.equal((html.match(/aria-label="1 место"/g) || []).length,5);
  assert.match(html,/&lt;Участник 4&gt;/);
  assert.doesNotMatch(html,/<Участник/);
  assert.doesNotMatch(renderResults([{...person(null,8)}],10),/aria-label="1 место"/,'The client must not invent missing ranks');
});
