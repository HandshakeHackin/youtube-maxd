import test from 'node:test';
import assert from 'node:assert/strict';
import {Library} from './library.mjs';
import {YTNodes,Parser} from 'youtubei.js';
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

const rawVideo=key=>({videoRenderer:{videoId:key,title:{simpleText:key},thumbnail:{thumbnails:[]}}});
const parsed=(kind,rows,target='watch-next-feed')=>Parser.parseResponse({onResponseReceivedEndpoints:[{
  [kind]:{continuationItems:rows,...(kind==='reloadContinuationItemsCommand'?{targetId:target}:{target})}
}]});
function chip(call) {
  const node=new YTNodes.ChipCloudChip({text:{simpleText:'From this channel'},isSelected:false,
    navigationEndpoint:{continuationCommand:{token:'filter-token',request:'CONTINUATION_REQUEST_TYPE_WATCH_NEXT'}}});
  node.endpoint.call=call;return node;
}

test('filter capabilities use parsed reload and continuation nodes without mutating the source group',async()=>{
  let calls=0,continuations=0;
  const filtered=parsed('reloadContinuationItemsCommand',[rawVideo(next),{continuationItemRenderer:{
    trigger:'CONTINUATION_TRIGGER_ON_ITEM_SHOWN',continuationEndpoint:{continuationCommand:{token:'filtered-next'}}}}]);
  filtered.on_response_received_endpoints[0].contents.at(-1).endpoint.call=async()=>{
    continuations++;return parsed('appendContinuationItemsAction',[rawVideo(id)]);
  };
  const info={basic_info:{id},watch_next_feed:[card(id)],wn_has_continuation:false,
    related_chip_cloud:{chips:[chip(async()=>{calls++;return filtered;})]}};
  const library=new Library(async()=>({getInfo:async()=>info}),[{id:'other',cookie:''}]);
  const root=await library.execute({action:'suggestions',id});
  assert(root.items[0].is_folder);assert.equal(root.items[1].playback_context.index,1);
  const key=root.items[0].id.slice('library:filter:'.length);
  await assert.rejects(library.execute({action:'suggestions_filter',id:key,profile:'other'}),{code:'cursor_expired'});
  const [a,b]=await Promise.all([library.execute({action:'suggestions_filter',id:key}),library.execute({action:'suggestions_filter',id:key})]);
  assert.equal(calls,1);assert.equal(a.items[0].id,next);
  assert.deepEqual(a.items[0].playback_context,b.items[0].playback_context);
  assert.notEqual(a.items[0].playback_context.group,root.items[1].playback_context.group);
  const tail=await library.execute({action:'continue',cursor:a.cursor});
  assert.equal(tail.items[0].id,id);assert.equal(tail.items[0].playback_context.index,1);assert.equal(continuations,1);
  assert.equal(info.watch_next_feed[0].id,id);assert.equal(info.wn_has_continuation,false);
});

test('filter references expire during requests and failed loads can retry',async()=>{
  let time=0,finish,calls=0;
  const info={basic_info:{id},watch_next_feed:[],wn_has_continuation:false,related_chip_cloud:{chips:[chip(async()=>{
    calls++;if(calls===1)throw new Error('temporary');
    await new Promise(resolve=>{finish=resolve;});return parsed('reloadContinuationItemsCommand',[rawVideo(next)]);
  })]}};
  const library=new Library(async()=>({getInfo:async()=>info}),[],()=>time);
  const root=await library.execute({action:'suggestions',id}),key=root.items[0].id.slice(15);
  await assert.rejects(library.execute({action:'suggestions_filter',id:key}),/temporary/);
  const pending=library.execute({action:'suggestions_filter',id:key});
  while(!finish)await new Promise(resolve=>setImmediate(resolve));
  time=120001;finish();await assert.rejects(pending,{code:'cursor_expired'});
  await assert.rejects(library.execute({action:'suggestions_filter',id:key}),{code:'cursor_expired'});assert.equal(calls,2);
});

test('filtered feeds reject unrelated reload targets and ambiguous continuation tokens',async()=>{
  let response=parsed('reloadContinuationItemsCommand',[rawVideo(next)],'comments-section');
  const info={basic_info:{id},watch_next_feed:[],related_chip_cloud:{chips:[chip(async()=>response)]}};
  const library=new Library(async()=>({getInfo:async()=>info}));
  const root=await library.execute({action:'suggestions',id}),key=root.items[0].id.slice(15);
  await assert.rejects(library.execute({action:'suggestions_filter',id:key}),{code:'upstream_format'});
  response=parsed('reloadContinuationItemsCommand',[{continuationItemRenderer:{continuationEndpoint:{}}},{continuationItemRenderer:{continuationEndpoint:{}}}]);
  await assert.rejects(library.execute({action:'suggestions_filter',id:key}),{code:'upstream_format'});
});

const shelf=(title,items,kind='horizontalListRenderer')=>new YTNodes.Shelf({title:{simpleText:title},content:{[kind]:{items}}});
test('suggestion shelves remain independent profile-owned groups with duplicate occurrences',async()=>{
  const info={basic_info:{id},watch_next_feed:[shelf('Channel videos',[rawVideo(next),rawVideo(next)]),card(id)],wn_has_continuation:false};
  const library=new Library(async()=>({getInfo:async()=>info}),[{id:'other',cookie:''}]);
  const root=await library.execute({action:'suggestions',id});
  assert.equal(root.items[0].title,'Channel videos');assert(root.items[0].is_folder);
  assert.deepEqual(root.items.filter(row=>!row.is_folder).map(row=>row.id),[id]);
  const key=root.items[0].id.slice(15);
  await assert.rejects(library.execute({action:'suggestions_filter',profile:'other',id:key}),{code:'cursor_expired'});
  const child=await library.execute({action:'suggestions_filter',id:key});
  assert.deepEqual(child.items.map(row=>row.id),[next,next]);
  assert.deepEqual(child.items.map(row=>row.playback_context.index),[0,1]);
  assert.notEqual(child.items[0].playback_context.group,root.items[1].playback_context.group);
  assert.deepEqual(await library.execute({action:'suggestions_filter',id:key}),child);
});

