// Shapes distilled from the Guest live check of 2026-10-11; identities are synthetic.
// SPDX-License-Identifier: GPL-3.0-or-later
import test from 'node:test';
import assert from 'node:assert/strict';
import {Library} from './library.mjs';
import {YTNodes} from 'youtubei.js';
const pad=n=>String(n).padStart(11,'0');
const channel='UC'+'c'.repeat(22);
const response=data=>({success:true,status_code:200,data});
// The live Shorts tile: its image sits in a view model the pinned parser skips.
const shortTile=n=>new YTNodes.ShortsLockupView({entityId:'shorts-shelf-item-'+pad(n),accessibilityText:'x',
  thumbnailViewModel:{thumbnailViewModel:{image:{sources:[{url:`https://i.ytimg.com/vi/${pad(n)}/sardefault.jpg`,width:405,height:720}]}}},
  onTap:{innertubeCommand:{reelWatchEndpoint:{videoId:pad(n),thumbnail:{thumbnails:[{url:`https://i.ytimg.com/vi/${pad(n)}/frame0.jpg`,width:1080,height:1920}]}}}},
  overlayMetadata:{primaryText:{content:'Short '+n},secondaryText:{content:'42K views'}}});

test('search returns the Shorts of grid shelves, alone or between videos',async()=>{
  const shelf={type:'GridShelfView',contents:[shortTile(1),shortTile(2)]};
  const results={mixed:[{type:'Video',id:pad(9),title:'Video 9'},shelf],shorts:[shelf,{type:'GridShelfView',contents:[shortTile(3)]}]};
  const library=new Library(async()=>({search:async(_query,filters)=>({results:results[filters.type==='shorts'?'shorts':'mixed'],has_continuation:false})}));
  const mixed=await library.execute({action:'search',query:'cats'});
  assert.deepEqual(mixed.items.map(item=>[item.kind,item.id,item.title,!!item.is_short,item.art_url]),[
    ['video',pad(9),'Video 9',false,''],
    ['video',pad(1),'Short 1',true,`https://i.ytimg.com/vi/${pad(1)}/frame0.jpg`],
    ['video',pad(2),'Short 2',true,`https://i.ytimg.com/vi/${pad(2)}/frame0.jpg`]]);
  const only=await library.execute({action:'search',query:'cats',type:'shorts'});
  assert.deepEqual(only.items.map(item=>item.id),[pad(1),pad(2),pad(3)]);
  assert.deepEqual((await library.execute({action:'search',query:'cats',hide:['shorts_search']})).items.map(item=>item.id),[pad(9)]);
});

test('replies load through the reply renderer continuation the pinned thread parser does not read',async()=>{
  const actions={name:'actions'},calls=[];
  const reply=n=>({type:'CommentView',comment_id:'reply-'+n,content:'Reply '+n,author:{name:'@viewer'}});
  const append=contents=>({on_response_received_endpoints:[{type:'AppendContinuationItemsAction',contents}]});
  const call=(name,result)=>({async call(given,args){assert.equal(given,actions);assert.deepEqual(args,{parse:true});calls.push(name);return result;}});
  const more={type:'ContinuationItem',button:{endpoint:call('more',append([reply(2)]))}};
  const thread=(first)=>({comment:{comment_id:'parent',content:'Parent'},has_replies:true,is_prepopulated:false,
    comment_replies_data:{contents:[{type:'ContinuationItem',endpoint:call('first',first)}],sub_threads:[]},
    getReplies:async()=>{throw new Error('Replies continuation item not found');}});
  let served=thread(append([reply(0),reply(1),more]));
  const library=new Library(async()=>({actions,getComments:async()=>({contents:[served],has_continuation:false})}));
  const open=async()=>(await library.execute({action:'comments',id:pad(0)})).items[0].reply_cursor;
  const cursor=await open();
  let page=await library.execute({action:'replies',cursor});
  assert.deepEqual(page.items.map(item=>[item.id,item.content,item.author,item.reply_cursor]),
    [['reply-0','Reply 0','@viewer',''],['reply-1','Reply 1','@viewer','']]);
  assert.equal(page.has_more,true);
  assert.deepEqual(await library.execute({action:'replies',cursor}),page);
  page=await library.execute({action:'continue',cursor:page.cursor});
  assert.deepEqual(page.items.map(item=>item.id),['reply-2']);assert.equal(page.has_more,false);
  assert.deepEqual(calls,['first','more']);
  for(const broken of [{},append([reply(0),more,more]),{on_response_received_endpoints:[]},
    append([{type:'ContinuationItem',button:{}}])]) {
    served=thread(broken);
    await assert.rejects(library.execute({action:'replies',cursor:await open()}),{code:'upstream_format',status:502});
  }
});

