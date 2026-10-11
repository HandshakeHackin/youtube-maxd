// Shipping replay reader, parsed by the pinned YouTube.js parser; no network.
import test from 'node:test';
import assert from 'node:assert/strict';
import { Library } from './library.mjs';
const item = id => ({ liveChatTextMessageRenderer: {
  id, authorName: {simpleText: 'Viewer'}, authorExternalChannelId: 'author',
  message: {runs: [{text: id}]}, timestampText: {simpleText:'0:00'}, timestampUsec:'123000000',
}});
const add = id => ({addChatItemAction:{item:item(id)}});
const event = (ms, actions) => ({replayChatItemAction:{videoOffsetTimeMsec:String(ms),actions}});
const response = (events, next='', extra={}) => ({data:{continuationContents:{liveChatContinuation:{
  actions:events, continuations:[{playerSeekContinuationData:{continuation:'private-seek'}},
    ...(next ? [{liveChatReplayContinuationData:{continuation:next}}] : [])], ...extra,
}}}});
function fixture(responder, profiles=[]) {
  let now=0;const calls=[];
  const client={session:{logged_in:true},async getInfo(id) {return {basic_info:{id},livechat:{is_replay:true,continuation:'private-root'},
    actions:{async execute(path,options) {
      assert.equal(path,'live_chat/get_live_chat_replay');assert.equal(options.parse,false);
      assert.match(options.currentPlayerState.playerOffsetMs,/^\d+$/);
      calls.push(options);return responder(options,calls.length);
    }}};}};
  const library=new Library(async()=>client,profiles,()=>now);
  return {library,calls,time(value){now=value;}};
}
const open = (library,position=0,extra={}) => library.execute({action:'live_chat',id:'abcdefghijk',chat_position_sec:position,...extra});
const poll = (library,cursor,position,extra={}) => library.execute({action:'chat_poll',cursor,chat_position_sec:position,...extra});

test('replay uses playback clock, holds future actions through pause and applies timed deletion',async()=>{
  const {library,calls,time}=fixture(()=>response([
    event(0,[add('start')]),event(5000,[add('five')]),
    event(10000,[{removeChatItemAction:{targetItemId:'start'}}]),
  ]));
  const first=await open(library);assert.deepEqual(first.items.map(x=>x.id),['start']);
  assert.equal(first.items[0].offset_ms,0);assert.equal(first.is_replay,true);assert.equal(first.ended,false);
  assert.equal(JSON.stringify(first).includes('private-'),false);
  time(2000);const paused=await poll(library,first.cursor,0);assert.equal(calls.length,1);
  const five=await poll(library,paused.cursor,5);assert.equal(calls.length,1);
  assert.deepEqual(five.items.map(x=>x.id),['start','five']);assert.equal(five.items[1].offset_ms,5000);
  const ended=await poll(library,five.cursor,10);assert.equal(ended.ended,true);
  assert.deepEqual(ended.items.map(x=>x.id),['five']);assert.equal(ended.position_ms,10000);
  assert.deepEqual(await poll(library,five.cursor,10),ended); // repeat a delivered request
  await assert.rejects(poll(library,first.cursor,2),{code:'cursor_expired'});
  await library.execute({action:'chat_close',cursor:ended.cursor});assert.equal(library.chats.contexts.size,0);
});

test('replay rewind and large forward seek reset rows and use the server seek continuation',async()=>{
  const {library,calls,time}=fixture(options=>{
    const position=Number(options.currentPlayerState.playerOffsetMs);
    return response([event(position,[add('at-'+position)]),event(position+1000,[add('future')])],'private-next');
  });
  const first=await open(library,30);assert.equal(first.items[0].id,'at-30000');
  time(2000);const back=await poll(library,first.cursor,2);
  assert.equal(calls[1].continuation,'private-seek');assert.deepEqual(back.items.map(x=>x.id),['at-2000']);
  const ahead=await poll(library,back.cursor,100);
  assert.equal(calls[2].continuation,'private-seek');assert.deepEqual(ahead.items.map(x=>x.id),['at-100000']);
  await assert.rejects(poll(library,ahead.cursor,100,{profile:'another'}));
});

