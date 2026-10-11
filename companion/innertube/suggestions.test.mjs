import test from 'node:test';
import assert from 'node:assert/strict';
import {Library} from './library.mjs';
import {YTNodes} from 'youtubei.js';
const id='abcdefghijk',next='bcdefghijkl';
const card=(key,percent=0)=>new YTNodes.Video({videoId:key,title:{simpleText:key},thumbnail:{thumbnails:[]},
  thumbnailOverlays:[{thumbnailOverlayResumePlaybackRenderer:{percentDurationWatched:percent}}]});

test('related videos filter watched cards and preserve duplicate navigation across mutable continuations',async()=>{
  let calls=0;
  const info={basic_info:{id},watch_next_feed:[card(next),card(id,96),card(next),{type:'AdSlot'},
    {type:'Shelf',contents:[card(id)]}],wn_has_continuation:true,
    async getWatchNextContinuation(){calls++;this.watch_next_feed=[card(id,50)];this.wn_has_continuation=false;return this;}};
  const library=new Library(async profile=>({getInfo:async key=>{assert.equal(profile.id,'account');assert.equal(key,id);return info;}}),
    [{id:'account',cookie:'synthetic'}]);
  const page=await library.execute({action:'suggestions',profile:'account',id});
  assert.deepEqual(page.items.map(row=>row.id),[next,next]);
  assert.deepEqual(page.items.map(row=>row.playback_context.index),[0,1]);
  await assert.rejects(library.execute({action:'continue',cursor:page.cursor}),{code:'cursor_expired'});
  const group=page.items[0].playback_context.group;
  const [window,tail]=await Promise.all([
    library.execute({action:'group_window',profile:'account',group,id:next,index:1,target:2,direction:1}),
    library.execute({action:'continue',profile:'account',cursor:page.cursor})]);
  assert.equal(calls,1);assert.equal(window.video_id,id);assert.equal(window.index,2);
  assert.equal(tail.items[0].playback_context.index,2);assert.equal(tail.has_more,false);
});

test('suggestions reject invalid targets, mismatched identity and unknown layouts',async()=>{
  let calls=0,info={basic_info:{id:next},watch_next_feed:[]};
  const library=new Library(async()=>({getInfo:async()=>{calls++;return info;}}));
  await assert.rejects(library.execute({action:'suggestions',id:'bad'}),{code:'invalid_identity'});assert.equal(calls,0);
  await assert.rejects(library.execute({action:'suggestions',id}),{code:'upstream_format'});
  for(const feed of [undefined,{},Array(4097).fill(card(id))]) {
    info={basic_info:{id},watch_next_feed:feed};
    await assert.rejects(library.execute({action:'suggestions',id}),{code:'upstream_format'});
  }
  info={basic_info:{id},watch_next_feed:[],wn_has_continuation:true};
  await assert.rejects(library.execute({action:'suggestions',id}),{code:'upstream_format'});
  info.wn_has_continuation=false;
  assert.deepEqual((await library.execute({action:'suggestions',id})).items,[]);
});
