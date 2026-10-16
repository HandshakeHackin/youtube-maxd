// Renderer structure minimized from MediaServiceCore 7f36935513f55ba3f76f40427b339146a5b52f13:
// https://github.com/yuliskov/MediaServiceCore/blob/7f36935513f55ba3f76f40427b339146a5b52f13/youtubeapi/src/test/resources/browse/tv/2025.07.30_home.json
// All titles, identities and thumbnail URLs are synthetic; no account responses are retained.
// SPDX-License-Identifier: GPL-3.0-or-later
import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {YT,Log} from 'youtubei.js';
import {Library,LibraryError,normalizeVideo} from './library.mjs';
import {TopicCatalog} from './topic_catalog.mjs';
const raw=JSON.parse(readFileSync(new URL('./fixtures/tv-home-shape.json',import.meta.url)));
Log.setLevel(Log.Level.NONE);

test('TV tile feed is absent from pinned web getters but existing TV mapper retains it',async()=>{
  const parsed=new YT.HomeFeed({}, {data:raw});
  assert.equal(parsed.memo.get('Tile').length,1);
  assert.equal(parsed.videos.length,0); // The old account path silently lost this real renderer shape.
  const topics=new TopicCatalog(LibraryError,normalizeVideo,url=>url ?? '');
  const first=topics.feed('saved','home',{},raw,true);
  assert.equal(first.catalog_items.length,1);
  assert.equal(first.catalog_items[0].title,'For you');
  const key=first.catalog_items[0].id.split(':')[2];
  const row=await topics.open('saved','home',key);
  assert.equal(row.catalog_items.length,1);
  assert.equal(row.catalog_items[0].id,'fixtureTV01');
  assert.equal(row.catalog_items[0].title,'Saved account recommendation');
  assert.equal(row.catalog_items[0].duration_sec,123);
});

const credentials={access_token:'synthetic-access',refresh_token:'synthetic-refresh',expiry_date:'2099-01-01T00:00:00.000Z',
  client:{client_id:'synthetic.apps.invalid',client_secret:'synthetic-secret'}};
const profiles=['saved','other'].map(id=>({id,name:id,cookie:'',oauth:credentials}));
const videoId=n=>String(n).padStart(11,'0');
const sourceTile=raw.contents.tvBrowseRenderer.content.tvSurfaceContentRenderer.content.sectionListRenderer.contents[0]
  .shelfRenderer.content.horizontalListRenderer.items[0].tileRenderer;
function tile(n) {
  const value=structuredClone(sourceTile);value.contentId=videoId(n);value.onSelectCommand.watchEndpoint.videoId=videoId(n);
  value.metadata.tileMetadataRenderer.title.simpleText='Video '+n;return {tileRenderer:value};
}
function tvResponse(shelves=true) {
  const list={items:Array.from({length:26},(_,n)=>tile(n)),continuations:[{nextContinuationData:{continuation:'synthetic-next'}}]};
  if(!shelves)return {contents:{tvBrowseRenderer:{content:{tvSurfaceContentRenderer:{content:{gridRenderer:list}}}}}};
  const value=structuredClone(raw);
  value.contents.tvBrowseRenderer.content.tvSurfaceContentRenderer.content.sectionListRenderer.contents[0]
    .shelfRenderer.content.horizontalListRenderer=list;
  return value;
}

// Additional public TV wrappers from the same MediaServiceCore revision:
// browse/tv/history_25.01.2023.json and browse/tv/2023.12.2_browse_playlist.json.
function accountResponse(action) {
  if(action==='home')return tvResponse();
  const list=tvResponse(false).contents.tvBrowseRenderer.content.tvSurfaceContentRenderer.content.gridRenderer;
  if(action==='playlist')return {contents:{tvBrowseRenderer:{content:{tvSurfaceContentRenderer:{content:{
    twoColumnRenderer:{rightColumn:{playlistVideoListRenderer:{contents:list.items,continuations:list.continuations}}}
  }}}}}};
  return {continuationContents:{sectionListContinuation:{contents:[{shelfRenderer:{title:{simpleText:'History'},
    content:{horizontalListRenderer:{items:list.items}}}}],continuations:list.continuations}}};
}