test('shelves on source and filtered continuations stay browsable without entering their parent video group',async()=>{
  let calls=0;
  const info={basic_info:{id},watch_next_feed:[card(id)],wn_has_continuation:true,
    async getWatchNextContinuation(){calls++;this.watch_next_feed=[shelf('More',[rawVideo(next)],'expandedShelfContentsRenderer')];this.wn_has_continuation=false;return this;},
    related_chip_cloud:{chips:[chip(async()=>parsed('reloadContinuationItemsCommand',[
      {shelfRenderer:{title:{simpleText:'Filtered section'},content:{verticalListRenderer:{items:[rawVideo(next)]}}}}
    ]))]}};
  const library=new Library(async()=>({getInfo:async()=>info}));
  const root=await library.execute({action:'suggestions',id});
  const tail=await library.execute({action:'continue',cursor:root.cursor});
  assert.equal(calls,1);assert(tail.items[0].is_folder);
  const opened=await library.execute({action:'suggestions_filter',id:tail.items[0].id.slice(15)});assert.equal(opened.items[0].id,next);
  const filtered=await library.execute({action:'suggestions_filter',id:root.items[0].id.slice(15)});
  assert(filtered.items[0].is_folder && filtered.items[0].title==='Filtered section');
  const child=await library.execute({action:'suggestions_filter',id:filtered.items[0].id.slice(15)});assert.equal(child.items[0].id,next);
  await assert.rejects(library.execute({action:'group_window',group:root.items[1].playback_context.group,id,index:1,target:2,direction:1}),{code:'group_unavailable'});
});

test('oversized and deeply nested suggestion shelves fail within bounds',async()=>{
  let rows=[rawVideo(next)];
  for(let i=0;i<6;i++)rows=[{shelfRenderer:{title:{simpleText:'Nested'},content:{horizontalListRenderer:{items:rows}}}}];
  const info={basic_info:{id},watch_next_feed:[Parser.parseItem(rows[0])],wn_has_continuation:false};
  const library=new Library(async()=>({getInfo:async()=>info}));
  let page=await library.execute({action:'suggestions',id});
  for(let i=0;i<3;i++)page=await library.execute({action:'suggestions_filter',id:page.items[0].id.slice(15)});
  await assert.rejects(library.execute({action:'suggestions_filter',id:page.items[0].id.slice(15)}),{code:'upstream_limit'});
  info.watch_next_feed=[shelf('Too large',Array(4097).fill(rawVideo(id)))];
  await assert.rejects(library.execute({action:'suggestions',id}),{code:'upstream_limit'});
});

test('parsed grid cards in shelves retain duration, live markers and watch progress',async()=>{
  const grid={gridVideoRenderer:{videoId:next,title:{simpleText:'Grid stream'},thumbnail:{thumbnails:[]},
    lengthText:{simpleText:'1:02:03'},thumbnailOverlays:[
      {thumbnailOverlayTimeStatusRenderer:{style:'LIVE',text:{simpleText:'En directo'}}},
      {thumbnailOverlayResumePlaybackRenderer:{percentDurationWatched:50}}]}};
  const info={basic_info:{id},watch_next_feed:[shelf('Grid videos',[grid])],wn_has_continuation:false};
  const library=new Library(async()=>({getInfo:async()=>info}));
  const root=await library.execute({action:'suggestions',id});
  const page=await library.execute({action:'suggestions_filter',id:root.items[0].id.slice(15)});
  assert.equal(page.items[0].duration_sec,3723);assert.equal(page.items[0].is_live,true);assert.equal(page.items[0].watch_percent,50);
  grid.gridVideoRenderer.lengthText.simpleText='2:99';grid.gridVideoRenderer.thumbnailOverlays=[];
  info.watch_next_feed=[shelf('Grid videos',[grid])];
  const invalid=await library.execute({action:'suggestions',id});
  const child=await library.execute({action:'suggestions_filter',id:invalid.items[0].id.slice(15)});
  assert.equal(child.items[0].duration_sec,0);assert.equal(child.items[0].is_live,false);
});

test('a shelf continuation belongs to that shelf and shares browser/playback pagination',async()=>{
  const section=shelf('Paged section',[rawVideo(next),{continuationItemRenderer:{
    trigger:'CONTINUATION_TRIGGER_ON_ITEM_SHOWN',continuationEndpoint:{continuationCommand:{token:'section-next'}}}}]);
  let calls=0;
  section.content.items.at(-1).endpoint.call=async()=>{calls++;return parsed('appendContinuationItemsAction',[rawVideo(next),rawVideo(id)]);};
  const library=new Library(async()=>({getInfo:async()=>({basic_info:{id},watch_next_feed:[section,card(id)],wn_has_continuation:false})}));
  const root=await library.execute({action:'suggestions',id});
  const child=await library.execute({action:'suggestions_filter',id:root.items[0].id.slice(15)});
  const group=child.items[0].playback_context.group;
  const [window,tail]=await Promise.all([
    library.execute({action:'group_window',group,id:next,index:0,target:1,direction:1}),
    library.execute({action:'continue',cursor:child.cursor})]);
  assert.equal(calls,1);assert.equal(window.video_id,next);assert.equal(window.index,1);
  assert.deepEqual(tail.items.map(row=>row.playback_context.index),[1,2]);
  assert.equal(root.has_more,false);
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
