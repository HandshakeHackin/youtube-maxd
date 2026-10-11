import test from 'node:test';
import assert from 'node:assert/strict';
import {TopicCatalog} from './topic_catalog.mjs';
import { Library, normalizeVideo, imageUrl } from './library.mjs';
const tile = n => ({ tileRenderer: { contentType: 'TILE_CONTENT_TYPE_VIDEO', contentId: String(n).padStart(11, '0'),
  onSelectCommand: { watchEndpoint: { videoId: String(n).padStart(11, '0') } },
  header: { tileHeaderRenderer: { thumbnail: { thumbnails: [{ url: 'https://i.ytimg.com/vi/test/default.jpg' }] },
    thumbnailOverlays: [{ thumbnailOverlayTimeStatusRenderer: { text: { simpleText: '2:03' } } }] } },
  metadata: { tileMetadataRenderer: { title: { simpleText: `Video ${n}` }, lines: [] } } } });
const cont = token => ({ continuations: [{ nextContinuationData: { continuation: token } }] });
const shelf = (items, token='') => ({ shelfRenderer: { title: { simpleText: 'Curated row' },
  content: { horizontalListRenderer: { items, ...(token ? cont(token) : {}) } } } });
const response = data => ({ success: true, status_code: 200, data });
test('TV topics preserve shelves and paginate both rows and root with profile-owned capabilities', async () => {
  const calls=[]; let time=0;
  const client={actions:{execute:async (path,args) => {
    calls.push(args); assert.equal(path,'/browse'); assert.equal(args.client,'TV');
    if(args.continuation==='row-next') return response({continuationContents:{horizontalListContinuation:{items:[tile(30)]}}});
    if(args.continuation==='root-next') return response({continuationContents:{sectionListContinuation:{contents:[shelf([tile(40)])]}}});
    return response({tvBrowseRenderer:{content:{sectionListRenderer:{contents:[shelf(Array.from({length:30},(_,i)=>tile(i)),'row-next')],...cont('root-next')}}}});
  }}};
  const library=new Library(async()=>client,[{id:'other',cookie:''}],()=>time);
  const root=await library.execute({action:'topics',section:'music'});
  assert.equal(calls[0].browseId,'FEtopics_music'); assert.equal(root.items.length,1);
  assert.equal(root.items[0].title,'Curated row'); assert(root.items[0].is_folder);
  const id=root.items[0].id.split(':')[2];
  await assert.rejects(library.execute({action:'topic_open',section:'music',id,profile:'other'}),{code:'cursor_expired'});
  await assert.rejects(library.execute({action:'topic_open',section:'gaming',id}),{code:'cursor_expired'});
  let page=await library.execute({action:'topic_open',section:'music',id}); const ids=[];
  assert.equal(page.items[0].duration_sec,123);
  assert.equal(calls.length,1); // Opening retained rows needs no upstream request.
  while(true) {ids.push(...page.items.map(item=>item.id));if(!page.has_more)break;
    const input={action:'continue',cursor:page.cursor}; page=await library.execute(input);
    assert.deepEqual(await library.execute(input),page);
  }
  assert.deepEqual(ids,Array.from({length:31},(_,i)=>String(i).padStart(11,'0')));
  assert.equal(calls.length,2);
  const nextRoot=await library.execute({action:'continue',cursor:root.cursor}); assert(nextRoot.items[0].is_folder);
  time=600001; await assert.rejects(library.execute({action:'topic_open',section:'music',id}),{code:'cursor_expired'});
  await assert.rejects(library.execute({action:'topics',section:'__proto__'}),{code:'invalid_catalog'});
});
test('TV topic tiles handle carousel metadata, live status, ads and bounded playlist endpoints', async () => {
  const carousel=tile(1); delete carousel.tileRenderer.metadata;
  carousel.tileRenderer.onFocusCommand={updateCarouselHeaderCommand:{spotlight:{entityMetadataRenderer:{title:{runs:[{text:'Carousel title'}]}}}}};
  carousel.tileRenderer.header.tileHeaderRenderer.thumbnailOverlays[0].thumbnailOverlayTimeStatusRenderer.style='LIVE';
  const ad=tile(2); ad.tileRenderer.adSlotMetadata={};
  const playlist={tileRenderer:{contentType:'TILE_CONTENT_TYPE_PLAYLIST',metadata:{tileMetadataRenderer:{title:{simpleText:'Playlist'}}},onSelectCommand:{browseEndpoint:{browseId:'VLPLabcdefghij'}}}};
  const unsafe=structuredClone(playlist); unsafe.tileRenderer.onSelectCommand.browseEndpoint.browseId='https://evil.invalid';
  const calls=[]; const client={actions:{execute:async (_,args)=>{
    calls.push(args); return response(args.browseId==='VLPLabcdefghij' ? {playlistVideoListRenderer:{contents:[tile(7)]}} :
      {sectionListRenderer:{contents:[shelf([carousel,ad,playlist,unsafe,tile(1)])]}});
  }}};
  const library=new Library(async()=>client);
  const root=await library.execute({action:'topics',section:'news'});
  const row=await library.execute({action:'topic_open',section:'news',id:root.items[0].id.split(':')[2]});
  assert.equal(row.items.length,2); assert.equal(row.items[0].title,'Carousel title'); assert(row.items[0].is_live);
  const children=await library.execute({action:'topic_open',section:'news',id:row.items[1].id.split(':')[2]});
  assert.equal(children.items[0].id,'00000000007'); assert.equal(calls[1].browseId,'VLPLabcdefghij');
});

