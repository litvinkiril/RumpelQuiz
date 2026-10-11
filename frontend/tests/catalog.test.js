import test from 'node:test';
import assert from 'node:assert/strict';
import {createApi, createController, ApiError, renderView} from '../app.js';

const quiz = (i=0) => ({quiz_id:'quiz-'+i,title:'Математика '+i,creator_first_name:'Анна',creator_last_name:'Иванова',question_count:3,is_favourite:false});
const setup = api => {
 const c=createController({api});c.state.token='token';c.state.screen='quiz-section';c.state.accountRoles=['teacher'];return c;
};

test('section navigation preserves authentication, cancels pending loads and retries roles in place',async()=>{
 let resolve;
 const c=setup(()=>new Promise(r=>{resolve=r;}));
 const pending=c.openProfile();c.navigate('test-section');
 resolve({success:true,university_position:[{role:'student'}]});
 assert.equal(await pending,false);assert.equal(c.state.screen,'test-section');assert.equal(c.state.token,'token');assert.equal(c.state.profile,null);
 c.state.accountRoles=null;
 const roles=c.reloadAccountRoles();resolve({success:true,university_position:[{role:'student'}]});
 assert.equal(await roles,true);assert.equal(c.state.screen,'test-section');assert.deepEqual(c.state.accountRoles,['student']);
 c.navigate('login');c.navigate('quiz-section');assert.equal(c.state.screen,'login');
});

test('quiz search sends all required encoded parameters and paginates independently of the input',async()=>{
 const requests=[];
 const api=createApi(async(url,options)=>{requests.push({url,options});return {ok:true,json:async()=>({quizzes:requests.length===1?Array.from({length:10},(_,i)=>quiz(i)):[quiz(10)]})};});
 const c=setup(api);c.state.quizCatalogName='  Алгебра & геометрия  ';c.state.quizCatalogFavourites=true;
 assert.equal(await c.openQuizCatalog(),true);
 let url=new URL(requests[0].url,'https://example.test');
 assert.equal(url.pathname,'/v1/quizzes/search');assert.equal(url.searchParams.get('name'),'Алгебра & геометрия');assert.equal(url.searchParams.get('favourites'),'true');assert.equal(url.searchParams.get('count_spend'),'0');
 assert.equal(requests[0].options.headers.Authorization,'Bearer token');assert.equal(c.state.quizCatalogMore,true);
 assert.equal(await c.searchQuizCatalog('unsent input',false,true),true);
 url=new URL(requests[1].url,'https://example.test');assert.equal(url.searchParams.get('count_spend'),'10');assert.equal(url.searchParams.get('favourites'),'true');
 assert.equal(c.state.quizCatalog.length,11);assert.equal(c.state.quizCatalogMore,false);
 assert.equal(await c.searchQuizCatalog(undefined,undefined,true),false);assert.equal(requests.length,2);
});

test('a new query resets paging; failures can retry and late responses cannot reopen the catalog',async()=>{
 let resolve;
 const c=setup(()=>new Promise(r=>{resolve=r;}));
 const pending=c.openQuizCatalog();c.navigate('account');resolve({quizzes:[quiz()]});
 assert.equal(await pending,false);assert.equal(c.state.screen,'account');assert.equal(c.state.quizCatalog,null);
 c.state.screen='quiz-catalog';c.state.quizCatalog=[quiz()];c.state.quizCatalogOffset=10;
 const search=c.searchQuizCatalog(' Новая тема ');assert.equal(c.state.quizCatalogOffset,0);assert.equal(c.state.quizCatalog,null);
 resolve({wrong:[]});assert.equal(await search,false);assert.match(c.state.error,/Не удалось загрузить/);
 const retry=c.searchQuizCatalog();resolve({quizzes:[quiz(2)]});assert.equal(await retry,true);assert.equal(c.state.quizCatalogName,'Новая тема');assert.equal(c.state.quizCatalog.length,1);
});

test('students cannot open the author catalog',async()=>{
 let calls=0;const c=setup(async()=>{calls++;return {quizzes:[]};});c.state.accountRoles=['student'];
 assert.equal(await c.openQuizCatalog(),false);assert.equal(calls,0);assert.equal(c.state.screen,'quiz-section');
});

test('favourite writes are protected, preserve failed state and adjust the filtered page offset',async()=>{
 const calls=[];let failed=false;
 const c=setup(async(...args)=>{calls.push(args);if(failed)throw new ApiError('No network');return {success:true};});
 c.state.screen='quiz-catalog';c.state.quizCatalog=[quiz()];
 assert.equal(await c.toggleQuizFavourite('quiz-0'),true);assert.equal(c.state.quizCatalog[0].is_favourite,true);
 assert.deepEqual(calls[0],['quizzes/quiz-0/favourite',undefined,'token','POST']);
 failed=true;assert.equal(await c.toggleQuizFavourite('quiz-0'),false);assert.equal(c.state.quizCatalog[0].is_favourite,true);
 failed=false;c.state.quizCatalogFavourites=true;c.state.quizCatalogOffset=10;
 assert.equal(await c.toggleQuizFavourite('quiz-0'),true);assert.deepEqual(calls.at(-1),['quizzes/quiz-0/favourite',undefined,'token','DELETE']);
 assert.equal(c.state.quizCatalog.length,0);assert.equal(c.state.quizCatalogOffset,9);
 c.navigate('login');assert.equal(c.state.quizCatalog,null);assert.equal(c.state.quizCatalogName,'');
});

test('catalog metadata and queries are escaped, empty/error states retain navigation',()=>{
 const state={screen:'quiz-catalog',quizCatalog:[{...quiz(),title:'<script>x</script>',creator_first_name:'<img onerror=x>'}],quizCatalogName:'" onfocus="bad'};
 const html=renderView(state);assert.ok(!html.includes('<script>'));assert.ok(!html.includes('<img onerror'));assert.ok(!html.includes(' onfocus="bad'));assert.match(html,/data-action="my-quizzes"/);
 assert.match(renderView({...state,quizCatalog:[]}),/Квизы не найдены/);
 assert.match(renderView({...state,quizCatalog:null,error:'Offline'}),/quiz-catalog-retry/);
});
