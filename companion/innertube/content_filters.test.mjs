import test from 'node:test';
import assert from 'node:assert/strict';
import {Library,LibraryError,normalizeVideo} from './library.mjs';
import {hideFlags,hideSet,hideSection,hides,isShort,isMix,isWatched,shortsCard} from './content_filters.mjs';
import {YTNodes} from 'youtubei.js';
const pad=n=>String(n).padStart(11,'0');
const profiles=[{id:'signed',cookie:'synthetic'}];
const video=(n,extra={})=>new YTNodes.Video({videoId:pad(n),title:{runs:[{text:'Video '+n}]},thumbnail:{thumbnails:[]},...extra});
const shortLink=n=>({navigationEndpoint:{commandMetadata:{webCommandMetadata:{url:'/shorts/'+pad(n)}},reelWatchEndpoint:{videoId:pad(n)}}});
const live={badges:[{metadataBadgeRenderer:{style:'BADGE_STYLE_TYPE_LIVE_NOW',label:'LIVE'}}]};
const upcoming={upcomingEventData:{startTime:'4102444800'}};
const page=nodes=>({videos:nodes,has_continuation:false});

test('the sixteen reference switches validate as one explicit selection',()=>{
  assert.equal(hideFlags.length,16);assert.equal(new Set(hideFlags).size,16);
  assert.equal(hideSet(undefined,LibraryError),null);assert.equal(hideSet(null,LibraryError),null);
  assert.equal(hideSet([],LibraryError).size,0);
  assert.deepEqual(Array.from(hideSet(['mixes','shorts_all'],LibraryError)),['mixes','shorts_all']);
  for(const value of ['mixes',{},['mixes','mixes'],['shorts'],['streams_home'],[7],Array(17).fill('mixes')])
    assert.throws(()=>hideSet(value,LibraryError),{code:'invalid_filter'});
  assert.equal(hides(null,'home'),null);assert.equal(hides(new Set(),'home'),null);
});

test('Shorts are classified from structured parser evidence only',()=>{
  assert.equal(isShort(video(1)),false);
  assert.equal(isShort(video(1,shortLink(1))),true);
  assert.equal(isShort(video(1,{thumbnailOverlays:[{thumbnailOverlayTimeStatusRenderer:{text:{simpleText:'SHORTS'},style:'SHORTS'}}]})),true);
  assert.equal(isShort(video(1,{title:{runs:[{text:'#shorts in a title'}]}})),false);
  assert.equal(isShort({type:'LockupView',content_type:'SHORT'}),true);
  assert.equal(isShort({type:'LockupView',content_type:'VIDEO',renderer_context:{command_context:{on_tap:{name:'reelWatchEndpoint'}}}}),true);
  assert.equal(isShort({type:'LockupView',content_type:'VIDEO'}),false);
  assert.equal(isShort({type:'ReelItem'}),true);assert.equal(isShort({is_short:true}),true);
  for(const node of [null,undefined,'x',{},{endpoint:{metadata:{url:'/watch?v=x'}}}])assert.equal(isShort(node),false);
  assert.equal(normalizeVideo(video(1,shortLink(1))).is_short,true);
  assert.ok(!('is_short' in normalizeVideo(video(1))));
  const tile=new YTNodes.ShortsLockupView({entityId:'shorts-shelf-item-'+pad(4),accessibilityText:'x',
    thumbnail:{sources:[{url:'https://i.ytimg.com/vi/x/frame0.jpg',width:9,height:16}]},
    onTap:{innertubeCommand:{reelWatchEndpoint:{videoId:pad(4)}}},overlayMetadata:{primaryText:{content:'A Short'}}});
  const row=normalizeVideo(shortsCard(tile,{name:'Channel',id:'UC'+'a'.repeat(22)}));
  assert.deepEqual([row.id,row.title,row.subtitle,row.channel_id,row.is_short,row.art_url],
    [pad(4),'A Short','Channel','UC'+'a'.repeat(22),true,'https://i.ytimg.com/vi/x/frame0.jpg']);
  assert.equal(normalizeVideo(tile),null);
});

