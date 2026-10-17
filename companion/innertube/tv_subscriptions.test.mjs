// TV Subscriptions page of an OAuth (TV) sign-in. The fixture's renderer structure is
// minimized from MediaServiceCore 7f36935513f55ba3f76f40427b339146a5b52f13
// youtubeapi/src/test/resources/browse/tv/2025.10.31_subscriptions.json and
// 2025.08.22_subscriptions_continuation.json; every title, identity and address is synthetic.
// SPDX-License-Identifier: GPL-3.0-or-later
import test from 'node:test';
import assert from 'node:assert/strict';
import {Log} from 'youtubei.js';
import {Library} from './library.mjs';
import shapes from './fixtures/tv-subscriptions-shape.json' with {type: 'json'};
Log.setLevel(Log.Level.NONE);

const credentials={access_token:'synthetic-access',refresh_token:'synthetic-refresh',expiry_date:'2099-01-01T00:00:00.000Z',
  client:{client_id:'synthetic.apps.invalid',client_secret:'synthetic-secret'}};
const profiles=[{id:'saved',name:'saved',cookie:'',oauth:credentials}];

// A TV sign-in as YouTube answers it: the TV Subscriptions page on /browse, and a web
// channel list that names no channel (what the old path read for this account).
function account(calls) {
  return async()=>({session:{logged_in:true},
    actions:{execute:async(path,args)=>{
      calls.push({path,...args});
      if(path==='/guide')return {success:true,status_code:200,data:{}};
      assert.equal(path,'/browse');
      if(args.continuation==='synthetic-subscriptions-next')return {success:true,status_code:200,data:structuredClone(shapes.continuation)};
      assert.equal(args.browseId,'FEsubscriptions');assert.equal(args.client,'TV');
      return {success:true,status_code:200,data:structuredClone(shapes.page)};
    }},
    getChannelsFeed:async()=>{calls.push({path:'web-channels'});return {channels:[],has_continuation:false};}});
}

test('a TV Subscriptions tab names its channel in the browse parameters',async()=>{
  const {tabChannel}=await import('./tv_account_catalog.mjs');
  const tabs=shapes.page.contents.tvBrowseRenderer.content.tvSecondaryNavRenderer.sections[0].tvSecondaryNavSectionRenderer.tabs;
  assert.deepEqual(tabs.map(tab=>tabChannel(tab.tabRenderer)),['','UCfixtureChannel0000000A','UCfixtureChannel0000000B']);
  assert.equal(tabChannel({endpoint:{browseEndpoint:{browseId:'UCfixtureChannel0000000C'}}}),'UCfixtureChannel0000000C');
  for(const params of ['%%%','not base64!','igMaChhVQ1'])assert.equal(tabChannel({endpoint:{browseEndpoint:{browseId:'FEsubscriptions',params}}}),'');
});

test('OAuth Subscribed channels come from the TV Subscriptions page and say why in counts',async()=>{
  const calls=[],logged=[],error=console.error;
  console.error=line=>logged.push(String(line));
  try {
    const library=new Library(account(calls),profiles);
    const page=await library.execute({action:'channels',profile:'saved',new_content:true});
    assert.deepEqual(page.items.map(item=>[item.id,item.channel_id,item.title,item.is_folder]),[
      ['channel:UCfixtureChannel0000000A','UCfixtureChannel0000000A','Fixture channel A',true],
      ['channel:UCfixtureChannel0000000B','UCfixtureChannel0000000B','Fixture channel B',true]]);
    assert.equal(page.items[0].art_url,'https://yt3.googleusercontent.com/synthetic-A=s176-c-k-c0x00ffffff-no-rj-mo');
    assert.equal(page.has_more,false);
    assert.ok(!calls.some(call=>call.path==='web-channels'),'the web list does not answer a TV sign-in');
    assert.deepEqual(logged,['YouTube browse shape: action=channels tabs=3 channels=2 mode=tv']);
  } finally {console.error=error;}
});

test('OAuth Subscribed channels fall back to the web list when the TV page has no channel tabs',async()=>{
  const calls=[],logged=[],error=console.error;
  console.error=line=>logged.push(String(line));
  try {
    const library=new Library(async()=>({session:{logged_in:true},
      actions:{execute:async()=>({success:true,status_code:200,data:{contents:{}}})},
      getChannelsFeed:async()=>{calls.push('web');return {channels:[],has_continuation:false};}}),profiles);
    const page=await library.execute({action:'channels',profile:'saved'});
    assert.deepEqual([page.items,calls],[[],['web']]);
    assert.deepEqual(logged,['YouTube browse shape: action=channels tabs=0 channels=0 mode=legacy']);
  } finally {console.error=error;}
});

