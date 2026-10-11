import test from 'node:test';
import assert from 'node:assert/strict';
import {Library,LibraryError} from './library.mjs';
import {HistoryActions} from './history_actions.mjs';
import {YT,YTNodes} from 'youtubei.js';
const id='abcdefghijk';
const profiles=[{id:'signed',cookie:'synthetic'},{id:'other',cookie:'synthetic'}];
test('history removal references are private, profile-owned and confirmed only once',async()=>{
  let calls=0;
  const feed={videos:[{id}],has_continuation:false,async removeVideo(video,pages){
    calls++;assert.equal(video,id);assert.equal(pages,1);return true;
  }};
  const library=new Library(async()=>({session:{logged_in:true},getHistory:async()=>feed,getHomeFeed:async()=>feed}),profiles);
  const page=await library.execute({action:'history',profile:'signed'});
  const cursor=page.items[0].history_remove_cursor;assert.match(cursor,/^[A-Za-z0-9_-]{1,64}$/);
  assert.ok(!(await library.execute({action:'home',profile:'signed'})).items[0].history_remove_cursor);
  const request={action:'history_remove',profile:'signed',id,cursor};
  await assert.rejects(library.execute({...request,profile:'guest'}),{code:'account_required'});
  await assert.rejects(library.execute({...request,profile:'other'}),{code:'cursor_expired'});
  await assert.rejects(library.execute({...request,id:'12345678901'}),{code:'cursor_expired'});
  assert.equal(calls,0);
  const result=await library.execute(request);
  assert.deepEqual(result,{api_version:1,success:true,video_id:id});
  assert.strictEqual(await library.execute(request),result);assert.equal(calls,1);
});
test('local pages and upstream history continuations retain their exact removal feed',async()=>{
  const removed=[];
  const tail={videos:[{id:'12345678901'}],has_continuation:false,async removeVideo(video){removed.push('tail:'+video);return true;}};
  const feed={videos:Array.from({length:30},()=>({id})),has_continuation:true,
    async getContinuation(){return tail;},async removeVideo(video){removed.push('first:'+video);return true;}};
  const library=new Library(async()=>({session:{logged_in:true},getHistory:async()=>feed}),profiles);
  const a=await library.execute({action:'history',profile:'signed'});
  const b=await library.execute({action:'continue',profile:'signed',cursor:a.cursor});
  assert.equal(a.items[0].history_remove_cursor,b.items[0].history_remove_cursor);
  const c=await library.execute({action:'continue',profile:'signed',cursor:b.cursor});
  await library.execute({action:'history_remove',profile:'signed',id:'12345678901',cursor:c.items[0].history_remove_cursor});
  assert.deepEqual(removed,['tail:12345678901']);
});
test('failed, concurrent, expired and unsigned removals never repeat an uncertain intent',async()=>{
  let now=0,calls=0,finish;
  const actions=new HistoryActions(LibraryError,()=>now),client={session:{logged_in:true}};
  const feed={removeVideo(){calls++;return new Promise(resolve=>finish=resolve);}};
  const cursor=actions.register('signed',feed,id),request={id,cursor};
  await assert.rejects(actions.remove('signed',{session:{logged_in:false}},request),{code:'account_required'});
  const pending=actions.remove('signed',client,request);
  await assert.rejects(actions.remove('signed',client,request),{code:'history_unconfirmed'});
  finish(false);await assert.rejects(pending,{code:'history_unconfirmed'});
  await assert.rejects(actions.remove('signed',client,request),{code:'history_unconfirmed'});assert.equal(calls,1);
  now=120001;await assert.rejects(actions.remove('signed',client,request),{code:'cursor_expired'});
  assert.equal(actions.register('signed',{},id),'');
  for(let i=0;i<256;++i)assert.ok(actions.register('signed',feed,String(i).padStart(11,'0')));
  assert.equal(actions.register('signed',feed,'zzzzzzzzzzz'),'');
});
test('pinned History removal submits the retained feedback token and requires processed confirmation',async()=>{
  let processed=true,calls=0;
  const node=new YTNodes.Video({videoId:id,title:{runs:[{text:'Fixture'}]},thumbnail:{thumbnails:[]},
    menu:{menuRenderer:{topLevelButtons:[{buttonRenderer:{text:{simpleText:'Remove'},
      serviceEndpoint:{feedbackEndpoint:{feedbackToken:'private-feedback-fixture'}}}}]}}});
  const feed={sections:[{contents:[node]}],actions:{async execute(path,body){
    calls++;assert.equal(path,'/feedback');assert.deepEqual(body,{feedbackTokens:['private-feedback-fixture']});
    return {data:{feedbackResponses:[{isProcessed:processed}]}};
  }},removeVideo:YT.History.prototype.removeVideo};
  const actions=new HistoryActions(LibraryError),client={session:{logged_in:true}};
  const cursor=actions.register('signed',feed,id);
  assert.equal((await actions.remove('signed',client,{id,cursor})).success,true);
  processed=false;
  const other=actions.register('other',feed,id);
  await assert.rejects(actions.remove('other',client,{id,cursor:other}));
  await assert.rejects(actions.remove('other',client,{id,cursor:other}),{code:'history_unconfirmed'});
  assert.equal(calls,2);
});