test('replay view switches before committing and scope, read-only and invalid clock are enforced',async()=>{
  const profile={id:'one',name:'Fixture',cookie:'fixture-cookie'};
  const {library,calls,time}=fixture(options=>options.continuation==='private-root'
    ? response([], 'unused', {header:{liveChatHeaderRenderer:{viewSelector:{sortFilterSubMenuRenderer:{subMenuItems:[
      {title:'Top chat',selected:true}, {title:'Live chat',selected:false,continuation:{reloadContinuationData:{continuation:'private-all'}}},
    ]}}}}}) : response([event(0,[add('all')])]),[profile]);
  const first=await open(library,0,{profile:'one',chat_mode:'live'});
  assert.equal(calls.length,2);assert.equal(calls[1].continuation,'private-all');assert.equal(first.items[0].id,'all');
  await assert.rejects(poll(library,first.cursor,0),{code:'cursor_expired'});
  await assert.rejects(library.execute({action:'chat_send',cursor:first.cursor,profile:'one',id:'abcdefghijk',text:'hello',session:'intent'}),{code:'chat_replay_readonly'});
  for (const value of [undefined,null,-1,Infinity,NaN,'2',1000001])
    await assert.rejects(library.execute({action:'live_chat',id:'abcdefghijk',chat_position_sec:value,profile:'one'}),{code:'chat_playback_required'});
  time(1800001);await assert.rejects(poll(library,first.cursor,1,{profile:'one'}),{code:'cursor_expired'});
});

test('invalid replay timestamps and response limits preserve the committed messages and cursor',async()=>{
  let invalid=false;
  const {library,time}=fixture(()=>response([event(invalid?'bad':0,[add('good')])],'private-next'));
  const first=await open(library);const context=library.chats.cursors.get(first.cursor).context;
  const before=[...context.messages];invalid=true;time(2000);
  await assert.rejects(poll(library,first.cursor,1),{code:'chat_response'});
  assert.deepEqual([...context.messages],before);assert.equal(context.current_cursor,first.cursor);
  assert.equal(context.position,0);invalid=false;
  assert.equal((await poll(library,first.cursor,1)).items[0].id,'good');
  const oversized=fixture(()=>response(new Array(4097).fill(event(0,[]))));
  await assert.rejects(open(oversized.library),{code:'chat_response'});
  assert.equal(oversized.library.chats.contexts.size,0);
});

test('sparse replay catch-up is bounded and does not create background pollers',async()=>{
  const {library,calls,time}=fixture((_,number)=>response([],'private-page-'+number));
  const first=await open(library);assert.equal(calls.length,4);assert.equal(first.ended,false);
  time(2000);const next=await poll(library,first.cursor,0);assert.equal(calls.length,8);
  await library.execute({action:'chat_close',cursor:next.cursor});time(10000);assert.equal(calls.length,8);
});

test('a paused seek keeps catching up across sparse pages until it reaches a future action',async()=>{
  const {library,calls,time}=fixture((_,number)=>number<=4
    ? response([event(number*1000,[add('early-'+number)])],'private-page-'+number)
    : response([event(11000,[add('future-eleven')])]));
  const first=await open(library,10);assert.equal(calls.length,4);
  assert.equal(first.items.length,4);
  time(2000);const caught=await poll(library,first.cursor,10);assert.equal(calls.length,5);
  assert.equal(caught.items.length,4);assert.equal(caught.ended,false);
  time(4000);const paused=await poll(library,caught.cursor,10);assert.equal(calls.length,5);
  const eleven=await poll(library,paused.cursor,11);assert.equal(eleven.items.at(-1).id,'future-eleven');
  assert.equal(eleven.ended,true);
});

