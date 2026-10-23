// Issue #2 host reproduction through the real OAuth TV account route.
// Synthetic identities/credentials only; no network requests or account data.
// Run after npm ci --ignore-scripts in companion/innertube.
// SPDX-License-Identifier: GPL-3.0-or-later
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {Library} from '../companion/innertube/library.mjs';

const fixture=JSON.parse(readFileSync(new URL('../companion/innertube/fixtures/tv-home-shape.json',import.meta.url)));
const shelves=fixture.contents.tvBrowseRenderer.content.tvSurfaceContentRenderer.content.sectionListRenderer.contents;
const original=shelves[0].shelfRenderer;
const seed=original.content.horizontalListRenderer.items[0].tileRenderer;
const credentials={access_token:'synthetic-access',refresh_token:'synthetic-refresh',expiry_date:'2099-01-01T00:00:00.000Z',
  client:{client_id:'synthetic.apps.invalid',client_secret:'synthetic-secret'}};
const ids=offset=>Array.from({length:6},(_,i)=>String(offset+i).padStart(11,'0'));
function shelf(title,offset,extra={}) {
  const value=structuredClone(original);
  delete value.headerRenderer;
  value.title={simpleText:title};
  value.content.horizontalListRenderer={items:ids(offset).map((id,i)=>{
    const tile=structuredClone(seed);
    tile.contentId=id;tile.onSelectCommand={watchEndpoint:{videoId:id}};
    tile.metadata.tileMetadataRenderer.title={simpleText:title+' '+i};
    Object.assign(tile,extra);
    return {tileRenderer:tile};
  })};
  return {shelfRenderer:value};
}
shelves.splice(0,shelves.length,
  shelf('Primary recommendations',100,{isWatched:true}),
  shelf('Shorts',200,{contentType:'TILE_CONTENT_TYPE_SHORTS'}),
  shelf('Explicitly marked ads',300,{isAd:true}),
  shelf('Other recommendations',400));
async function collect(hide) {
  let calls=0;
  const library=new Library(async()=>({session:{logged_in:true},actions:{execute:async(path,args)=>{
    assert.equal(path,'/browse');assert.equal(args.browseId,'default');assert.equal(args.client,'TV');++calls;
    return {success:true,status_code:200,data:structuredClone(fixture)};
  }}}),[{id:'saved',name:'Synthetic test profile',cookie:'',oauth:credentials}]);
  let page=await library.execute({action:'home',profile:'saved',...(hide===undefined?{}:{hide})});
  const rows=[];
  for(let pages=0;;++pages) {
    assert.ok(pages<5,'unexpected pagination loop');rows.push(...page.items);
    if(!page.has_more)break;
    page=await library.execute({action:'continue',cursor:page.cursor,profile:'saved'});
  }
  assert.equal(calls,1);
  const folders=rows.filter(row=>row.is_folder),opened=[];
  for(const folder of folders) {
    assert.ok(folder.id.startsWith('library:filter:'));
    const result=await library.execute({action:'suggestions_filter',profile:'saved',
      id:folder.id.replace('library:filter:',''),...(hide===undefined?{}:{hide})});
    opened.push({title:folder.title,videos:result.items.filter(row=>!row.is_folder).length});
  }
  return {rows,videos:rows.filter(row=>!row.is_folder),folders,opened};
}
const normal=await collect();
assert.deepEqual(normal.videos.map(row=>row.id),[...ids(100),...ids(200),...ids(400)]);
assert.ok(normal.videos.every(row=>!ids(300).includes(row.id)),'explicit ad marker must be respected');
const hidden=await collect(['shorts_home']);
assert.deepEqual(hidden.videos.map(row=>row.id),[...ids(100),...ids(400)]);
const watched=await collect(['watched_home']);
const summary={
  scope:'Offline synthetic OAuth TV Home response, not a live-account reproduction',
  upstream_shelves:shelves.map(row=>row.shelfRenderer.title.simpleText),
  default_video_titles:normal.videos.map(row=>row.title),
  default_shorts:normal.videos.filter(row=>row.is_short).length,
  with_hide_shorts_home:hidden.videos.filter(row=>row.is_short).length,
  explicit_ad_tiles_returned:normal.videos.filter(row=>ids(300).includes(row.id)).length,
  default_watched:normal.videos.filter(row=>row.played).length,
  with_hide_watched_home:watched.videos.filter(row=>row.played).length,
  navigation_tiles:normal.folders.map(row=>row.title),
  opened_sections_default:normal.opened,
  opened_sections_with_hide_shorts_home:hidden.opened,
  source_video_order_preserved:true,
  live_personalization_compared:false
};
console.log(JSON.stringify(summary,null,2));