for(const request of [{action:'home'},{action:'history'},{action:'playlist',id:'LL'}])
  test('OAuth '+request.action+' maps TV cards and owns all continuation state',async()=>{
    const calls=[];
    const library=new Library(async()=>{
      const actions={execute:async(path,args)=>{
        calls.push({path,...args});assert.equal(path,'/browse');
        if(args.continuation){assert.equal(args.continuation,'synthetic-next');
          return {success:true,status_code:200,data:{continuationContents:{gridContinuation:{items:[tile(26)]}}}};}
        assert.equal(args.browseId,{home:'default',history:'FEhistory',playlist:'VLLL'}[request.action]);
        return {success:true,status_code:200,data:accountResponse(request.action)};
      }};
      return {session:{logged_in:true,context:{client:{clientName:'TVHTML5'}}},actions,
        // These are the actual pinned getters' result, not pre-normalized fixture rows.
        getHomeFeed:async()=>new YT.HomeFeed(actions,{data:tvResponse()}),
        getHistory:async()=>new YT.History(actions,{data:tvResponse(false)}),
        getPlaylist:async()=>new YT.Playlist(actions,{data:tvResponse(false)})};
    },profiles);
    let page=await library.execute({...request,profile:'saved'});
    assert.ok(page.items.length,'TV browse must not silently return an empty page');
    if(page.items[0].is_folder){
      const [prefix,section,id]=page.items[0].id.split(':');assert.equal(prefix,'section');
      await assert.rejects(library.execute({action:'topic_open',section,id,profile:'other'}),{code:'cursor_expired'});
      page=await library.execute({action:'topic_open',section,id,profile:'saved'});
    }
    const ids=[],folders=[];
    async function collect(value) {
      for(;;){
        ids.push(...value.items.filter(item=>!item.is_folder).map(item=>item.id));
        folders.push(...value.items.filter(item=>item.is_folder));
        if(!value.has_more)break;
        assert.ok(value.cursor);
        await assert.rejects(library.execute({action:'continue',cursor:value.cursor,profile:'other'}),{code:'cursor_expired'});
        value=await library.execute({action:'continue',cursor:value.cursor,profile:'saved'});
      }
    }
    await collect(page);
    if(request.action==='home') {
      assert.deepEqual(ids,Array.from({length:26},(_,n)=>videoId(n)));
      assert.equal(folders.length,1);
      const key=folders[0].id.replace('library:filter:','');
      assert.notEqual(key,folders[0].id);
      await assert.rejects(library.execute({action:'suggestions_filter',id:key,profile:'other'}),{code:'cursor_expired'});
      ids.length=0;
      await collect(await library.execute({action:'suggestions_filter',id:key,profile:'saved'}));
    }
    assert.deepEqual(ids,Array.from({length:27},(_,n)=>videoId(n)));
    assert.equal(calls.filter(call=>call.continuation).length,1);
    assert.ok(calls.every(call=>call.client==='TV'));
  });

for(const action of ['home','history','playlist'])test('OAuth legacy '+action+' retains the web parser fallback',async()=>{
  const card={videoId:'fixtureWEB1',title:{simpleText:'Legacy card'},lengthText:{simpleText:'1:02'},
    thumbnail:{thumbnails:[{url:'https://i.ytimg.com/vi/fixtureWEB1/hqdefault.jpg',width:480,height:360}]},
    navigationEndpoint:{watchEndpoint:{videoId:'fixtureWEB1',playlistId:'LL',index:0}}};
  const content=action==='home'?{richGridRenderer:{contents:[{richItemRenderer:{content:{videoRenderer:card}}}]}}:
    {sectionListRenderer:{contents:[{itemSectionRenderer:{contents:action==='playlist'?
      [{playlistVideoListRenderer:{playlistId:'LL',contents:[{playlistVideoRenderer:{...card,title:{...card.title,accessibility:{accessibilityData:{label:'Legacy card'}}},index:{simpleText:'1'},shortBylineText:{runs:[{text:'Fixture channel'}]},lengthSeconds:'62',isPlayable:true}}]}}]:
      [{videoRenderer:card}]}}]}};
  const legacy={contents:{twoColumnBrowseResultsRenderer:{tabs:[{tabRenderer:{selected:true,content}}]}}};
  const library=new Library(async()=>({session:{logged_in:true},actions:{execute:async()=>
    ({success:true,status_code:200,data:legacy})}}),profiles);
  const page=await library.execute({action,profile:'saved',...(action==='playlist'?{id:'LL'}:{})});
  assert.equal(page.items.length,1);assert.equal(page.items[0].id,'fixtureWEB1');
  assert.equal(page.items[0].title,'Legacy card');assert.equal(page.items[0].duration_sec,62);
  assert.equal(page.has_more,false);
});