test('signed pre-stream replay offsets are visible at frame zero while later actions remain buffered',async()=>{
  const {library}=fixture(()=>response([
    event(-2000,[add('waiting-room')]),event(0,[add('start')]),event(1000,[add('later')]),
  ]));
  const first=await open(library);
  assert.deepEqual(first.items.map(x=>x.id),['waiting-room','start']);
  assert.equal(first.items[0].offset_ms,-2000);
  assert.equal(first.position_ms,0);assert.equal(first.ended,false);
});

test('historical replay pages dense intervals without dropping occurrences or exposing tokens',async()=>{
  const {library,calls}=fixture(()=>response([event(0,Array.from({length:700},(_,i)=>add('message-'+i)))]));
  let result=await open(library,0,{chat_history:true,chat_page:0});
  const ids=[];
  for(let page=0;page<3;++page){
    assert.equal(result.chat_page,page);ids.push(...result.items.map(item=>item.id));
    assert.equal(result.has_more,page<2);assert.equal(result.ended,page===2);
    assert.equal(JSON.stringify(result).includes('private-'),false);
    if(page<2){
      const cursor=result.cursor;
      result=await poll(library,cursor,0,{chat_page:page+1});
      assert.deepEqual(await poll(library,cursor,0,{chat_page:page+1}),result);
    }
  }
  assert.deepEqual(ids,Array.from({length:700},(_,i)=>'message-'+i));assert.equal(calls.length,1);
  await assert.rejects(poll(library,result.cursor,0,{chat_page:3}),{code:'invalid_chat_page'});
  const restart=await poll(library,result.cursor,0,{chat_page:0});
  assert.equal(restart.chat_page,0);assert.equal(restart.items[0].id,'message-0');assert.equal(restart.has_more,true);
});

test('history pages follow upstream continuations and reset even for short seeks',async()=>{
  const {library,calls}=fixture((options,n)=>response([event(Number(options.currentPlayerState.playerOffsetMs),
    Array.from({length:300},(_,i)=>add(`batch-${n}-${i}`)))],n<5?'private-page-'+n:''));
  let result=await open(library,0,{chat_history:true,chat_page:0});const all=[];
  while(true){all.push(...result.items.map(item=>item.id));if(!result.has_more)break;
    result=await poll(library,result.cursor,0,{chat_page:result.chat_page+1});}
  assert.equal(all.length,1500);assert.equal(new Set(all).size,1500);assert.equal(calls.length,5);
  await assert.rejects(poll(library,result.cursor,1,{chat_page:1}),{code:'invalid_chat_page'});
  const seek=await poll(library,result.cursor,1,{chat_page:0});
  assert.equal(seek.chat_page,0);assert.equal(calls.at(-1).continuation,'private-seek');
  assert.equal(seek.items[0].id,'batch-6-0');
});

test('history page byte bounds, ownership and failed fetch preserve the current cursor',async()=>{
  let fail=false;
  const {library}=fixture((_,n)=>{
    if(fail)throw new Error('offline');
    const actions=Array.from({length:100},(_,i)=>{const action=add(`long-${n}-${i}`);
      action.addChatItemAction.item.liveChatTextMessageRenderer.message={runs:[{text:'x'.repeat(3000)}]};return action;});
    return response([event(0,actions)],'private-next-'+n);
  });
  const first=await open(library,0,{chat_history:true,chat_page:0});
  assert(first.items.length<256 && first.has_more);assert(Buffer.byteLength(JSON.stringify(first))<768*1024);
  await assert.rejects(poll(library,first.cursor,0,{chat_page:2}),{code:'invalid_chat_page'});
  await assert.rejects(poll(library,first.cursor,0,{chat_page:1,profile:'another'}));
  fail=true;await assert.rejects(poll(library,first.cursor,0,{chat_page:1}));
  const context=library.chats.cursors.get(first.cursor).context;
  assert.equal(context.current_cursor,first.cursor);assert.equal(context.page,0);
  fail=false;const second=await poll(library,first.cursor,0,{chat_page:1});assert.equal(second.chat_page,1);
});