test('rows are hidden per section exactly as the selected switches say',()=>{
  const short={id:pad(1),is_short:true},stream={id:pad(2),is_live:true},later={id:pad(3),is_upcoming:true};
  const seen={id:pad(4),played:true,watch_percent:100},half={id:pad(5),watch_percent:50},plain={id:pad(6),watch_percent:-1};
  const mix={id:'library:playlist:RDabcdefghijk',is_folder:true},list={id:'library:playlist:PLabcdefghijk',is_folder:true};
  assert.equal(isMix(mix),true);assert.equal(isMix(list),false);assert.equal(isMix({id:'library:playlist:RDx'}),false);
  assert.deepEqual([seen,half,plain,{watch_percent:96},{watch_percent:10}].map(isWatched),[true,false,false,true,true]);
  const rows=[short,stream,later,seen,half,plain,mix,list];
  const kept=(flags,section)=>{const drop=hides(new Set(flags),section);return rows.filter(row=>!drop(row)).map(row=>row.id);};
  const all=rows.map(row=>row.id),without=(...gone)=>all.filter(id=>!gone.includes(id));
  assert.deepEqual(kept(['shorts_all'],'history'),without(short.id));
  assert.deepEqual(kept(['shorts_all'],''),without(short.id));
  assert.deepEqual(kept(['mixes'],''),without(mix.id));
  for(const section of ['subscriptions','search','home','channel','history','trending']) {
    assert.deepEqual(kept(['shorts_'+section],section),without(short.id));
    assert.deepEqual(kept(['shorts_'+section],section==='home'?'search':'home'),all);
  }
  assert.deepEqual(kept(['streams_subscriptions','upcoming_subscriptions','watched_subscriptions'],'subscriptions'),
    without(stream.id,later.id,seen.id));
  assert.deepEqual(kept(['streams_subscriptions','watched_subscriptions'],'channel'),all);
  assert.deepEqual(kept(['upcoming_home'],'home'),without(later.id));
  assert.deepEqual(kept(['upcoming_channel'],'channel'),without(later.id));
  assert.deepEqual(kept(['watched_watch_later'],'watch_later'),without(seen.id));
  assert.deepEqual(kept(['watched_notifications'],'notifications'),without(seen.id));
  // Watched Home cards belong to the Home grid rule, not to the row predicate.
  assert.deepEqual(kept(['watched_home'],'home'),all);
  assert.deepEqual(['home','search','subscriptions','history','watch_later','notifications','channel','channel_search','playlist','library']
    .map(action=>hideSection({action})),['home','search','subscriptions','history','watch_later','notifications','channel','channel','','']);
  assert.deepEqual([hideSection({action:'topics',section:'trending'}),hideSection({action:'topic_open',section:'music'}),
    hideSection({action:'catalog',kind:'channel'}),hideSection({action:'catalog',kind:'playlist'})],['trending','','channel','']);
});

