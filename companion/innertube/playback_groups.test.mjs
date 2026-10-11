import test from 'node:test';
import assert from 'node:assert/strict';
import {PlaybackGroups} from './playback_groups.mjs';
class Failure extends Error {constructor(code,message){super(message);this.code=code;}}
const id='abcdefghijk',other='bcdefghijkl',last='cdefghijklm';
const row=(id,extra={})=>({id,channel_id:'UC'+'a'.repeat(22),subtitle:'Author',...extra});
const groups=clock=>new PlaybackGroups(Failure,feed=>feed.items,clock);
test('source windows preserve repeated occurrences and skip folders/upcoming entries in both directions',async()=>{
  const store=groups(),feed={has_continuation:false};
  const {key,items}=store.attach('guest',feed,[row(id),row(other,{is_upcoming:true}),row('folder',{is_folder:true}),row(id),row(last)]);
  assert.equal(items[3].playback_context.index,3);
  let result=await store.window('guest',{group:key,id,index:0,target:1,direction:1},{});
  assert.equal(result.index,3);assert.equal(result.skipped_unavailable,2);assert.deepEqual(result.video_indices,[0,3,4]);
  result=await store.window('guest',{group:key,id:last,index:4,target:3,direction:-1},{});
  assert.equal(result.index,3);assert.equal(result.playlist_size,5);
  await assert.rejects(store.window('guest',{group:key,id:other,index:0},{}),{code:'group_changed'});
});
test('concurrent navigation shares one continuation and appends it only once',async()=>{
  const store=groups();let calls=0;
  const next={items:[row(other)],has_continuation:false};
  const feed={has_continuation:true,async getContinuation(){calls++;await new Promise(resolve=>setImmediate(resolve));return next;}};
  const {key}=store.attach('guest',feed,[row(id)]);
  const input={group:key,id,index:0,target:1,direction:1};
  const results=await Promise.all([store.window('guest',input,{}),store.window('guest',input,{})]);
  assert.equal(calls,1);assert.ok(results.every(result=>result.index===1 && result.playlist_size===2));
  assert.equal(store.attach('guest',next,next.items,key).items[0].playback_context.index,1);
});
test('group ownership expiry eviction and anchor validation prevent unrelated navigation',async()=>{
  let now=0;const store=groups(()=>now),feed={has_continuation:false};
  const {key}=store.attach('one',feed,[row(id)]);
  await assert.rejects(store.window('two',{group:key,id,index:0},{}),{code:'group_expired'});
  now=21600001;await assert.rejects(store.window('one',{group:key,id,index:0},{}),{code:'group_expired'});
  const first=store.attach('one',feed,[row(id)]).key;
  for(let i=0;i<32;i++)store.attach('one',{},[row(id)]);
  assert.equal(store.groups.size,32);assert.ok(!store.groups.has(first));
  store.removeOwner('one');assert.equal(store.groups.size,0);
});
test('navigation attempts at most one continuation and can retry a failed upstream call',async()=>{
  const store=groups();let calls=0;
  const feed={has_continuation:true,async getContinuation(){calls++;throw Error('offline');}};
  const {key}=store.attach('guest',feed,[row(id)]),input={group:key,id,index:0,target:1,direction:1};
  await assert.rejects(store.window('guest',input,{}),/offline/);
  feed.getContinuation=async()=>{calls++;return {has_continuation:true,items:[row(other,{is_upcoming:true})]};};
  await assert.rejects(store.window('guest',input,{}),{code:'group_unavailable'});assert.equal(calls,2);
});

import {Library} from './library.mjs';
test('catalog continuation and playback share source identity without duplicate upstream work',async()=>{
  let calls=0;
  const feed={videos:Array.from({length:25},()=>({id,author:{id:'UC'+'a'.repeat(22),name:'Author'}})),has_continuation:true,
    async getContinuation(){calls++;return {videos:[{id:other}],has_continuation:false};}};
  const library=new Library(async()=>({getHomeFeed:async()=>feed}));
  const a=await library.execute({action:'home'}),group=a.items[0].playback_context.group;
  const b=await library.execute({action:'continue',cursor:a.cursor});
  assert.deepEqual(b.items[0].playback_context,{group,index:24});
  const window=await library.execute({action:'group_window',group,id,index:24,target:25,direction:1});
  assert.equal(window.video_id,other);assert.equal(window.index,25);
  const c=await library.execute({action:'continue',cursor:b.cursor});
  assert.deepEqual(c.items[0].playback_context,{group,index:25});assert.equal(calls,1);
});

test('catalog browsing can start a fresh group after retained capacity or eviction',()=>{
  const store=groups(),feed={},rows=Array.from({length:4096},()=>row(id));
  const {key}=store.attach('guest',feed,rows);
  assert.throws(()=>store.attach('guest',{},[row(other)],key),{code:'group_limit'});
  const fresh=store.attach('guest',{},[row(other)],key,true);
  assert.notEqual(fresh.key,key);assert.equal(fresh.items[0].playback_context.index,0);
  store.groups.delete(fresh.key);
  assert.notEqual(store.attach('guest',{},[row(other)],fresh.key,true).key,fresh.key);
});

test('Guest search catalogs retain groups without enabling credential access',async()=>{
  const library=new Library(async profile=>({search:async query=>{assert.equal(profile.id,'guest');assert.equal(query,'sample');
    return {videos:[{id},{id:other}],has_continuation:false};}}));
  const page=await library.playback({action:'catalog',kind:'search',query:'sample',profile:'guest'});
  const group=page.items[0].playback_context.group;
  const next=await library.execute({action:'group_window',group,id,index:0,target:1,direction:1});
  assert.equal(next.video_id,other);
  await assert.rejects(library.playback({action:'credentials',profile:'guest'}),{code:'playback_account_context'});
  await assert.rejects(library.playback({action:'catalog',kind:'search',query:'sample',profile:'missing'}),{code:'unknown_profile'});
});