test('tracking-only continuation ends a row, while invalid initial layouts remain errors', async () => {
  let invalid=false;
  const library=new Library(async()=>({actions:{execute:async (_,args)=>response(
    invalid || args.continuation ? {responseContext:{},trackingParams:'end'} :
      {sectionListRenderer:{contents:[shelf([tile(1)],'end')]}})
  }}));
  const root=await library.execute({action:'topics',section:'gaming'});
  const row=await library.execute({action:'topic_open',section:'gaming',id:root.items[0].id.split(':')[2]});
  const end=await library.execute({action:'continue',cursor:row.cursor}); assert.deepEqual(end.items,[]); assert(!end.has_more);
  invalid=true; await assert.rejects(library.execute({action:'topics',section:'gaming'}),{code:'upstream_format'});
});

test('authenticated Music prepends a retained liked row with account-owned pagination', async () => {
  const calls=[];
  const library=new Library(async profile=>({session:{logged_in:true},actions:{execute:async (_,args)=>{
    calls.push({owner:profile.id,...args});
    if(args.browseId) return response({sectionListRenderer:{contents:[shelf([tile(99)])]}});
    return response({continuationContents:{horizontalListContinuation:{items:[tile(args.continuation==='liked-next'?2:1)],
      ...(args.continuation==='liked-next'?{}:cont('liked-next'))}}});
  }}}),[{id:'signed',cookie:'SID=test'},{id:'other',cookie:'SID=other'}]);
  const root=await library.execute({action:'topics',section:'music',profile:'signed'});
  assert.deepEqual(root.items.map(item=>item.title),['Liked music','Curated row']);
  const id=root.items[0].id.split(':')[2];
  assert.equal(calls[1].continuation,'4qmFsgIWEhRGRW11c2ljX2xpa2VkX3ZpZGVvcw%3D%3D');
  const row=await library.execute({action:'topic_open',section:'music',id,profile:'signed'});
  assert.equal(row.items[0].id,'00000000001'); assert.equal(calls.length,2);
  await assert.rejects(library.execute({action:'topic_open',section:'music',id,profile:'guest'}),{code:'cursor_expired'});
  await assert.rejects(library.execute({action:'continue',cursor:row.cursor,profile:'other'}),{code:'cursor_expired'});
  const next=await library.execute({action:'continue',cursor:row.cursor,profile:'signed'});
  assert.equal(next.items[0].id,'00000000002'); assert.equal(calls.at(-1).client,'TV');
  assert.equal(calls.at(-1).owner,'signed');
  const guest=await library.execute({action:'topics',section:'music'});
  assert.deepEqual(guest.items.map(item=>item.title),['Curated row']);
});