test('movie posters and storefront categories take their title from the tile header overlay',async()=>{
  const overlay=(title,line)=>[{tileMetadataRenderer:{title:{simpleText:title},
    ...(line?{lines:[{lineRenderer:{items:[{lineItemRenderer:{text:{simpleText:line}}}]}}]}:{})}}];
  const poster={tileRenderer:{style:'TILE_STYLE_YTLR_DEFAULT',contentType:'TILE_CONTENT_TYPE_VIDEO',contentId:pad(5),
    onSelectCommand:{browseEndpoint:{browseId:'MV'+pad(5)}},
    header:{tileHeaderRenderer:{thumbnail:{thumbnails:[{url:'https://i.ytimg.com/vi/poster/movieposter.jpg'}]},
      thumbnailOverlays:overlay('A feature film','Horror')}}}};
  const category={tileRenderer:{contentType:'TILE_CONTENT_TYPE_CHANNEL',onSelectCommand:{browseEndpoint:{browseId:'FEstorefront',params:'ogUCKAY%3D'}},
    header:{tileHeaderRenderer:{thumbnail:{thumbnails:[{url:'https://lh3.googleusercontent.com/category'}]},
      thumbnailOverlays:[{tileMetadataRenderer:{title:{runs:[{text:'Free with ads'}]}}}]}}}};
  const library=new Library(async()=>({actions:{execute:async()=>response({sectionListRenderer:{contents:[
    {shelfRenderer:{title:{simpleText:'Top selling'},content:{horizontalListRenderer:{items:[poster,category]}}}}]}})}}));
  const root=await library.execute({action:'topics',section:'movies'});
  const row=await library.execute({action:'topic_open',section:'movies',id:root.items[0].id.split(':')[2]});
  assert.deepEqual(row.items.map(item=>[item.id.startsWith('section:movies:')?'folder':item.id,item.title,item.subtitle,item.is_folder]),
    [[pad(5),'A feature film','Horror',false],['folder','Free with ads','YouTube',true]]);
});

test('the retired Trending and Shorts browse identities report an unavailable section',async()=>{
  let status=400;
  const library=new Library(async()=>({actions:{execute:async()=>{
    throw new Error(`Request to https://www.youtube.com/youtubei/v1/browse?prettyPrint=false&alt=json failed with status code ${status}`);}}}));
  for(const section of ['trending','shorts'])
    await assert.rejects(library.execute({action:'topics',section}),{code:'catalog_unavailable',status:404});
  await assert.rejects(library.execute({action:'topics',section:'music'}),error=>error.code!=='catalog_unavailable');
  status=500;
  await assert.rejects(library.execute({action:'topics',section:'trending'}),error=>error.code!=='catalog_unavailable');
});

test('a channel\'s own Videos and Live cards name the channel instead of their view count',async()=>{
  const card=(n,parts)=>({type:'LockupView',content_id:pad(n),content_type:'VIDEO',
    metadata:{title:'Video '+n,metadata:{metadata_rows:[{metadata_parts:parts}]}}});
  const elsewhere='UC'+'d'.repeat(22);
  const videos={videos:[card(1,[{text:'100K views'},{text:'2 days ago'}]),
    card(2,[{text:{endpoint:{payload:{browseId:elsewhere}},toString:()=>'Guest channel'}}])],playlists:[],has_continuation:false};
  const page={metadata:{title:'Fixture channel'},has_videos:true,has_live_streams:true,has_search:false,
    async getVideos(){return videos;},async getLiveStreams(){return videos;}};
  const library=new Library(async()=>({actions:{},async getChannel(){return page;}}));
  const expected=[[pad(1),'Fixture channel',channel],[pad(2),'Guest channel',elsewhere]];
  for(const tab of ['videos','live'])
    assert.deepEqual((await library.execute({action:'channel',channel,tab})).items.map(item=>[item.id,item.subtitle,item.channel_id]),expected);
  const catalog=await library.playback({action:'catalog',profile:'guest',kind:'channel',id:channel});
  assert.deepEqual(catalog.items.map(item=>[item.id,item.subtitle,item.channel_id]),expected);
  assert.deepEqual(catalog.items[0].playback_context,{channel,index:0});
  page.metadata={};
  assert.deepEqual((await library.execute({action:'channel',channel})).items.map(item=>[item.subtitle,item.channel_id])[0],['100K views','']);
});
