// Occurrence identity through real parser nodes and bounded service pagination.
import test from 'node:test';
import assert from 'node:assert/strict';
import {YTNodes} from 'youtubei.js';
import {Library, normalizeVideo} from './library.mjs';
const playlist = 'PLabcdefghijk', id = 'abcdefghijk';
const channel = 'UC'+'a'.repeat(22);
function node(index, list = playlist) {
  return new YTNodes.PlaylistVideo({videoId:id,index:{simpleText:String(index+1)},
    title:{simpleText:'Repeated video',accessibility:{accessibilityData:{label:'Repeated video'}}},
    shortBylineText:{runs:[{text:'Channel'}]},thumbnail:{thumbnails:[]},thumbnailOverlays:[],
    lengthSeconds:'10',isPlayable:true,
    navigationEndpoint:{watchEndpoint:{videoId:id,playlistId:list,index}}});
}
test('OAuth channels use Videos and retain source positions through filtered slots and pagination', async()=>{
  let videosCalls=0,continuations=0;
  const raw=Array.from({length:30},(_,i)=>node(i));
  raw[3]={};
  const feed={videos:raw,has_continuation:true,async getContinuation(){
    continuations++;return {videos:[node(0),node(1)],has_continuation:false};
  }};
  const oauth={access_token:'synthetic',refresh_token:'synthetic',expiry_date:'2099-01-01T00:00:00.000Z',
    client:{client_id:'synthetic',client_secret:'synthetic'}};
  const library=new Library(async()=>({getChannel:async id=>{
    assert.equal(id,channel);
    return {videos:[node(999)],async getVideos(){videosCalls++;return feed;}};
  }}),[{id:'signed',cookie:'',oauth},{id:'other',cookie:'',oauth}]);
  const a=await library.playback({action:'catalog',kind:'channel',id:channel,profile:'signed'});
  assert.equal(videosCalls,1);
  assert.deepEqual(a.items.map(x=>x.playback_context),Array.from({length:25},(_,index)=>({channel,index})).filter(x=>x.index!==3));
  await assert.rejects(library.execute({action:'continue',cursor:a.cursor,profile:'other'}),{code:'cursor_expired'});
  const b=await library.execute({action:'continue',cursor:a.cursor,profile:'signed'});
  assert.deepEqual(b.items.map(x=>x.playback_context.index),[25,26,27,28,29]);
  assert.equal(continuations,0);
  assert.strictEqual(await library.execute({action:'continue',cursor:a.cursor,profile:'signed'}),b);
  const c=await library.execute({action:'continue',cursor:b.cursor,profile:'signed'});
  assert.deepEqual(c.items.map(x=>x.playback_context),[{channel,index:30},{channel,index:31}]);
  assert.equal(continuations,1);assert.equal(c.has_more,false);
});
test('actual playlist/lockup parsers retain explicit occurrence positions, not video identity alone', () => {
  for(const index of [0,4,100000]) {
    const item = normalizeVideo(node(index),playlist);
    assert.equal(item.id,id);
    assert.deepEqual(item.playback_context,{playlist,index});
  }
  const classic=node(4);delete classic.endpoint.payload.index;
  assert.deepEqual(normalizeVideo(classic,playlist).playback_context,{playlist,index:4});
  const lockup=new YTNodes.LockupView({contentId:id,contentType:'LOCKUP_CONTENT_TYPE_VIDEO',
    rendererContext:{commandContext:{onTap:{watchEndpoint:{videoId:id,playlistId:playlist,index:7}}}}});
  assert.deepEqual(normalizeVideo(lockup,playlist).playback_context,{playlist,index:7});
  assert.equal(normalizeVideo(classic).playback_context,undefined);
});
test('conflicting identities and unusable positions cannot become playback context', () => {
  for(const index of [null,true,-1,1.5,NaN,100001,'0']) {
    const video=node(0);video.endpoint.payload.index=index;
    assert.equal(normalizeVideo(video,playlist).playback_context,undefined);
  }
  for(const change of [{videoId:'12345678901'},{playlistId:'PLdifferent11'},{playlistIndex:9}]) {
    const video=node(0);Object.assign(video.endpoint.payload,change);
    assert.equal(normalizeVideo(video,playlist).playback_context,undefined);
  }
  const video=node(0);delete video.endpoint.payload.index;video.index='unknown';
  assert.equal(normalizeVideo(video,playlist).playback_context,undefined);
});
test('duplicates and sparse source positions survive local pages and upstream continuations with owner isolation', async () => {
  let calls=0;
  const first={videos:Array.from({length:30},(_,i)=>node(i)),has_continuation:true,
    async getContinuation(){calls++;return {videos:[node(40),node(64)],has_continuation:false};}};
  const library=new Library(async()=>({getPlaylist:async()=>first,getHomeFeed:async()=>first}),[{id:'two',cookie:'synthetic'}]);
  const a=await library.execute({action:'playlist',id:playlist});
  assert.equal(a.items.length,24);assert.ok(a.items.every(item=>item.id===id));
  assert.deepEqual(a.items.map(item=>item.playback_context.index),Array.from({length:24},(_,i)=>i));
  await assert.rejects(library.execute({action:'continue',cursor:a.cursor,profile:'two'}),{code:'cursor_expired'});
  const home=await library.execute({action:'home'});assert.ok(home.items.every(item=>!item.playback_context));
  const b=await library.execute({action:'continue',cursor:a.cursor});
  assert.deepEqual(b.items.map(item=>item.playback_context.index),[24,25,26,27,28,29]);assert.equal(calls,0);
  assert.strictEqual(await library.execute({action:'continue',cursor:a.cursor}),b);
  const c=await library.execute({action:'continue',cursor:b.cursor});
  assert.deepEqual(c.items.map(item=>item.playback_context),[{playlist,index:40},{playlist,index:64}]);
  assert.equal(c.has_more,false);assert.equal(calls,1);
});
test('private watch-later and OAuth catalog entry points also retain occurrence context', async()=>{
  const oauth={access_token:'synthetic',refresh_token:'synthetic',expiry_date:'2099-01-01T00:00:00.000Z',
    client:{client_id:'synthetic',client_secret:'synthetic'}};
  const library=new Library(async()=>({getPlaylist:async list=>({videos:[node(8,list)],has_continuation:false})}),
    [{id:'signed',cookie:'',oauth}]);
  const a=await library.execute({action:'watch_later',profile:'signed'});
  assert.deepEqual(a.items[0].playback_context,{playlist:'WL',index:8});
  const b=await library.playback({action:'catalog',kind:'playlist',id:playlist,profile:'signed'});
  assert.deepEqual(b.items[0].playback_context,{playlist,index:8});
});