test('liked Music WEB fallback retains playlist metadata and WEB continuations', async () => {
  const calls=[];
  const web=n=>({playlistVideoRenderer:{videoId:String(n).padStart(11,'0'),title:{runs:[{text:`Liked ${n}`}]},
    lengthText:{simpleText:'3:21'},shortBylineText:{runs:[{text:'Artist',navigationEndpoint:{browseEndpoint:{browseId:'UC'+'a'.repeat(22)}}}]},
    thumbnail:{thumbnails:[{url:'https://i.ytimg.com/vi/test/default.jpg'}]}}});
  const library=new Library(async()=>({session:{logged_in:true},actions:{execute:async(_,args)=>{
    calls.push(args);
    if(args.client==='TV' && args.continuation) return {success:false,status_code:500};
    if(args.client==='TV') return response({sectionListRenderer:{contents:[shelf([tile(9)])]}});
    if(args.continuation) return response({onResponseReceivedActions:[{appendContinuationItemsAction:{continuationItems:[web(2),
      {playlistVideoRenderer:{...web(3).playlistVideoRenderer,isPlayable:false}}]}}]});
    assert.equal(args.browseId,'VLLM');
    return response({playlistVideoListRenderer:{contents:[web(1),{continuationItemRenderer:{continuationEndpoint:{continuationCommand:{token:'web-next'}}}}]}});
  }}}),[{id:'signed',cookie:'SID=test'}]);
  const root=await library.execute({action:'topics',section:'music',profile:'signed'});
  const row=await library.execute({action:'topic_open',section:'music',id:root.items[0].id.split(':')[2],profile:'signed'});
  assert.equal(row.items[0].title,'Liked 1'); assert.equal(row.items[0].duration_sec,201);
  assert.equal(row.items[0].subtitle,'Artist'); assert.equal(row.items[0].channel_id,'UC'+'a'.repeat(22));
  const next=await library.execute({action:'continue',cursor:row.cursor,profile:'signed'});
  assert.equal(next.items[0].title,'Liked 2'); assert.equal(calls.at(-1).client,'WEB');
  assert.equal(calls.at(-1).continuation,'web-next'); assert.equal(calls.length,4);
});

test('missing liked Music and expired sign-in preserve public shelves without account requests', async () => {
  let loggedIn=true; const calls=[];
  const library=new Library(async()=>({session:{get logged_in(){return loggedIn;}},actions:{execute:async(_,args)=>{
    calls.push(args);
    if(args.browseId==='FEtopics_music') return response({sectionListRenderer:{contents:[shelf([tile(8)])]}});
    throw Error('unavailable');
  }}}),[{id:'signed',cookie:'SID=test'}]);
  let root=await library.execute({action:'topics',section:'music',profile:'signed'});
  assert.equal(root.items.length,1); assert.equal(calls.length,3);
  loggedIn=false; calls.length=0;
  root=await library.execute({action:'topics',section:'music',profile:'signed'});
  assert.equal(root.items.length,1); assert.equal(calls.length,1);
  assert.equal(calls[0].browseId,'FEtopics_music');
});

test('an empty liked row needs no fallback and other sections never request it', async () => {
  const calls=[];
  const library=new Library(async()=>({session:{logged_in:true},actions:{execute:async(_,args)=>{
    calls.push(args);
    return response(args.continuation ? {horizontalListContinuation:{items:[]}} :
      {sectionListRenderer:{contents:[shelf([tile(1)])]}});
  }}}),[{id:'signed',cookie:'SID=test'}]);
  const music=await library.execute({action:'topics',section:'music',profile:'signed'});
  assert.equal(music.items.length,1); assert.equal(calls.length,2);
  calls.length=0;
  await library.execute({action:'topics',section:'gaming',profile:'signed'});
  await library.execute({action:'topics',section:'news',profile:'signed'});
  assert.deepEqual(calls.map(call=>call.browseId),['FEtopics_gaming','FEtopics_news']);
});

test('Sports/Live/Movies route real section identities and preserve nested storefront/channel shelves', async () => {
  const calls=[];
  const folder=(browseId)=>({tileRenderer:{contentType:'TILE_CONTENT_TYPE_CHANNEL',
    metadata:{tileMetadataRenderer:{title:{simpleText:'Nested shelves'}}},onSelectCommand:{browseEndpoint:{browseId,params:'genre'}}}});
  const movie=tile(5);movie.tileRenderer.onSelectCommand={browseEndpoint:{browseId:'MV00000000005'}};
  const live=tile(6);live.tileRenderer.header.tileHeaderRenderer.thumbnailOverlays[0].thumbnailOverlayTimeStatusRenderer.style='LIVE';
  const client={actions:{execute:async(_,args)=>{
    calls.push(args);
    if(args.browseId==='FEstorefront' || args.browseId?.startsWith('UC'))
      return response({sectionListRenderer:{contents:[shelf([tile(7)])],...cont('nested-next')}});
    if(args.continuation==='nested-next') return response({sectionListContinuation:{contents:[shelf([tile(8)])]}});
    return response({sectionListRenderer:{contents:[shelf([movie,live,folder('FEstorefront'),folder('UC'+'a'.repeat(22)),folder('FEarbitrary')])]}});
  }}};
  const library=new Library(async()=>client);
  for(const section of ['sports','live','movies']) {
    const root=await library.execute({action:'topics',section}); assert.equal(calls.at(-1).browseId,'FEtopics_'+section);
    const row=await library.execute({action:'topic_open',section,id:root.items[0].id.split(':')[2]});
    assert.equal(row.items[0].id,'00000000005');assert(row.items[1].is_live);
    assert.equal(row.items.length,section==='movies'?4:3);
    const nested=await library.execute({action:'topic_open',section,id:row.items.at(-1).id.split(':')[2]});
    assert(nested.items[0].is_folder);
    const next=await library.execute({action:'continue',cursor:nested.cursor});assert(next.items[0].is_folder);
    if(section==='movies') {
      const genres=await library.execute({action:'topic_open',section,id:row.items[2].id.split(':')[2]});
      assert(genres.items[0].is_folder);assert.equal(calls.at(-1).browseId,'FEstorefront');
    }
  }
});