test('list actions apply the selection on every page and keep it with the cursor',async()=>{
  const nodes=Array.from({length:60},(_,i)=>video(i,i%4===0?shortLink(i):i%4===1?live:i%4===2?upcoming:{isWatched:i===3}));
  const chunk=offset=>({videos:nodes.slice(offset,offset+30),has_continuation:offset===0,async getContinuation(){return chunk(30);}});
  const library=new Library(async()=>({session:{logged_in:true},getSubscriptionsFeed:async()=>chunk(0),
    search:async()=>chunk(0),getHistory:async()=>chunk(0)}),profiles);
  const collect=async request=>{
    const ids=[];let result=await library.execute(request);
    for(;;) {ids.push(...result.items.map(item=>Number(item.id)));if(!result.has_more)break;
      result=await library.execute({action:'continue',profile:request.profile,cursor:result.cursor});}
    return ids;
  };
  const base={action:'subscriptions',profile:'signed'};
  assert.equal((await collect(base)).length,60);assert.equal((await collect({...base,hide:[]})).length,60);
  assert.deepEqual(await collect({...base,hide:['shorts_subscriptions','streams_subscriptions','upcoming_subscriptions','watched_subscriptions']}),
    Array.from({length:60},(_,i)=>i).filter(i=>i%4===3 && i!==3));
  assert.deepEqual(await collect({...base,hide:['shorts_search','upcoming_home','shorts_all']}),
    Array.from({length:60},(_,i)=>i).filter(i=>i%4!==0));
  assert.equal((await collect({...base,hide:['shorts_search','streams_subscriptions']})).length,45);
  assert.equal((await collect({action:'search',query:'cats',hide:['shorts_search','streams_subscriptions']})).length,45);
  assert.equal((await collect({action:'history',profile:'signed',hide:['shorts_history']})).length,45);
  assert.equal((await collect({action:'history',profile:'signed',hide:['shorts_search']})).length,60);
  // Hidden rows keep their retained-group positions for navigation.
  const first=await library.execute({action:'search',query:'cats',hide:['shorts_search']});
  assert.deepEqual(first.items.slice(0,3).map(item=>item.playback_context.index),[1,2,3]);
  const routed=await library.playback({action:'catalog',profile:'guest',kind:'search',query:'cats',hide:['shorts_search']});
  assert.ok(routed.items.every(item=>!item.is_short));
  await assert.rejects(library.execute({...base,hide:['shorts']}),{code:'invalid_filter'});
  await assert.rejects(library.playback({action:'catalog',profile:'guest',kind:'search',query:'cats',hide:'mixes'}),{code:'invalid_filter'});
});

test('the Home watched filter is a switch that defaults to the earlier behaviour',async()=>{
  const watched=video(1,{isWatched:true}),fresh=video(2),short=video(3,shortLink(3)),soon=video(4,upcoming);
  const nodes=[watched,fresh,short,soon];
  const home={videos:nodes,contents:{contents:nodes.map(node=>({type:'RichItem',content:node}))},has_continuation:false};
  const library=new Library(async()=>({getHomeFeed:async()=>home}));
  const ids=async hide=>(await library.execute({action:'home',...(hide?{hide}:{})})).items.map(item=>Number(item.id));
  assert.deepEqual(await ids(),[2,3,4]);
  assert.deepEqual(await ids([]),[1,2,3,4]);
  assert.deepEqual(await ids(['watched_home']),[2,3,4]);
  assert.deepEqual(await ids(['shorts_home','upcoming_home']),[1,2]);
  assert.deepEqual(await ids(['watched_home','shorts_all']),[2,4]);
  assert.deepEqual(await ids(['watched_subscriptions','shorts_search']),[1,2,3,4]);
});

test('opened Home rows remember their section and hide mixes on request',async()=>{
  const short=video(7,shortLink(7)),plain=video(8);
  const mix={type:'GridPlaylist',id:'RDabcdefghijk',title:'Mix'},list={type:'GridPlaylist',id:'PLabcdefghijk',title:'List'};
  const shelf={type:'RichSection',content:{type:'RichShelf',title:'Row',contents:[
    {type:'RichItem',content:short},{type:'RichItem',content:plain},{type:'RichItem',content:mix},{type:'RichItem',content:list}]}};
  const home={videos:[short,plain],playlists:[mix,list],contents:{contents:[shelf]},has_continuation:false};
  const library=new Library(async()=>({getHomeFeed:async()=>home}));
  const folder=(await library.execute({action:'home'})).items[0];
  assert.ok(folder.is_folder && folder.id.startsWith('library:filter:'));
  const open=async hide=>(await library.execute({action:'suggestions_filter',id:folder.id.slice(15),...(hide?{hide}:{})})).items.map(item=>item.id);
  const all=[pad(7),pad(8),'library:playlist:RDabcdefghijk','library:playlist:PLabcdefghijk'];
  assert.deepEqual(await open(),all);
  assert.deepEqual(await open(['shorts_home','mixes']),[pad(8),'library:playlist:PLabcdefghijk']);
  assert.deepEqual(await open(['shorts_search']),all);
  assert.deepEqual(await open(),all);
});
