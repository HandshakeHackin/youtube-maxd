// Issue #2: synthetic TV account Home responses; no login or account data.
// SPDX-License-Identifier: GPL-3.0-or-later
import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {Log} from 'youtubei.js';
import {Library,LibraryError,normalizeVideo,imageUrl} from './library.mjs';
import {TvAccountCatalog} from './tv_account_catalog.mjs';
import {SuggestionFilters} from './suggestions.mjs';
import {hides} from './content_filters.mjs';
Log.setLevel(Log.Level.NONE);
const fixture=JSON.parse(readFileSync(new URL('./fixtures/tv-home-shape.json',import.meta.url)));
const source=fixture.contents.tvBrowseRenderer.content.tvSurfaceContentRenderer.content.sectionListRenderer.contents[0].shelfRenderer;
const seed=source.content.horizontalListRenderer.items[0].tileRenderer;
const id=n=>String(n).padStart(11,'0');
function tile(n,extra={}) {
  const value=structuredClone(seed);value.contentId=id(n);value.onSelectCommand={watchEndpoint:{videoId:id(n)}};
  value.metadata.tileMetadataRenderer.title={simpleText:'Synthetic video '+n};
  Object.assign(value,extra);return {tileRenderer:value};
}
function shelf(title,items,token='') {
  return {shelfRenderer:{...(title?{title:{simpleText:title}}:{}),content:{horizontalListRenderer:{
    items,...(token?{continuations:[{nextContinuationData:{continuation:token}}]}:{})}}}};
}
const page=(contents,token='')=>({contents:{sectionListRenderer:{contents,
  ...(token?{continuations:[{nextContinuationData:{continuation:token}}]}:{})}}});
function setup() {
  const references=new SuggestionFilters(LibraryError,normalizeVideo);
  return {references,catalog:new TvAccountCatalog(LibraryError,normalizeVideo,imageUrl,references)};
}
const videos=feed=>feed.catalog_items.filter(row=>!row.is_folder).map(row=>row.id);
const folders=feed=>feed.catalog_items.filter(row=>row.id.startsWith('library:filter:'));
const key=folder=>folder.id.replace('library:filter:','');
const noNetwork={actions:{execute:async()=>{throw new Error('Unexpected network call');}}};

test('TV Home keeps category videos in their shelf instead of flattening them into the primary grid',async()=>{
  const {catalog,references}=setup();
  const feed=catalog.homeFeed('saved',noNetwork,page([
    {adSlotRenderer:{tileRenderer:tile(99).tileRenderer}},
    shelf('Recommended',[tile(1),tile(2)]),
    shelf('Another category',[tile(3),tile(4)]),
    shelf('Shorts',[tile(5,{contentType:'TILE_CONTENT_TYPE_SHORTS'})]),
    shelf('Ad-only shelf',[tile(6,{isAd:true})])]),new Set());
  assert.deepEqual(videos(feed),[id(1),id(2)]);
  assert.deepEqual(folders(feed).map(row=>row.title),['Recommended','Another category','Shorts']);
  const category=folders(feed)[1];
  assert.deepEqual(videos(await references.open('saved',key(category))),[id(3),id(4)]);
  await assert.rejects(references.open('other',key(category)),{code:'cursor_expired'});
});

test('TV Home removes final shelves emptied by Shorts, mixes or ad filtering',()=>{
  const {catalog}=setup();
  const mix=tile(4);mix.tileRenderer.onSelectCommand={browseEndpoint:{browseId:'VLRDabcdefghijk'}};
  const feed=catalog.homeFeed('saved',noNetwork,page([
    shelf('Recommended',[tile(1)]),shelf('Shorts',[tile(2,{contentType:'TILE_CONTENT_TYPE_SHORTS'})]),
    shelf('Ad-only shelf',[tile(3,{isAd:true})]),shelf('Mixes',[mix])]),new Set(['shorts_home','mixes']));
  assert.deepEqual(videos(feed),[id(1)]);
  assert.deepEqual(folders(feed).map(row=>row.title),['Recommended']);
});

test('TV primary watched policy matches web Home defaults and respects an explicit empty selection',async()=>{
  const {catalog,references}=setup();
  const progress=tile(2);progress.tileRenderer.header.tileHeaderRenderer.thumbnailOverlays.push({
    thumbnailOverlayResumePlaybackRenderer:{percentDurationWatched:96}});
  const raw=page([shelf('Recommended',[tile(1,{isWatched:true}),progress,tile(3)]),
    shelf('Retained category',[tile(4,{isWatched:true})])]);
  assert.deepEqual(videos(catalog.homeFeed('saved',noNetwork,raw)),[id(3)]);
  assert.deepEqual(videos(catalog.homeFeed('saved',noNetwork,raw,new Set())),[id(1),id(2),id(3)]);
  const selected=catalog.homeFeed('saved',noNetwork,raw,new Set(['watched_home']));
  assert.deepEqual(videos(selected),[id(3)]);
  // As on web Home, explicitly opened category shelves retain watched cards.
  assert.deepEqual(videos(await references.open('saved',key(folders(selected)[1]))),[id(4)]);
});

