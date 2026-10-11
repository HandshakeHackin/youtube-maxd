import test from 'node:test';
import assert from 'node:assert/strict';
import {Library,LibraryError} from './library.mjs';
import {channelPage} from './channel_window.mjs';
import {YTNodes,Parser} from 'youtubei.js';
const pad=n=>String(n).padStart(11,'0');
const channel='UC'+'c'.repeat(22);
const rows=(first,count)=>Array.from({length:count},(_,i)=>({id:pad(first+i),title:'Video '+(first+i)}));
const feed=(videos,more=null,extra={})=>({videos,playlists:[],has_continuation:!!more,
  ...(more?{async getContinuation(){return more;}}:{}),...extra});
const chip=(selected,result,calls)=>({type:'ChipCloudChip',is_selected:selected,text:'Localized',
  endpoint:{async call(actions,args){calls.push(args);return result;}}});
function upstream(overrides={}) {
  const calls=[],searches=[];
  const popular={parsed:'popular'},oldest={parsed:'oldest'};
  const videos=feed(rows(0,30),feed(rows(30,3)),{filter_nodes:{primary_filters:[chip(true,null,calls),chip(false,popular,calls),chip(false,oldest,calls)]}});
  const page={metadata:{title:'Fixture channel'},has_home:true,has_videos:true,has_shorts:true,has_live_streams:false,
    has_playlists:true,has_search:true,
    async getVideos(){return videos;},
    async getShorts(){return feed([new YTNodes.ShortsLockupView({entityId:'shorts-shelf-item-'+pad(70),accessibilityText:'x',
      thumbnail:{sources:[]},onTap:{innertubeCommand:{reelWatchEndpoint:{videoId:pad(70)}}},overlayMetadata:{primaryText:{content:'Short 70'}}})]);},
    async getPlaylists(){return {videos:[],playlists:[{id:'PLabcdefghijk',title:'A playlist'}],has_continuation:false};},
    async getHome(){
      const inside=[{type:'GridVideo',id:pad(50),title:'Shelf video'},{type:'GridPlaylist',id:'PLabcdefghijk',title:'Shelf playlist'}];
      return {videos:[...inside.slice(0,1),{id:pad(60),title:'Featured'}],has_continuation:false,current_tab:{content:{contents:[
        {type:'ItemSection',contents:[{type:'ChannelVideoPlayer'}]},
        {type:'ItemSection',contents:[{type:'Shelf',title:'Uploads',content:{type:'HorizontalList',items:inside}}]},
        {type:'ItemSection',contents:[{type:'Shelf',title:'',content:{type:'HorizontalList',items:inside}}]}]}}};
    },
    async search(query){searches.push(query);return feed(rows(80,2));},...overrides};
  const client={actions:{name:'actions'},async getChannel(id){assert.equal(id,channel);return page;}};
  return {client,calls,searches,popular,oldest};
}

test('a channel page lists its sections, keeps Videos positions and pages like other lists',async()=>{
  const {client}=upstream();
  const library=new Library(async()=>client);
  let page=await library.execute({action:'channel',channel});
  assert.deepEqual(page.channel,{id:channel,title:'Fixture channel',tabs:['home','videos','shorts','playlists'],search:true,
    tab:'videos',sort:'newest',sorts:['newest','popular','oldest']});
  assert.equal(page.kind,'catalog');assert.equal(page.items.length,24);
  assert.deepEqual(page.items[5].playback_context,{channel,index:5});
  const ids=[...page.items.map(item=>item.id)];
  while(page.has_more) {
    page=await library.execute({action:'continue',cursor:page.cursor});
    assert.ok(!('channel' in page));ids.push(...page.items.map(item=>item.id));
  }
  assert.deepEqual(ids,Array.from({length:33},(_,i)=>pad(i)));
  assert.deepEqual(page.items.at(-1).playback_context,{channel,index:32});
  const shorts=await library.execute({action:'channel',channel,tab:'shorts'});
  assert.deepEqual(shorts.items.map(item=>[item.id,item.title,item.subtitle,item.channel_id,item.is_short]),
    [[pad(70),'Short 70','Fixture channel',channel,true]]);
  assert.equal(shorts.channel.sort,'');assert.deepEqual(shorts.channel.sorts,[]);
  assert.equal((await library.execute({action:'channel',channel,tab:'shorts',hide:['shorts_channel']})).items.length,0);
  const playlists=await library.execute({action:'channel',channel,tab:'playlists'});
  assert.deepEqual(playlists.items.map(item=>[item.id,item.is_folder]),[['library:playlist:PLabcdefghijk',true]]);
  await assert.rejects(library.execute({action:'channel',channel,tab:'live'}),{code:'channel_tab_unavailable',status:404});
  for(const patch of [{channel:'bad'},{tab:'about'},{channel_sort:'rating'},{id:pad(1)},{playlist:'PLabcdefghijk'}])
    await assert.rejects(library.execute({action:'channel',channel,...patch}),error=>['invalid_request','invalid_identity'].includes(error.code));
});