test('Subscriptions cards keep 16:9 or 4:3 pictures and give portrait ones the 16:9 mqdefault',async()=>{
  const {videoCardImage}=await import('./topic_catalog.mjs');
  const sd={url:'https://i.ytimg.com/vi/fixtureSU01/sddefault.jpg',width:640,height:480};
  const wide={url:'https://i.ytimg.com/vi/fixtureSU01/hq720.jpg',width:640,height:360};
  const oar=[{url:'https://i.ytimg.com/vi/fixtureSU05/oardefault.jpg?sqp=x',width:203,height:360},
    {url:'https://i.ytimg.com/vi/fixtureSU05/oardefault.jpg?sqp=y',width:405,height:720}];
  assert.equal(videoCardImage([sd],'fixtureSU01'),sd.url);
  assert.equal(videoCardImage([sd,wide],'fixtureSU01'),wide.url);
  assert.equal(videoCardImage(oar,'fixtureSU05'),'https://i.ytimg.com/vi/fixtureSU05/mqdefault.jpg');
  assert.equal(videoCardImage([{url:'https://i.ytimg.com/vi/fixtureSU05/frame0.jpg?usqp=CCk'}],'fixtureSU05'),
    'https://i.ytimg.com/vi/fixtureSU05/mqdefault.jpg');
  assert.equal(videoCardImage([],'fixtureSU05'),undefined);
  const library=new Library(account([]),profiles);
  const page=await library.execute({action:'subscriptions',profile:'saved'});
  const art=Object.fromEntries(page.items.filter(item=>!item.is_folder).map(item=>[item.id,item.art_url]));
  assert.equal(art.fixtureSU05,'https://i.ytimg.com/vi/fixtureSU05/mqdefault.jpg');
  assert.equal(art.fixtureSU03,'https://i.ytimg.com/vi/fixtureSU03/sddefault.jpg');
});

test('OAuth Subscriptions pages list every grid and shelf card once, no shelf folders, and end with the feed',async()=>{
  const calls=[];
  const library=new Library(account(calls),profiles);
  const pages=[];
  let page=await library.execute({action:'subscriptions',profile:'saved'});
  for(;;) {
    pages.push(page.items.map(item=>item.is_folder?'folder:'+item.title:item.id));
    if(!page.has_more)break;
    page=await library.execute({action:'continue',cursor:page.cursor,profile:'saved'});
  }
  assert.deepEqual(pages,[['fixtureSU01','fixtureSU02','fixtureSU03','fixtureSU04','fixtureSU05'],
    ['fixtureSU06','fixtureSU07','fixtureSU08']]);
  assert.equal(calls.filter(call=>call.continuation).length,1);
});

// A signed-in TV Home whose tiles carry command chains nested deeper than 40
// levels (the console's Home failed on such an answer with a plain Error).
test('OAuth Home reads its cards when command chains nest very deeply',async()=>{
  const {find}=await import('./topic_catalog.mjs');
  let chain={signalAction:{signal:'END'}};
  for(let i=0;i<120;i++)chain={commandExecutorCommand:{commands:[chain]}};
  const tile=n=>({tileRenderer:{contentType:'TILE_CONTENT_TYPE_VIDEO',contentId:`fixtureHM${String(n).padStart(2,'0')}`,
    onSelectCommand:{watchEndpoint:{videoId:`fixtureHM${String(n).padStart(2,'0')}`}},onLongPressCommand:structuredClone(chain),
    metadata:{tileMetadataRenderer:{title:{simpleText:`Home ${n}`},lines:[]}},header:{tileHeaderRenderer:{thumbnail:{thumbnails:[
      {url:`https://i.ytimg.com/vi/fixtureHM${String(n).padStart(2,'0')}/hqdefault.jpg`,width:480,height:270}]}}}}});
  const home={contents:{tvBrowseRenderer:{content:{tvSurfaceContentRenderer:{content:{sectionListRenderer:{contents:[
    {shelfRenderer:{title:{simpleText:'Recommended'},content:{horizontalListRenderer:{items:[1,2,3].map(tile)}}}},
    {shelfRenderer:{title:{simpleText:'Watch again'},content:{horizontalListRenderer:{items:[4,5].map(tile)}}}}]}}}}}}};
  assert.equal(find(home,'tileRenderer',true).length,5);
  const library=new Library(async()=>({session:{logged_in:true},actions:{execute:async(path,args)=>{
    assert.equal(args.browseId,'default');return {success:true,status_code:200,data:structuredClone(home)};}}}),profiles);
  const logged=[],error=console.error;console.error=line=>logged.push(String(line));
  try {
    const page=await library.execute({action:'home',profile:'saved'});
    assert.deepEqual(page.items.filter(item=>!item.is_folder).map(item=>item.id),['fixtureHM01','fixtureHM02','fixtureHM03','fixtureHM04','fixtureHM05']);
  } finally {console.error=error;}
  // The work stays bounded: a structure of more nodes than the budget is refused.
  assert.throws(()=>find({rows:Array.from({length:400001},()=>({}))},'tileRenderer',true),/exceeds limits/);
});

// A TV sign-in searches through the Guest session: its own TV session answers a
// search in a layout the reader does not know, so every search came back empty.
test('a TV sign-in searches through the Guest session',async()=>{
  const asked=[];
  const library=new Library(async profile=>({session:{logged_in:!!profile.oauth},
    search:async query=>{asked.push([profile.id,query]);return {results:[],has_continuation:false};}}),profiles);
  const page=await library.execute({action:'search',profile:'saved',query:'minecr'});
  assert.deepEqual([asked,page.items],[[['guest','minecr']],[]]);
  await library.execute({action:'search',query:'zoo'});
  assert.deepEqual(asked.at(-1),['guest','zoo']);
});
