import test from 'node:test';
import assert from 'node:assert/strict';
import {Library} from './library.mjs';
import {YT,YTNodes} from 'youtubei.js';
const pad=n=>String(n).padStart(11,'0');
const id='abcdefghijk';
const profiles=[{id:'signed',cookie:'synthetic'},{id:'other',cookie:'synthetic'}];
const tile=(n,short=false)=>({tileRenderer:{contentType:short?'TILE_CONTENT_TYPE_SHORTS':'TILE_CONTENT_TYPE_VIDEO',
  onSelectCommand:short?{reelWatchEndpoint:{videoId:pad(n)}}:{watchEndpoint:{videoId:pad(n)}},
  header:{tileHeaderRenderer:{thumbnail:{thumbnails:[{url:'https://i.ytimg.com/vi/test/default.jpg'}]}}},
  metadata:{tileMetadataRenderer:{title:{simpleText:`Video ${n}`},lines:[]}}}});
const response=data=>({success:true,status_code:200,data});

test('Trending keeps TV shelves and Shorts pages a flat grid with profile-owned cursors',async()=>{
  const calls=[];
  const library=new Library(async()=>({actions:{execute:async(path,args)=>{
    calls.push(args);assert.equal(path,'/browse');assert.equal(args.client,'TV');
    if(args.browseId==='FEtrending')return response({sectionListRenderer:{contents:[{shelfRenderer:{title:{simpleText:'Now'},
      content:{horizontalListRenderer:{items:[tile(1),tile(2)]}}}}]}});
    if(args.continuation==='shorts-next')return response({continuationContents:{gridContinuation:{items:[tile(90,true)]}}});
    return response({tvBrowseRenderer:{content:{tvSurfaceContentRenderer:{content:{gridRenderer:{
      items:Array.from({length:30},(_,i)=>tile(i,true)),continuations:[{nextContinuationData:{continuation:'shorts-next'}}]}}}}}});
  }}}),profiles);
  const trending=await library.execute({action:'topics',section:'trending'});
  assert.equal(calls[0].browseId,'FEtrending');assert.equal(trending.items[0].title,'Now');
  assert.ok(trending.items[0].id.startsWith('section:trending:'));
  const key=trending.items[0].id.split(':')[2];
  await assert.rejects(library.execute({action:'topic_open',section:'shorts',id:key}),{code:'cursor_expired'});
  const row=await library.execute({action:'topic_open',section:'trending',id:key});
  assert.deepEqual(row.items.map(item=>item.id),[pad(1),pad(2)]);
  let page=await library.execute({action:'topics',section:'shorts'});const ids=[];
  assert.equal(calls.at(-1).browseId,'FEshorts');
  await assert.rejects(library.execute({action:'continue',cursor:page.cursor,profile:'other'}),{code:'cursor_expired'});
  for(;;) {ids.push(...page.items.map(item=>item.id));if(!page.has_more)break;
    page=await library.execute({action:'continue',cursor:page.cursor});}
  assert.deepEqual(ids,[...Array.from({length:30},(_,i)=>pad(i)),pad(90)]);
  assert.equal(calls.at(-1).continuation,'shorts-next');
});

test('own uploads require a signed-in profile and legacy sections keep their strict layout',async()=>{
  const calls=[];let logged=true;
  const library=new Library(async profile=>({session:{logged_in:logged && profile.id!=='guest'},actions:{execute:async(_,args)=>{
    calls.push(args);return response({gridRenderer:{items:[tile(7)]}});
  }}}),profiles);
  await assert.rejects(library.execute({action:'topics',section:'my_videos'}),{code:'account_required',status:401});
  assert.equal(calls.length,0);
  const page=await library.execute({action:'topics',section:'my_videos',profile:'signed'});
  assert.equal(calls[0].browseId,'FEmy_videos');assert.deepEqual(page.items.map(item=>item.id),[pad(7)]);
  logged=false;
  const expired=new Library(async()=>({session:{logged_in:false},actions:{execute:async()=>{throw new Error('unexpected');}}}),profiles);
  await assert.rejects(expired.execute({action:'topics',section:'my_videos',profile:'signed'}),{code:'account_required'});
  await assert.rejects(library.execute({action:'topics',section:'news'}),{code:'upstream_format'});
});

test('subscribed channels are account-only folders with pagination',async()=>{
  const node=n=>new YTNodes.GridChannel({channelId:'UC'+String(n).padStart(22,'0'),title:{simpleText:'Channel '+n},
    thumbnail:{thumbnails:[{url:'https://yt3.ggpht.com/a'+n,width:88,height:88}]},subscriberCountText:{simpleText:n+' subscribers'},
    navigationEndpoint:{browseEndpoint:{browseId:'UC'+String(n).padStart(22,'0')}}});
  const tail={channels:[node(40),{id:'not-a-channel'}],has_continuation:false};
  const feed={channels:[...Array.from({length:30},(_,i)=>node(i)),node(0)],has_continuation:true,async getContinuation(){return tail;}};
  let calls=0;
  const library=new Library(async()=>({getChannelsFeed:async()=>{calls++;return feed;}}),profiles);
  await assert.rejects(library.execute({action:'channels'}),{code:'account_required',status:401});assert.equal(calls,0);
  let page=await library.execute({action:'channels',profile:'signed'});
  assert.deepEqual(page.items[3],{kind:'channel',id:'channel:UC'+'3'.padStart(22,'0'),channel_id:'UC'+'3'.padStart(22,'0'),
    title:'Channel 3',subtitle:'3 subscribers',overview:'',art_url:'https://yt3.ggpht.com/a3',is_folder:true});
  const ids=[];
  for(;;) {ids.push(...page.items.map(item=>item.channel_id));if(!page.has_more)break;
    page=await library.execute({action:'continue',cursor:page.cursor,profile:'signed'});}
  assert.equal(ids.length,31);assert.equal(new Set(ids).size,31);assert.equal(calls,1);
});