test('channel Home shelves open as folders and loose cards follow them',async()=>{
  const {client}=upstream();
  const library=new Library(async()=>client);
  const home=await library.execute({action:'channel',channel,tab:'home'});
  assert.equal(home.channel.tab,'home');
  assert.deepEqual(home.items.map(item=>[item.title,item.subtitle ?? '',!!item.is_folder]),
    [['Uploads','Channel section',true],['Featured','',false]]);
  const opened=await library.execute({action:'suggestions_filter',id:home.items[0].id.slice(15)});
  assert.deepEqual(opened.items.map(item=>item.id),[pad(50),'library:playlist:PLabcdefghijk']);
});

test('orders are chosen by chip position and an order that is not offered fails',async()=>{
  const {client,calls,popular,oldest}=upstream();
  const wrapped=[];
  const wrap=(actions,page)=>{wrapped.push([actions,page]);return feed(rows(page.parsed==='popular'?200:300,2));};
  const first=await channelPage(client,{action:'channel',channel,channel_sort:'popular'},LibraryError,'',wrap);
  assert.equal(first.indexed,false);assert.equal(first.info.sort,'popular');assert.equal(first.feed.videos[0].id,pad(200));
  const second=await channelPage(client,{action:'channel',channel,channel_sort:'oldest'},LibraryError,'',wrap);
  assert.equal(second.feed.videos[0].id,pad(300));
  assert.deepEqual(wrapped,[[client.actions,popular],[client.actions,oldest]]);assert.deepEqual(calls,[{parse:true},{parse:true}]);
  assert.equal((await channelPage(client,{action:'channel',channel,channel_sort:'newest'},LibraryError)).indexed,true);
  assert.equal(calls.length,2);
  await assert.rejects(channelPage(client,{action:'channel',channel,tab:'shorts',channel_sort:'popular'},LibraryError,'',wrap),
    {code:'channel_sort_unavailable',status:404});
  // Two selected chips, a dropdown or four chips are not a recognizable order bar.
  for(const chips of [[chip(true,null,calls),chip(true,null,calls)],[{type:'ListItemView'},chip(false,null,calls)],
      Array.from({length:4},(_,i)=>chip(i===0,null,calls)),[chip(false,null,calls),chip(true,null,calls)]]) {
    const odd=upstream({async getVideos(){return feed(rows(0,1),null,{filter_nodes:{primary_filters:chips}});}}).client;
    assert.deepEqual((await channelPage(odd,{action:'channel',channel},LibraryError)).info.sorts,[]);
    await assert.rejects(channelPage(odd,{action:'channel',channel,channel_sort:'popular'},LibraryError),{code:'channel_sort_unavailable'});
  }
  const throwing=upstream({async getVideos(){return {videos:[],get filter_nodes(){throw new Error('too many chip bars');}};}}).client;
  assert.deepEqual((await channelPage(throwing,{action:'channel',channel},LibraryError)).info.sorts,[]);
});

test('a parsed order response becomes a pinned continuation feed',async()=>{
  const {client}=upstream();
  const parsed=Parser.parseResponse({onResponseReceivedActions:[{reloadContinuationItemsCommand:{targetId:'browse-feed',slot:'RELOAD_CONTINUATION_SLOT_BODY',
    continuationItems:[{richItemRenderer:{content:{videoRenderer:{videoId:pad(9),title:{runs:[{text:'Popular'}]},thumbnail:{thumbnails:[]}}}}}]}}]});
  const page=(await client.getChannel(channel));
  const original=await page.getVideos();
  original.filter_nodes.primary_filters[1].endpoint.call=async()=>parsed;
  const library=new Library(async()=>client);
  const result=await library.execute({action:'channel',channel,channel_sort:'popular'});
  assert.deepEqual(result.items.map(item=>item.id),[pad(9)]);
  assert.equal(result.channel.sort,'popular');
  assert.deepEqual(Object.keys(result.items[0].playback_context),['group','index']);
});

test('in-channel search validates the query and returns ordinary rows',async()=>{
  const {client,searches}=upstream();
  const library=new Library(async()=>client);
  const found=await library.execute({action:'channel_search',channel,query:'  cats  '});
  assert.deepEqual(searches,['cats']);assert.deepEqual(found.items.map(item=>item.id),[pad(80),pad(81)]);
  assert.deepEqual(found.channel,{id:channel,title:'Fixture channel',tabs:['home','videos','shorts','playlists'],search:true,
    tab:'search',sort:'',sorts:[]});
  assert.deepEqual(Object.keys(found.items[0].playback_context),['group','index']);
  for(const patch of [{query:''},{query:'a\nb'},{query:'x'.repeat(201)}])
    await assert.rejects(library.execute({action:'channel_search',channel,...patch}),{code:'invalid_search'});
  for(const patch of [{tab:'videos'},{channel_sort:'popular'},{channel:'UCshort'}])
    await assert.rejects(library.execute({action:'channel_search',channel,query:'cats',...patch}),
      error=>['invalid_request','invalid_identity'].includes(error.code));
  assert.equal(searches.length,1);
  const closed=new Library(async()=>upstream({has_search:false}).client);
  await assert.rejects(closed.execute({action:'channel_search',channel,query:'cats'}),{code:'channel_search_unavailable',status:404});
});