test('an empty storefront reports the upstream explanation instead of an unsupported layout', async () => {
  const library=new Library(async()=>({actions:{execute:async(_,args)=>response(args.browseId==='FEstorefront' ?
    {tvBrowseRenderer:{content:{genericPromoRenderer:{bodyText:{runs:[{text:'Videos you purchase or rent will appear here'}]}}}}} :
    {sectionListRenderer:{contents:[shelf([{tileRenderer:{contentType:'TILE_CONTENT_TYPE_CHANNEL',
      onSelectCommand:{browseEndpoint:{browseId:'FEstorefront',params:'ogUCKAQ%3D'}}}}])]}})
  }}));
  const root=await library.execute({action:'topics',section:'movies'});
  const row=await library.execute({action:'topic_open',section:'movies',id:root.items[0].id.split(':')[2]});
  await assert.rejects(library.execute({action:'topic_open',section:'movies',id:row.items[0].id.split(':')[2]}),
    {code:'catalog_unavailable',status:404,message:'Videos you purchase or rent will appear here'});
});

test('topic raw video adapters preserve structured live upcoming and watch-progress metadata',()=>{
  const topics=new TopicCatalog(Error,normalizeVideo,imageUrl,()=>1000000);
  const raw={videoId:'abcdefghijk',title:{simpleText:'Future video'},upcomingEventData:{startTime:'1001'},
    thumbnailOverlays:[{thumbnailOverlayResumePlaybackRenderer:{percentDurationWatched:45}}],
    badges:[{metadataBadgeRenderer:{style:'BADGE_STYLE_TYPE_LIVE_NOW',label:'Localized label'}}]};
  const row=topics.webVideo(raw);assert.equal(row.watch_percent,45);assert.equal(row.is_upcoming,true);assert.equal(row.is_live,true);
  const past=topics.webVideo({...raw,upcomingEventData:{startTime:'999'},badges:[]});assert.equal(past.is_upcoming,false);assert.equal(past.is_live,false);
  const invalid=topics.webVideo({...raw,upcomingEventData:{startTime:true},thumbnailOverlays:[],badges:[{metadataBadgeRenderer:{label:'LIVE'}}]});
  assert.equal(invalid.watch_percent,-1);assert.equal(invalid.is_upcoming,false);assert.equal(invalid.is_live,false);
});
test('TV topic progress survives normalization and upcoming items remain ineligible for group Next',async()=>{
  const first=tile(1),upcoming=tile(2),last=tile(3);
  upcoming.tileRenderer.upcomingEventData={startTime:'9999999999'};
  last.tileRenderer.header.tileHeaderRenderer.thumbnailOverlays.push({thumbnailOverlayResumePlaybackRenderer:{percentDurationWatched:65}});
  const library=new Library(async()=>({actions:{execute:async()=>response({sectionListRenderer:{contents:[shelf([first,upcoming,last])]}})}}));
  const root=await library.execute({action:'topics',section:'music'});
  const page=await library.execute({action:'topic_open',section:'music',id:root.items[0].id.split(':')[2]});
  assert.equal(page.items[1].is_upcoming,true);assert.equal(page.items[2].watch_percent,65);
  const context=page.items[0].playback_context;
  const next=await library.execute({action:'group_window',group:context.group,id:page.items[0].id,index:0,target:1,direction:1});
  assert.equal(next.video_id,page.items[2].id);assert.equal(next.index,2);
});