test('TV Home retains an unnamed later shelf rather than losing its videos',async()=>{
  const {catalog,references}=setup();
  const feed=catalog.homeFeed('saved',noNetwork,page([shelf('Recommended',[tile(1)]),shelf('',[tile(2)])]),new Set());
  assert.equal(folders(feed)[1].title,'More videos');
  assert.deepEqual(videos(await references.open('saved',key(folders(feed)[1]))),[id(2)]);
});

test('a filtered-empty shelf with a continuation stays reachable without driving Home pagination',async()=>{
  const {catalog,references}=setup();const calls=[];
  const client={actions:{execute:async(path,args)=>{
    calls.push(args.continuation);assert.equal(path,'/browse');assert.equal(args.client,'TV');
    assert.equal(args.continuation,'row-next');
    return {success:true,status_code:200,data:{continuationContents:{gridContinuation:{items:[tile(3)]}}}};
  }}};
  const feed=catalog.homeFeed('saved',client,page([
    shelf('Recommended',[tile(1)]),shelf('More to load',[tile(2,{contentType:'TILE_CONTENT_TYPE_SHORTS'})],'row-next')
  ]),new Set(['shorts_home']));
  assert.equal(feed.has_continuation,false);
  const row=await references.open('saved',key(folders(feed)[1]));
  assert.equal(row.has_continuation,true);
  assert.equal(row.catalog_items.filter(item=>!hides(new Set(['shorts_home']),'home')(item)).length,0);
  assert.deepEqual(videos(await row.getContinuation()),[id(3)]);
  assert.deepEqual(calls,['row-next']);
});

test('Home page continuations keep later categories grouped and separate from shelf continuations',async()=>{
  const {catalog,references}=setup();
  const client={actions:{execute:async(path,args)=>{
    assert.equal(path,'/browse');assert.equal(args.continuation,'home-next');
    return {success:true,status_code:200,data:{continuationContents:{sectionListContinuation:{
      contents:[shelf('Later category',[tile(3)])]}}}};
  }}};
  const feed=catalog.homeFeed('saved',client,page([shelf('Recommended',[tile(1)],'row-next')],'home-next'),new Set());
  assert.equal(feed.has_continuation,true);
  const next=await feed.getContinuation();
  assert.deepEqual(videos(next),[]);
  assert.equal(next.has_continuation,false);
  assert.deepEqual(videos(await references.open('saved',key(folders(next)[0]))),[id(3)]);
});

test('flat TV Home continuations retain the selected watched and Shorts rules',async()=>{
  const {catalog}=setup();const flags=new Set(['watched_home','shorts_home']);
  const client={actions:{execute:async()=>({success:true,status_code:200,data:{
    continuationContents:{gridContinuation:{items:[tile(4,{isWatched:true}),tile(5)]}}}})}};
  const feed=catalog.homeFeed('saved',client,{gridRenderer:{
    items:[tile(1,{isWatched:true}),tile(2,{contentType:'TILE_CONTENT_TYPE_SHORTS'}),tile(3)],
    continuations:[{nextContinuationData:{continuation:'flat-next'}}]}},flags);
  assert.deepEqual(videos(feed),[id(3)]);
  assert.deepEqual(videos(await feed.getContinuation()),[id(5)]);
});

test('an all-ad final Home page returns an empty catalog rather than an upstream error',async()=>{
  const {catalog}=setup();
  const client={actions:{execute:async()=>({success:true,status_code:200,data:page([
    shelf('Empty',[tile(1,{isAd:true})])])})}};
  const feed=await catalog.browse('saved','home',client);
  assert.deepEqual(feed.catalog_items,[]);assert.equal(feed.has_continuation,false);
});

test('Home group limits reject malformed or excessive source rows',()=>{
  const {catalog}=setup();
  for(const rows of [null,Array.from({length:129},()=>shelf('Group',[tile(1)]))])
    assert.throws(()=>catalog.homeFeed('saved',noNetwork,page(rows)),{code:'upstream_limit'});
});

test('TV Home keeps the account-wide item budget across separate shelves',()=>{
  const {catalog}=setup();
  assert.throws(()=>catalog.homeFeed('saved',noNetwork,page([
    shelf('First',Array.from({length:2049},(_,n)=>tile(n))),
    shelf('Second',Array.from({length:2049},(_,n)=>tile(n+3000)))
  ])),{code:'upstream_limit'});
});

test('Library passes the Home hide selection into the OAuth TV adapter',async()=>{
  const credentials={access_token:'synthetic',refresh_token:'synthetic',expiry_date:'2099-01-01T00:00:00.000Z',
    client:{client_id:'synthetic.apps.invalid',client_secret:'synthetic-secret'}};
  const raw=page([shelf('Recommended',[tile(1,{isWatched:true}),tile(2)]),
    shelf('Shorts',[tile(3,{contentType:'TILE_CONTENT_TYPE_SHORTS'})])]);
  const library=new Library(async()=>({session:{logged_in:true},actions:{execute:async()=>({
    success:true,status_code:200,data:structuredClone(raw)})}}),
    [{id:'saved',name:'Saved fixture',cookie:'',oauth:credentials}]);
  const result=await library.execute({action:'home',profile:'saved',hide:['watched_home','shorts_home']});
  assert.deepEqual(result.items.filter(item=>!item.is_folder).map(item=>item.id),[id(2)]);
  assert.equal(result.items.some(item=>item.title==='Shorts'),false);
});