test('pinned notification menu yields video rows, read state and its own continuation',async()=>{
  const notice=(n,read)=>({notificationRenderer:{notificationId:'private-notification-'+n,read,
    shortMessage:{simpleText:'Uploaded: video '+n},sentTimeText:{simpleText:n+' hours ago'},
    thumbnail:{thumbnails:[{url:'https://yt3.ggpht.com/avatar',width:48,height:48}]},
    videoThumbnail:{thumbnails:[{url:'https://i.ytimg.com/vi/x/default.jpg',width:120,height:90}]},
    navigationEndpoint:n===9?{browseEndpoint:{browseId:'FEcomments'}}:{watchEndpoint:{videoId:pad(n)}},
    recordClickEndpoint:{recordNotificationInteractionsEndpoint:{serializedInteractionsRequest:'private-click'}}}});
  const menu=(items,token)=>({actions:[{openPopupAction:{popup:{multiPageMenuRenderer:{
    header:{simpleMenuHeaderRenderer:{title:{simpleText:'Notifications'}}},
    sections:[{multiPageMenuNotificationSectionRenderer:{items:[...items,...(token?[{continuationItemRenderer:{
      continuationEndpoint:{commandMetadata:{webCommandMetadata:{apiUrl:'/youtubei/v1/notification/get_notification_menu'}},
        getNotificationMenuEndpoint:{ctoken:token}}}}]:[])]}}]}}}}]});
  const calls=[];
  const actions={async execute(path,args) {
    calls.push(path);assert.equal(path,'notification/get_notification_menu');assert.equal(args.ctoken,'private-next');
    return {success:true,status_code:200,data:menu([notice(3,true)])};
  }};
  const first=new YT.NotificationsMenu(actions,{data:menu([notice(1,false),notice(9,false),notice(2,true)],'private-next')});
  assert.equal(first.contents.length,3);
  const library=new Library(async()=>({getNotifications:async()=>first}),profiles);
  await assert.rejects(library.execute({action:'notifications'}),{code:'account_required'});
  const page=await library.execute({action:'notifications',profile:'signed'});
  assert.deepEqual(page.items.map(item=>[item.kind,item.id,item.title,item.subtitle,item.unread]),
    [['video',pad(1),'Uploaded: video 1','1 hours ago',true],['video',pad(2),'Uploaded: video 2','2 hours ago',false]]);
  assert.equal(page.items[0].art_url,'https://i.ytimg.com/vi/x/default.jpg');
  assert.ok(page.has_more);assert.ok(!JSON.stringify(page).includes('private'));
  const next=await library.execute({action:'continue',cursor:page.cursor,profile:'signed'});
  assert.deepEqual(next.items.map(item=>item.id),[pad(3)]);assert.ok(!next.has_more);assert.equal(calls.length,1);
  const broken=new Library(async()=>({getNotifications:async()=>({})}),profiles);
  await assert.rejects(broken.execute({action:'notifications',profile:'signed'}),{code:'upstream_format'});
});

test('description returns the complete bounded text with pinned player metadata',async()=>{
  const info=text=>new YT.VideoInfo([{data:{videoDetails:{videoId:id,lengthSeconds:'125',thumbnail:{thumbnails:[]},
    title:'Fixture <b>title</b>',author:'Fixture channel',channelId:'UC'+'a'.repeat(22),viewCount:'1234567',shortDescription:text},
    microformat:{playerMicroformatRenderer:{title:{simpleText:'x'},description:{simpleText:'x'},thumbnail:{thumbnails:[]},
      publishDate:'2024-05-06T07:00:00-07:00'}}}}],{},'fixture-nonce');
  let text='Line one\nLine <i>two</i> 日本語 😀';
  const library=new Library(async()=>({getBasicInfo:async()=>info(text)}));
  assert.deepEqual(await library.execute({action:'description',id}),{api_version:1,kind:'description',id,
    title:'Fixture <b>title</b>',channel:'Fixture channel',channel_id:'UC'+'a'.repeat(22),published:'2024-05-06',
    views:'1234567',like_count:'',description:text,truncated:false});
  text='a'.repeat(19999)+'😀'+'tail';
  const long=await library.execute({action:'description',id});
  assert.equal(long.description,'a'.repeat(19999));assert.equal(long.truncated,true);
  text='b'.repeat(20000);
  const exact=await library.execute({action:'description',id});
  assert.equal(exact.description.length,20000);assert.equal(exact.truncated,false);
  await assert.rejects(library.execute({action:'description',id:'bad'}),{code:'invalid_identity'});
  await assert.rejects(library.execute({action:'description',id:pad(1)}),{code:'upstream_format',status:502});
  const bare=new Library(async()=>({getBasicInfo:async()=>({basic_info:{id,view_count:NaN}})}));
  assert.deepEqual(await bare.execute({action:'description',id}),{api_version:1,kind:'description',id,title:'',channel:'',
    channel_id:'',published:'',views:'',like_count:'',description:'',truncated:false});
});
