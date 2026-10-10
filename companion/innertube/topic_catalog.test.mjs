import test from 'node:test';
import assert from 'node:assert/strict';
import { Library } from './library.mjs';
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
