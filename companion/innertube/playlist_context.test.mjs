// Occurrence identity through real parser nodes and bounded service pagination.
import test from 'node:test';
import assert from 'node:assert/strict';
import {YTNodes} from 'youtubei.js';
import {Library, normalizeVideo} from './library.mjs';
const playlist = 'PLabcdefghijk', id = 'abcdefghijk';
const channel = 'UC'+'a'.repeat(22);
test('channel navigation dispatch uses the selected profile and real video normalization',async()=>{
  const owners=[];
  const library=new Library(async profile=>{
    owners.push(profile.id);
    return {async getChannel(key){assert.equal(key,channel);return {async getVideos(){
      return {videos:[node(0),node(1),node(2)],has_continuation:false};
    }};}};
  },[{id:'signed',cookie:'synthetic'}]);
  for(const profile of ['guest','signed']) {
    const result=await library.execute({action:'channel_window',profile,channel,id,index:1,target:2});
    assert.equal(result.anchor_index,1);assert.equal(result.index,2);
    assert.deepEqual(result.video_indices,[0,1,2]);assert.equal(result.playlist_size,3);
  }
  assert.deepEqual(owners,['guest','signed']);
  await assert.rejects(library.execute({action:'channel_window',profile:'missing',channel,id,index:1}),{code:'unknown_profile'});
});
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
  const home=await library.execute({action:'home'});assert.ok(home.items.every(item=>item.playback_context?.group && !item.playback_context.playlist));
  const b=await library.execute({action:'continue',cursor:a.cursor});
  assert.deepEqual(b.items.map(item=>item.playback_context.index),[24,25,26,27,28,29]);assert.equal(calls,0);
  assert.strictEqual(await library.execute({action:'continue',cursor:a.cursor}),b);
  const c=await library.execute({action:'continue',cursor:b.cursor});
  assert.deepEqual(c.items.map(item=>item.playback_context),[{playlist,index:40},{playlist,index:64}]);
  assert.equal(c.has_more,false);assert.equal(calls,1);
});
// Since f8617d2 a TV sign-in reads its lists from the TV browse page (/browse VL<list>): TV tiles,
// or the web playlist layout through the pinned parser when the page has no tiles.
// The tile is the one of fixtures/tv-home-shape.json (MediaServiceCore 7f36935), reduced.
function accountPlaylist(shape, list) {
  if(shape==='tv') {
    const tile={contentType:'TILE_CONTENT_TYPE_VIDEO',contentId:id,onSelectCommand:{watchEndpoint:{videoId:id,playlistId:list,index:8}},
      header:{tileHeaderRenderer:{thumbnail:{thumbnails:[{url:`https://i.ytimg.com/vi/${id}/hqdefault.jpg`,width:480,height:360}]},
        thumbnailOverlays:[{thumbnailOverlayTimeStatusRenderer:{text:{simpleText:'0:10'},style:'DEFAULT'}}]}},
      metadata:{tileMetadataRenderer:{title:{simpleText:'Repeated video'},
        lines:[{lineRenderer:{items:[{lineItemRenderer:{text:{runs:[{text:'Channel'}]}}}]}}]}}};
    return {contents:{tvBrowseRenderer:{content:{tvSurfaceContentRenderer:{content:{
      twoColumnRenderer:{rightColumn:{playlistVideoListRenderer:{contents:[{tileRenderer:tile}]}}}}}}}}};
  }
  const card={videoId:id,title:{simpleText:'Repeated video',accessibility:{accessibilityData:{label:'Repeated video'}}},
    index:{simpleText:'9'},shortBylineText:{runs:[{text:'Channel'}]},lengthSeconds:'10',isPlayable:true,
    thumbnail:{thumbnails:[]},navigationEndpoint:{watchEndpoint:{videoId:id,playlistId:list,index:8}}};
  return {contents:{twoColumnBrowseResultsRenderer:{tabs:[{tabRenderer:{selected:true,content:{sectionListRenderer:{contents:[
    {itemSectionRenderer:{contents:[{playlistVideoListRenderer:{playlistId:list,contents:[{playlistVideoRenderer:card}]}}]}}]}}}}]}}};
}
for(const shape of ['tv','legacy'])
  test(`private watch-later and OAuth catalog entry points also retain occurrence context (${shape} page)`, async()=>{
    const oauth={access_token:'synthetic',refresh_token:'synthetic',expiry_date:'2099-01-01T00:00:00.000Z',
      client:{client_id:'synthetic',client_secret:'synthetic'}};
    const browsed=[];
    const library=new Library(async()=>({session:{logged_in:true},actions:{execute:async(path,args)=>{
      assert.equal(path,'/browse');assert.match(args.browseId,/^VL/);browsed.push(args.browseId);
      return {success:true,status_code:200,data:accountPlaylist(shape,args.browseId.slice(2))};
    }},
    // A playlist opened for playback still goes through the pinned web getter.
    getPlaylist:async list=>({videos:[node(8,list)],has_continuation:false})}),[{id:'signed',cookie:'',oauth}]);
    const a=await library.execute({action:'watch_later',profile:'signed'});
    assert.deepEqual(a.items[0].playback_context,{playlist:'WL',index:8});
    const b=await library.playback({action:'catalog',kind:'playlist',id:playlist,profile:'signed'});
    assert.deepEqual(b.items[0].playback_context,{playlist,index:8});
    const c=await library.execute({action:'playlist',id:playlist,profile:'signed'});
    assert.deepEqual(c.items[0].playback_context,{playlist,index:8});
    assert.deepEqual(browsed,['VLWL','VL'+playlist]);
  });
