import test from 'node:test';
import assert from 'node:assert/strict';
import {Library,LibraryError} from './library.mjs';
import {HistoryActions} from './history_actions.mjs';
import {YT,YTNodes} from 'youtubei.js';
const id='abcdefghijk';
const profiles=[{id:'signed',cookie:'synthetic'},{id:'other',cookie:'synthetic'}];

test('mark watched uses pinned tracking methods at the parsed duration and retains confirmed intent',async()=>{
  const calls=[];
  const info=new YT.VideoInfo([{data:{videoDetails:{videoId:id,lengthSeconds:'125',thumbnail:{thumbnails:[]}},
    playbackTracking:{videostatsPlaybackUrl:{baseUrl:'https://s.youtube.com/api/stats/playback'},
      videostatsWatchtimeUrl:{baseUrl:'https://s.youtube.com/api/stats/watchtime'}}}}],
    {stats:async(url,client,params)=>{calls.push({url,params});return new Response(null,{status:204});}},'fixture-nonce');
  const library=new Library(async()=>({session:{logged_in:true},getBasicInfo:async()=>info}),profiles);
  const request={action:'mark_watched',profile:'signed',id,session:'confirm-one'};
  const result=await library.execute(request);
  assert.deepEqual(result,{api_version:1,success:true,video_id:id,position_sec:125});
  assert.equal(calls.length,2);assert.equal(calls[1].params.cmt,'125.000');assert.equal(calls[1].params.final,'1');
  assert.deepEqual(await library.execute(request),result);assert.equal(calls.length,2);
  await assert.rejects(library.execute({...request,id:'12345678901'}),{code:'invalid_progress'});
  await assert.rejects(library.execute({...request,profile:'guest'}),{code:'account_required'});
  await library.execute({...request,profile:'other'});assert.equal(calls.length,4);
});

test('mark watched rejects invalid metadata before submission and never repeats an uncertain intent',async()=>{
  for(const patch of [{id:'12345678901'},{duration:NaN},{duration:0},{duration:1000001},{is_live:true},{is_upcoming:true}]) {
    let writes=0;
    const library=new Library(async()=>({session:{logged_in:true},getBasicInfo:async()=>({basic_info:{id,duration:12,...patch},
      addToWatchHistory:async()=>{writes++;return {ok:true};}})}),profiles);
    await assert.rejects(library.execute({action:'mark_watched',profile:'signed',id,session:'metadata'}),{code:'history_unconfirmed'});
    assert.equal(writes,0);
  }
  let writes=0,release;
  const library=new Library(async()=>({session:{logged_in:true},getBasicInfo:async()=>({basic_info:{id,duration:12},
    addToWatchHistory:async()=>{writes++;await new Promise(resolve=>release=resolve);return {ok:true};},
    updateWatchTime:async()=>{writes++;return {ok:false};}})}),profiles);
  const request={action:'mark_watched',profile:'signed',id,session:'uncertain'};
  const first=library.execute(request);
  while(!release)await new Promise(resolve=>setImmediate(resolve));
  await assert.rejects(library.execute(request),{code:'history_unconfirmed'});
  release();await assert.rejects(first,{code:'history_unconfirmed'});
  await assert.rejects(library.execute(request),{code:'history_unconfirmed'});assert.equal(writes,2);
  await assert.rejects(library.execute({...request,session:''}),{code:'invalid_progress'});
});
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
