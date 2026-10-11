// Kids, reminders, playlist order, notification level, details, comment
// posting, first search result, new-content marks and the proxy tunnel.
// Offline: pinned parser objects and local sockets only. GPL-3.0-or-later
import test from 'node:test';
import assert from 'node:assert/strict';
import http from 'node:http';
import {Library,LibraryError,normalizeVideo,imageUrl} from './library.mjs';
import {reminderControl} from './stream_reminder.mjs';
import {playlistOrders,orderNames} from './playlist_order.mjs';
import {newContent} from './channel_marks.mjs';
import {commentText} from './comment_posts.mjs';
import {proxyFetch,proxySettings} from './proxy_fetch.mjs';
import {Intents} from './intents.mjs';
import {YT,YTKids,YTNodes,Parser} from 'youtubei.js';

const id='abcdefghijk',UC='UC'+'a'.repeat(22),pad=n=>String(n).padStart(11,'0'),channel=n=>'UC'+String(n).padStart(22,'0');
const profiles=[{id:'signed',cookie:'synthetic'},{id:'other',cookie:'synthetic'}];
const ok=(data={})=>({success:true,status_code:200,data});

const kidsVideo=n=>({compactVideoRenderer:{videoId:pad(n),title:{simpleText:`Kids ${n}`},
  thumbnail:{thumbnails:[{url:'https://i.ytimg.com/vi/x/default.jpg'}]},lengthText:{simpleText:'2:03'},thumbnailOverlays:[],
  longBylineText:{runs:[{text:'Kids channel',navigationEndpoint:{browseEndpoint:{browseId:UC}}}]}}});
const kidsHome=videos=>({header:{kidsCategoriesHeaderRenderer:{categoryTabs:['Shows','Music'].map(name=>({kidsCategoryTabRenderer:{
  title:{runs:[{text:name}]},endpoint:{browseEndpoint:{browseId:'FEkids_home',params:name.toLowerCase()}}}}))}},
  contents:{kidsHomeScreenRenderer:{anchors:[{anchoredSectionRenderer:{title:'Shows',
    navigationEndpoint:{browseEndpoint:{browseId:'FEkids_home',params:'shows'}},
    content:{sectionListRenderer:{contents:[{itemSectionRenderer:{contents:videos}}]}}}}]}}});

test('the Kids section lists the pinned Kids client category tabs and opens one on demand',async()=>{
  const calls=[];let time=0;
  const actions={session:{},execute:async(path,args)=>{calls.push([path,args]);
    return Parser.parseResponse(kidsHome([kidsVideo(1),kidsVideo(2),kidsVideo(1)]));}};
  const client={kids:{getHomeFeed:async()=>new YTKids.HomeFeed(actions,{data:kidsHome([kidsVideo(9)])})}};
  const library=new Library(async()=>client,profiles,()=>time);
  const root=await library.execute({action:'topics',section:'kids'});
  assert.deepEqual(root.items.map(item=>[item.title,item.subtitle,item.is_folder]),[['Shows','YouTube Kids',true],['Music','YouTube Kids',true]]);
  assert.match(root.items[1].id,/^section:kids:[0-9a-f-]{36}$/);assert.equal(calls.length,0);
  const key=root.items[1].id.split(':')[2];
  await assert.rejects(library.execute({action:'topic_open',section:'kids',id:key,profile:'other'}),{code:'cursor_expired'});
  const page=await library.execute({action:'topic_open',section:'kids',id:key});
  assert.deepEqual(calls,[['browse',{browseId:'FEkids_home',params:'music',client:'YTKIDS',parse:true}]]);
  assert.deepEqual(page.items.map(item=>[item.id,item.title,item.subtitle,item.channel_id,item.duration_sec]),
    [[pad(1),'Kids 1','Kids channel',UC,123],[pad(2),'Kids 2','Kids channel',UC,123]]);
  assert.ok(!page.has_more);assert.ok(page.items[0].playback_context.group);
  await library.execute({action:'topic_open',section:'kids',id:key});assert.equal(calls.length,1);
  time=600001;
  await assert.rejects(library.execute({action:'topic_open',section:'kids',id:key}),{code:'cursor_expired',status:409});
  for(const broken of [{},{header:{category_tabs:[]}},{header:{category_tabs:[{title:'x'}]},selectCategoryTab(){}}])
    await assert.rejects(new Library(async()=>({kids:{getHomeFeed:async()=>broken}})).execute({action:'topics',section:'kids'}),
      {code:'upstream_format',status:502});
  await assert.rejects(new Library(async()=>({})).execute({action:'topics',section:'kids'}),{code:'catalog_unavailable',status:404});
});

const reminderToggle=(toggled,extra={})=>({toggleButtonRenderer:{isToggled:toggled,defaultText:{simpleText:'Localized'},
  defaultServiceEndpoint:{addUpcomingEventReminderEndpoint:{params:'private-add'}},
  toggledServiceEndpoint:{removeUpcomingEventReminderEndpoint:{params:'private-remove'}},...extra}});
const upcomingPlayer=(buttons,details={})=>({videoDetails:{videoId:id,isUpcoming:true,...details},playabilityStatus:{status:'LIVE_STREAM_OFFLINE',
  liveStreamability:{liveStreamabilityRenderer:{offlineSlate:{liveStreamOfflineSlateRenderer:{actionButtons:buttons}}}}}});

test('a stream reminder is read from one unambiguous toggle and changed once per confirmation',async()=>{
  assert.deepEqual(reminderControl(upcomingPlayer([reminderToggle(false)])),{set:'private-add',unset:'private-remove',reminder_set:false});
  for(const raw of [null,{},upcomingPlayer([]),upcomingPlayer([reminderToggle(false),reminderToggle(true)]),
    upcomingPlayer([reminderToggle(false,{isDisabled:true})]),upcomingPlayer([reminderToggle('yes')]),
    upcomingPlayer([{toggleButtonRenderer:{isToggled:false,defaultServiceEndpoint:{likeEndpoint:{}}}}]),
    upcomingPlayer([reminderToggle(false,{toggledServiceEndpoint:{removeUpcomingEventReminderEndpoint:{params:'x'.repeat(4097)}}})])])
    assert.equal(reminderControl(raw),null);
  const sent=[];let player=upcomingPlayer([reminderToggle(false)]),accept=true,signedIn=true;
  const client={get session() {return {logged_in:signedIn};},actions:{async execute(path,body) {
    if(path==='/player') {assert.deepEqual(body,{videoId:id});return ok(player);}
    sent.push([path,body]);return accept?ok({}):{success:false,status_code:400,data:{}};}}};
  const library=new Library(async()=>client,profiles);
  assert.deepEqual(await library.execute({action:'stream_reminder',id,reminder:'status'}),
    {api_version:1,kind:'stream_reminder',id,upcoming:true,available:true,reminder_set:false});
  const request={action:'stream_reminder',profile:'signed',id,reminder:'set',session:'intent-1'};
  await assert.rejects(library.execute({...request,profile:'guest'}),{code:'account_required',status:401});
  await assert.rejects(library.execute({...request,reminder:'toggle'}),{code:'invalid_request'});
  await assert.rejects(library.execute({...request,session:''}),{code:'invalid_request'});
  await assert.rejects(library.execute({...request,id:'bad'}),{code:'invalid_identity'});
  const result=await library.execute(request);
  assert.deepEqual(result,{api_version:1,success:true,id,reminder_set:true});
  assert.strictEqual(await library.execute(request),result);
  assert.deepEqual(sent,[['notification/add_upcoming_event_reminder',{params:'private-add'}]]);
  await assert.rejects(library.execute({...request,reminder:'unset'}),{code:'reminder_unconfirmed',status:409});
  // A reminder that already has the wanted state sends nothing.
  player=upcomingPlayer([reminderToggle(true)]);
  assert.deepEqual(await library.execute({...request,session:'intent-2'}),{api_version:1,success:true,id,reminder_set:true});
  assert.equal(sent.length,1);
  assert.deepEqual(await library.execute({...request,reminder:'unset',session:'intent-3'}),{api_version:1,success:true,id,reminder_set:false});
  assert.deepEqual(sent[1],['notification/remove_upcoming_event_reminder',{params:'private-remove'}]);
  // No offered control: nothing is sent and the same confirmation stays usable.
  player=upcomingPlayer([]);
  await assert.rejects(library.execute({...request,session:'intent-4'}),{code:'reminder_unavailable',status:404});
  assert.deepEqual(await library.execute({action:'stream_reminder',id,reminder:'status'}),
    {api_version:1,kind:'stream_reminder',id,upcoming:true,available:false,reminder_set:false});
  player=upcomingPlayer([reminderToggle(false)]);accept=false;
  await assert.rejects(library.execute({...request,session:'intent-4'}),{code:'reminder_unconfirmed',status:502});
  accept=true;
  await assert.rejects(library.execute({...request,session:'intent-4'}),{code:'reminder_unconfirmed',status:409});
  assert.equal(sent.length,3);
  // A video that is not upcoming, or a reply for another video, offers nothing.
  player=upcomingPlayer([reminderToggle(false)],{isUpcoming:false});
  assert.equal((await library.execute({action:'stream_reminder',id,reminder:'status'})).available,false);
  player=upcomingPlayer([reminderToggle(false)],{videoId:pad(7)});
  await assert.rejects(library.execute({action:'stream_reminder',id,reminder:'status'}),{code:'upstream_format',status:502});
  await assert.rejects(library.execute({...request,session:'intent-5'}),{code:'upstream_format'});
  signedIn=false;
  await assert.rejects(library.execute({...request,session:'intent-6'}),{code:'account_required',status:401});
  assert.equal(sent.length,3);assert.ok(!JSON.stringify(result).includes('private'));
});

test('grid rows report the parsed reminder state and other rows leave it unknown',()=>{
  const grid=reminder=>new YTNodes.GridVideo({videoId:id,title:{simpleText:'Premiere'},thumbnail:{thumbnails:[]},thumbnailOverlays:[],
    navigationEndpoint:{watchEndpoint:{videoId:id}},upcomingEventData:{startTime:'4102444800',upcomingEventText:{simpleText:'Soon'},isReminderSet:reminder}});
  assert.equal(normalizeVideo(grid(true)).reminder_set,true);assert.equal(normalizeVideo(grid(false)).reminder_set,false);
  assert.equal(normalizeVideo(grid(true)).is_upcoming,true);
  assert.ok(!('reminder_set' in normalizeVideo({id,title:'Plain',is_upcoming:true})));
});

const orderItem=(value,selected=false,target='PLabcdefghijk',action='ACTION_SET_PLAYLIST_VIDEO_ORDER')=>({title:'Localized',selected,
  serviceEndpoint:{commandMetadata:{webCommandMetadata:{sendPost:true,apiUrl:'/youtubei/v1/browse/edit_playlist'}},
    playlistEditEndpoint:{playlistId:target,actions:[{action,playlistVideoOrder:value}]}}});
const playlistPage=items=>({contents:{twoColumnBrowseResultsRenderer:{tabs:[{tabRenderer:{content:{sectionListRenderer:{contents:[
  {itemSectionRenderer:{contents:[{playlistVideoListRenderer:{playlistId:'PLabcdefghijk',isEditable:true,contents:[],
    sortFilterMenu:{sortFilterSubMenuRenderer:{subMenuItems:items}}}}]}}]}}}}]}}});

test('playlist orders come from the page sort menu and only an offered value is sent',async()=>{
  const playlist='PLabcdefghijk';
  const all=playlistOrders(playlistPage([orderItem(6),orderItem(1),orderItem(2,true),orderItem(3),orderItem(4),orderItem(5)]),playlist);
  assert.deepEqual([...all.offered],orderNames.map((name,index)=>[name,index+1]));assert.equal(all.order,'added_oldest');
  // Foreign targets, other edit actions, unknown values and conflicts expose nothing.
  const partial=playlistOrders(playlistPage([orderItem(1,false,'PLzyxwvutsrqp'),orderItem(2,false,playlist,'ACTION_REMOVE_VIDEO'),
    orderItem('3'),orderItem(4),orderItem(4),orderItem(5,true),orderItem(3,true)]),playlist);
  assert.deepEqual([...partial.offered],[['published_oldest',5],['popular',3]]);assert.equal(partial.order,'');
  for(const raw of [null,{},playlistPage([]),playlistPage('bad')])assert.equal(playlistOrders(raw,playlist).offered.size,0);
  const sent=[];let page=playlistPage([orderItem(1,true),orderItem(3)]),status='STATUS_SUCCEEDED';
  const client={session:{logged_in:true},actions:{async execute(path,body) {
    if(path==='/browse') {assert.deepEqual(body,{browseId:'VL'+playlist});return ok(page);}
    sent.push([path,body]);return ok({status});}}};
  const library=new Library(async()=>client,profiles);
  const read={action:'playlist_order',profile:'signed',playlist};
  assert.deepEqual(await library.execute(read),{api_version:1,kind:'playlist_order',playlist_id:playlist,editable:true,
    orders:['added_newest','popular'],order:'added_newest'});
  await assert.rejects(library.execute({...read,profile:'guest'}),{code:'account_required',status:401});
  await assert.rejects(library.execute({...read,playlist:'bad'}),{code:'invalid_request'});
  await assert.rejects(library.execute({...read,playlist_order:'manual',session:'intent-1'}),{code:'invalid_request'});
  await assert.rejects(library.execute({...read,playlist_order:'popular'}),{code:'invalid_request'});
  const change={...read,playlist_order:'popular',session:'intent-1'};
  const result=await library.execute(change);
  assert.deepEqual(result,{api_version:1,success:true,playlist_id:playlist,order:'popular'});
  assert.strictEqual(await library.execute(change),result);
  assert.deepEqual(sent,[['browse/edit_playlist',{playlistId:playlist,actions:[{action:'ACTION_SET_PLAYLIST_VIDEO_ORDER',playlistVideoOrder:3}]}]]);
  await assert.rejects(library.execute({...change,playlist_order:'added_newest'}),{code:'playlist_unconfirmed',status:409});
  // An order the page does not offer sends nothing and leaves the confirmation usable.
  const absent={...read,playlist_order:'published_oldest',session:'intent-2'};
  await assert.rejects(library.execute(absent),{code:'playlist_order_unavailable',status:404});
  page=playlistPage([orderItem(5)]);status='STATUS_FAILED';
  await assert.rejects(library.execute(absent),{code:'playlist_unconfirmed',status:502});
  status='STATUS_SUCCEEDED';
  await assert.rejects(library.execute(absent),{code:'playlist_unconfirmed',status:409});
  assert.equal(sent.length,2);
  page=playlistPage([]);
  assert.deepEqual(await library.execute(read),{api_version:1,kind:'playlist_order',playlist_id:playlist,editable:false,orders:[],order:''});
});

test('a subscription can carry a notification level and the level can be changed alone',async()=>{
  const calls=[];let levelAccepted=true;
  const client={session:{logged_in:true},interact:{
    subscribe:async channelId=>{calls.push(['subscribe',channelId]);return ok({});},
    setNotificationPreferences:async(channelId,type)=>{calls.push(['level',channelId,type]);
      if(levelAccepted===null)throw new Error('private failure');
      return levelAccepted?ok({}):{success:false,status_code:400,data:{}};}}};
  const library=new Library(async()=>client,profiles);
  const request={action:'subscribe',profile:'signed',id:UC};
  assert.deepEqual(await library.execute(request),{api_version:1,success:true,playlist_id:''});
  assert.deepEqual(await library.execute({...request,notifications:''}),{api_version:1,success:true,playlist_id:''});
  assert.deepEqual(await library.execute({...request,notifications:'all'}),{api_version:1,success:true,playlist_id:'',notifications:'all'});
  assert.deepEqual(calls.slice(2),[['subscribe',UC],['level',UC,'ALL']]);
  // The subscription stands when the level is refused or fails.
  levelAccepted=false;
  assert.equal((await library.execute({...request,notifications:'none'})).notifications,'');
  levelAccepted=null;
  assert.deepEqual(await library.execute({...request,notifications:'personalized'}),{api_version:1,success:true,playlist_id:'',notifications:''});
  levelAccepted=true;
  const only={action:'subscribe_notifications',profile:'signed',id:UC,notifications:'personalized'};
  assert.deepEqual(await library.execute(only),{api_version:1,success:true,playlist_id:'',notifications:'personalized'});
  assert.deepEqual(calls.at(-1),['level',UC,'PERSONALIZED']);
  levelAccepted=false;
  await assert.rejects(library.execute(only),{code:'operation_failed',status:502});
  const before=calls.length;
  await assert.rejects(library.execute({...only,notifications:''}),{code:'invalid_request'});
  await assert.rejects(library.execute({...only,notifications:'loud'}),{code:'invalid_request'});
  await assert.rejects(library.execute({...request,notifications:'ALL'}),{code:'invalid_request'});
  await assert.rejects(library.execute({action:'unsubscribe',profile:'signed',id:UC,notifications:'all'}),{code:'invalid_request'});
  await assert.rejects(library.execute({...only,id:'bad'}),{code:'invalid_identity'});
  await assert.rejects(library.execute({...only,profile:'guest'}),{code:'account_required',status:401});
  assert.equal(calls.length,before);
});

const likeButton=label=>({segmentedLikeDislikeButtonViewModel:{likeButtonViewModel:{likeButtonViewModel:{toggleButtonViewModel:{toggleButtonViewModel:{
  defaultButtonViewModel:{buttonViewModel:{title:'4.3K',accessibilityText:label}},toggledButtonViewModel:{buttonViewModel:{title:'4.3K'}}}},
  likeStatusEntityKey:'k',likeStatusEntity:{key:'k',likeStatus:'INDIFFERENT'}}},
  dislikeButtonViewModel:{dislikeButtonViewModel:{toggleButtonViewModel:{toggleButtonViewModel:{defaultButtonViewModel:{buttonViewModel:{title:''}},
    toggledButtonViewModel:{buttonViewModel:{title:''}}}}}},likeCountEntity:{key:'c'},
  dynamicLikeCountUpdateData:{updateStatusKey:'a',placeholderLikeCountValuesKey:'b',updateDelayLoopId:'c',updateDelaySec:5}}});
const watchNext=(related,avatar='https://yt3.ggpht.com/avatar=s88')=>({contents:{twoColumnWatchNextResults:{results:{results:{contents:[
  {videoPrimaryInfoRenderer:{title:{runs:[{text:'t'}]},videoActions:{menuRenderer:{topLevelButtons:[likeButton('like this video along with 4,321 other people')]}}}},
  {videoSecondaryInfoRenderer:{owner:{videoOwnerRenderer:{title:{runs:[{text:'Fixture channel',navigationEndpoint:{browseEndpoint:{browseId:UC}}}]},
    thumbnail:{thumbnails:[{url:avatar,width:88,height:88}]},subscriberCountText:{simpleText:'1.2M subscribers'},
    navigationEndpoint:{browseEndpoint:{browseId:UC}}}},description:{runs:[{text:'d'}]},showMoreText:{simpleText:'more'},showLessText:{simpleText:'less'}}}]}},
  secondaryResults:{secondaryResults:{results:related}}}}});
const playerData=(description,extra={},formats=[])=>({videoDetails:{videoId:id,lengthSeconds:'125',thumbnail:{thumbnails:[]},title:'Fixture <b>title</b>',
  author:'Fixture channel',channelId:UC,viewCount:'1234567',shortDescription:description,...extra},
  streamingData:{expiresInSeconds:'100',formats:[],adaptiveFormats:formats},
  microformat:{playerMicroformatRenderer:{title:{simpleText:'x'},description:{simpleText:'x'},thumbnail:{thumbnails:[]},publishDate:'2024-05-06T07:00:00-07:00'}}});
const format=height=>({itag:313,mimeType:'video/webm; codecs="vp9"',bitrate:1,width:Math.round(height*16/9),height,url:'https://r1.googlevideo.com/x',
  lastModified:'1',contentLength:'1',quality:'hd',fps:30,qualityLabel:`${height}p`,projectionType:'RECTANGULAR',averageBitrate:1,approxDurationMs:'1'});
const related=n=>({compactVideoRenderer:{videoId:pad(n),title:{simpleText:'Related'},thumbnail:{thumbnails:[]},longBylineText:{runs:[{text:'x'}]},thumbnailOverlays:[]}});

test('video details summarize the pinned watch page for a details screen',async()=>{
  let info=new YT.VideoInfo([{data:playerData('Line one\n日本語 😀',{isUpcoming:true},[format(1080),format(2160)])},{data:watchNext([related(5)])}],{session:{}},'nonce');
  const calls=[];
  const library=new Library(async()=>({getInfo:async value=>{calls.push(value);return info;}}));
  assert.deepEqual(await library.execute({action:'video_details',id}),{api_version:1,kind:'video_details',id,title:'Fixture <b>title</b>',
    channel:{id:UC,name:'Fixture channel',subscribers:'1.2M subscribers',art_url:'https://yt3.ggpht.com/avatar=s88'},
    published:'2024-05-06',views:'1234567',like_count:'4321',description:'Line one\n日本語 😀',description_truncated:false,
    duration_sec:125,badges:['upcoming','4k'],has_related:true});
  assert.deepEqual(calls,[id]);
  // Bounded excerpt, live badge, foreign avatar origin and an empty related list.
  info=new YT.VideoInfo([{data:playerData('a'.repeat(599)+'😀tail',{isLive:true,isLiveContent:true},[format(720)])},
    {data:watchNext([],'https://example.com/avatar.jpg')}],{session:{}},'nonce');
  const live=await library.execute({action:'video_details',id});
  assert.equal(live.description,'a'.repeat(599));assert.equal(live.description_truncated,true);
  assert.deepEqual(live.badges,['live']);assert.equal(live.channel.art_url,'');assert.equal(live.has_related,false);
  // Player metadata alone still answers; unknown counts stay empty.
  const bare=new Library(async()=>({getInfo:async()=>({basic_info:{id,like_count:NaN}})}));
  assert.deepEqual(await bare.execute({action:'video_details',id}),{api_version:1,kind:'video_details',id,title:'',
    channel:{id:'',name:'',subscribers:'',art_url:''},published:'',views:'',like_count:'',description:'',description_truncated:false,
    duration_sec:0,badges:[],has_related:false});
  await assert.rejects(library.execute({action:'video_details',id:'bad'}),{code:'invalid_identity'});
  await assert.rejects(library.execute({action:'video_details',id:pad(1)}),{code:'upstream_format',status:502});
});

const commentThread=(n,extra={})=>({commentThreadRenderer:{commentViewModel:{commentViewModel:{commentId:'c'+n,commentKey:'k'+n,
  toolbarStateKey:'ts'+n,toolbarSurfaceKey:'tf'+n,commentSurfaceKey:'cs'+n,sharedKey:'sh'+n,...extra}}}});
const command=action=>({innertubeCommand:{performCommentActionEndpoint:{action}}});
const commentMutations=(n,state={})=>[
  {entityKey:'k'+n,payload:{commentEntityPayload:{key:'k'+n,properties:{commentId:'c'+n,content:{content:'Comment '+n},publishedTime:'1 day ago',replyLevel:0},
    author:{channelId:UC,displayName:'@viewer',avatarThumbnailUrl:'https://yt3.ggpht.com/a',isCreator:!!state.creator,
      ...(state.member?{sponsorBadgeUrl:'https://yt3.ggpht.com/badge'}:{})},toolbar:{likeCountNotliked:'5',likeCountLiked:'6',replyCount:'0'}}}},
  {entityKey:'ts'+n,payload:{engagementToolbarStateEntityPayload:{key:'ts'+n,likeState:state.like ?? 'TOOLBAR_LIKE_STATE_INDIFFERENT',
    heartState:state.hearted?'TOOLBAR_HEART_STATE_HEARTED':'TOOLBAR_HEART_STATE_UNHEARTED'}}},
  {entityKey:'tf'+n,payload:{engagementToolbarSurfaceEntityPayload:{key:'tf'+n,likeCommand:command('like'),dislikeCommand:command('dislike'),
    unlikeCommand:command('unlike'),undislikeCommand:command('undislike'),
    replyCommand:{innertubeCommand:{createCommentReplyDialogEndpoint:{dialog:{commentReplyDialogRenderer:{
      replyButton:{buttonRenderer:{text:{simpleText:'Reply'},...(state.noReply?{}:{serviceEndpoint:{
        commandMetadata:{webCommandMetadata:{sendPost:true,apiUrl:'/youtubei/v1/comment/create_comment_reply'}},
        createCommentReplyEndpoint:{createReplyParams:'private-reply-'+n}}})}},
      cancelButton:{buttonRenderer:{text:{simpleText:'Cancel'}}},placeholderText:{simpleText:'Add a reply'},errorMessage:{simpleText:'e'}}}}}}}}}];
const commentsPage=(simplebox=true)=>({onResponseReceivedEndpoints:[
  {reloadContinuationItemsCommand:{targetId:'comments-header',continuationItems:[{commentsHeaderRenderer:{
    countText:{runs:[{text:'1,234'},{text:' Comments'}]},commentsCount:{simpleText:'1.2K'},
    ...(simplebox?{createRenderer:{commentSimpleboxRenderer:{submitButton:{buttonRenderer:{text:{simpleText:'Comment'},serviceEndpoint:{
      commandMetadata:{webCommandMetadata:{sendPost:true,apiUrl:'/youtubei/v1/comment/create_comment'}},createCommentEndpoint:{createCommentParams:'private-create'}}}},
      cancelButton:{buttonRenderer:{text:{simpleText:'Cancel'}}},placeholderText:{simpleText:'Add'},authorThumbnail:{thumbnails:[]}}}}:{})}}]}},
  {reloadContinuationItemsCommand:{targetId:'comments-section',continuationItems:[commentThread(1,{pinnedText:'Pinned'}),commentThread(2),commentThread(3)]}}],
  frameworkUpdates:{entityBatchUpdate:{mutations:[
    ...commentMutations(1,{creator:true,member:true,hearted:true,like:'TOOLBAR_LIKE_STATE_LIKED'}),
    ...commentMutations(2,{like:'TOOLBAR_LIKE_STATE_DISLIKED'}),...commentMutations(3,{noReply:true})]}}});

test('comment rows carry viewer and creator state, and posting happens once per confirmation',async()=>{
  const sent=[];let status='STATUS_SUCCEEDED',signedIn=true;
  const actions={session:{logged_in:true},execute:async(path,body)=>{sent.push([path,body]);return ok({actionResult:{status}});}};
  const client={get session() {return {logged_in:signedIn};},getComments:async()=>new YT.Comments(actions,commentsPage()),
    interact:{comment:async(video,text)=>{sent.push(['comment',video,text]);return ok({actionResult:{status}});}}};
  const library=new Library(async()=>client,profiles);
  const page=await library.execute({action:'comments',profile:'signed',id});
  assert.equal(page.comment_count,'1.2K');assert.equal(page.can_post,true);
  assert.deepEqual(page.items.map(item=>[item.id,item.pinned,item.owner,item.liked,item.disliked,item.hearted,item.member]),
    [['c1',true,true,true,false,true,true],['c2',false,false,false,true,false,false],['c3',false,false,false,false,false,false]]);
  assert.equal(page.items[0].art_url,'https://yt3.ggpht.com/a');
  assert.match(page.items[0].respond_cursor,/^[0-9a-f-]{36}$/);assert.equal(page.items[2].respond_cursor,'');
  assert.ok(!JSON.stringify(page).includes('private'));
  // Guest rows expose no reply reference and cannot post.
  const guest=await library.execute({action:'comments',id});
  assert.equal(guest.can_post,false);assert.ok(guest.items.every(item=>item.respond_cursor===''));
  const post={action:'comment_post',profile:'signed',id,text:'  First 日本語 😀\nsecond line  ',session:'intent-1'};
  await assert.rejects(library.execute({...post,profile:'guest'}),{code:'account_required',status:401});
  for(const text of ['','   ','x'.repeat(1001),'tab\there','nul\0',7,undefined])
    await assert.rejects(library.execute({...post,text}),{code:'invalid_comment'});
  await assert.rejects(library.execute({...post,session:'bad session'}),{code:'invalid_request'});
  assert.equal(sent.length,0);
  const result=await library.execute(post);
  assert.deepEqual(result,{api_version:1,success:true,id,session:'intent-1',reply:false});
  assert.strictEqual(await library.execute(post),result);
  assert.deepEqual(sent,[['comment',id,'First 日本語 😀\nsecond line']]);
  await assert.rejects(library.execute({...post,text:'Another text'}),{code:'comment_unconfirmed',status:409});
  const reply={action:'comment_reply',profile:'signed',id,cursor:page.items[1].respond_cursor,text:'A reply',session:'intent-2'};
  await assert.rejects(library.execute({...reply,profile:'other'}),{code:'cursor_expired',status:409});
  await assert.rejects(library.execute({...reply,cursor:'missing'}),{code:'cursor_expired'});
  assert.deepEqual(await library.execute(reply),{api_version:1,success:true,id,session:'intent-2',reply:true});
  assert.deepEqual(sent[1],['comment/create_comment_reply',{createReplyParams:'private-reply-2',commentText:'A reply'}]);
  // An unconfirmed post is never sent again under the same confirmation.
  status='STATUS_FAILED';
  await assert.rejects(library.execute({...reply,session:'intent-3'}),{code:'comment_unconfirmed',status:502});
  status='STATUS_SUCCEEDED';
  await assert.rejects(library.execute({...reply,session:'intent-3'}),{code:'comment_unconfirmed',status:409});
  assert.equal(sent.length,3);
  signedIn=false;
  await assert.rejects(library.execute({...post,session:'intent-4'}),{code:'account_required',status:401});
  assert.equal(commentText(' ok ',LibraryError),'ok');assert.equal(commentText('😀'.repeat(1000),LibraryError).length,2000);
  const closed=new Library(async()=>({session:{logged_in:true},getComments:async()=>new YT.Comments(actions,commentsPage(false))}),profiles);
  assert.equal((await closed.execute({action:'comments',profile:'signed',id})).can_post,false);
});

test('a search can return only its first playable video',async()=>{
  const queries=[];
  const results=[{type:'Channel',id:UC,author:{name:'A channel'}},{type:'Video',id:pad(1),title:'Premiere',is_upcoming:true},
    {type:'Playlist',id:'PLabcdefghijk',title:'A list'},{type:'Video',id:pad(2),title:'Playable',is_live:true},{type:'Video',id:pad(3),title:'Later'}];
  let feed={results,has_continuation:true,getContinuation:async()=>{throw new Error('never requested');}};
  const library=new Library(async()=>({search:async(query,filters)=>{queries.push([query,filters]);return feed;}}));
  const found=await library.execute({action:'search',query:' play this ',first_playable:true,type:'video'});
  assert.deepEqual(Object.keys(found),['api_version','kind','query','found','item']);
  assert.equal(found.kind,'search_first');assert.equal(found.query,'play this');assert.equal(found.found,true);
  assert.equal(found.item.id,pad(2));assert.equal(found.item.kind,'video');assert.equal(found.item.is_live,true);
  assert.ok(found.item.playback_context.group);assert.equal(library.cursors.size,0);
  assert.deepEqual(queries,[['play this',{type:'video'}]]);
  feed={results:results.slice(0,3),has_continuation:false};
  assert.deepEqual(await library.execute({action:'search',query:'nothing',first_playable:true}),
    {api_version:1,kind:'search_first',query:'nothing',found:false,item:null});
  // Without the flag the ordinary page is unchanged.
  assert.equal((await library.execute({action:'search',query:'nothing',first_playable:false})).kind,'catalog');
  await assert.rejects(library.execute({action:'search',query:'x',first_playable:'yes'}),{code:'invalid_search'});
  await assert.rejects(library.execute({action:'home',first_playable:true}),{code:'invalid_search'});
});

const guideEntry=(channelId,style,extra={})=>({guideEntryRenderer:{formattedTitle:{simpleText:'Localized'},presentationStyle:style,
  entryData:{guideEntryData:{guideEntryId:channelId}},navigationEndpoint:{browseEndpoint:{browseId:channelId}},...extra}});

test('subscribed channels carry a new-content mark only when the guide states one',async()=>{
  const fresh='GUIDE_ENTRY_PRESENTATION_STYLE_NEW_CONTENT',none='GUIDE_ENTRY_PRESENTATION_STYLE_NONE';
  const guide={items:[{guideSubscriptionsSectionRenderer:{items:[guideEntry(channel(1),fresh),guideEntry(channel(2),none),
    guideEntry(channel(3),'GUIDE_ENTRY_PRESENTATION_STYLE_FUTURE'),guideEntry('FEsubscriptions',fresh),
    {guideCollapsibleEntryRenderer:{expandableItems:[guideEntry(channel(4),fresh),guideEntry(channel(5),fresh),guideEntry(channel(5),none)]}}]}}]};
  assert.deepEqual(Object.fromEntries(newContent(guide)),{[channel(1)]:true,[channel(2)]:false,[channel(4)]:true});
  for(const raw of [null,{},{items:'bad'}])assert.equal(newContent(raw).size,0);
  const calls=[];let failing=false;
  const channels=[1,2,3,4,5].map(n=>({type:'Channel',id:channel(n),author:{name:'Channel '+n}}));
  const client={session:{logged_in:true},getChannelsFeed:async()=>({channels,has_continuation:false}),
    actions:{execute:async(path,body)=>{calls.push([path,body]);if(failing)throw new Error('private');return ok(guide);}}};
  const library=new Library(async()=>client,profiles);
  const plain=await library.execute({action:'channels',profile:'signed'});
  assert.ok(plain.items.every(item=>!('has_new' in item)));assert.equal(calls.length,0);
  const marked=await library.execute({action:'channels',profile:'signed',new_content:true});
  assert.deepEqual(calls,[['/guide',{}]]);
  assert.deepEqual(marked.items.map(item=>[item.channel_id,item.has_new]),
    [[channel(1),true],[channel(2),false],[channel(3),undefined],[channel(4),true],[channel(5),undefined]]);
  // A failing guide request leaves the list intact and unmarked.
  failing=true;
  const unmarked=await library.execute({action:'channels',profile:'signed',new_content:true});
  assert.equal(unmarked.items.length,5);assert.ok(unmarked.items.every(item=>!('has_new' in item)));
});

test('confirmation sessions are bounded, expire and release only when nothing was sent',async()=>{
  let time=0;
  const intents=new Intents(LibraryError,()=>time);
  await assert.rejects(intents.once('a','s','i','x_unconfirmed',async release=>{release();throw new LibraryError('nothing_sent','n',404);}),{code:'nothing_sent'});
  assert.equal(await intents.once('a','s','i','x_unconfirmed',async()=>'done'),'done');
  assert.equal(await intents.once('a','s','i','x_unconfirmed',async()=>'again'),'done');
  await assert.rejects(intents.once('a','s','other','x_unconfirmed',async()=>'no'),{code:'x_unconfirmed',status:409});
  assert.equal(await intents.once('b','s','i','x_unconfirmed',async()=>'separate'),'separate');
  let finish;
  const pending=intents.once('a','slow','i','x_unconfirmed',()=>new Promise(resolve=>{finish=resolve;}));
  await assert.rejects(intents.once('a','slow','i','x_unconfirmed',async()=>'dup'),{code:'x_unconfirmed'});
  finish('late');assert.equal(await pending,'late');
  for(let n=intents.entries.size;n<256;n++)await intents.once('fill','s'+n,'i','x',async()=>n);
  await assert.rejects(intents.once('a','full','i','x',async()=>1),{code:'account_busy',status:429});
  time=1800000;
  assert.equal(await intents.once('a','s','other','x_unconfirmed',async()=>'fresh'),'fresh');
});

const listen=server=>new Promise(resolve=>server.listen(0,'127.0.0.1',()=>resolve(server.address().port)));

test('proxy settings accept only a bare HTTP or HTTPS proxy address',()=>{
  assert.deepEqual(proxySettings('http://127.0.0.1:3128'),{secure:false,host:'127.0.0.1',port:3128,authorization:''});
  assert.deepEqual(proxySettings('https://proxy.example/'),{secure:true,host:'proxy.example',port:443,authorization:''});
  assert.deepEqual(proxySettings('http://us%40er:p%3Ass@proxy.example'),{secure:false,host:'proxy.example',port:80,
    authorization:'Basic '+Buffer.from('us@er:p:ss').toString('base64')});
  assert.equal(proxySettings('http://[::1]:8080').host,'::1');
  for(const value of ['','proxy.example:3128','socks5://proxy.example:1080','ftp://proxy.example','http://proxy.example/path',
    'http://proxy.example?x=1','http://proxy.example#f',null,7])
    assert.throws(()=>proxySettings(value),{message:'invalid proxy'});
});

test('requests travel through a CONNECT tunnel and never fall back to a direct connection',async()=>{
  const seen=[];let accept=false,hello=null;
  const server=http.createServer((request,response)=>{seen.push(['plain',request.url]);response.end();});
  server.on('connect',(request,socket)=>{
    seen.push([request.method,request.url,request.headers.host,request.headers['proxy-authorization']]);
    if(!accept) {socket.end('HTTP/1.1 407 Proxy Authentication Required\r\n\r\n');return;}
    socket.write('HTTP/1.1 200 Connection Established\r\n\r\n');
    socket.once('data',chunk=>{hello=chunk;socket.destroy();});
  });
  const port=await listen(server);
  try {
    const secret='proxy-secret-value';
    const fetchThrough=proxyFetch(proxySettings(`http://user:${secret}@127.0.0.1:${port}`),LibraryError);
    // The target is a closed local port, so even a regression to a direct
    // connection could not leave this machine.
    const request=()=>new Request('https://localhost:8443/youtubei/v1/browse?prettyPrint=false',{method:'POST',body:'{"x":1}',
      headers:{'content-type':'application/json'}});
    const refused=await fetchThrough(request(),AbortSignal.timeout(5000)).catch(error=>error);
    assert.ok(refused instanceof LibraryError);assert.equal(refused.code,'proxy_failed');assert.equal(refused.status,502);
    assert.ok(!refused.message.includes(secret) && !refused.message.includes(String(port)));
    assert.deepEqual(seen,[['CONNECT','localhost:8443','localhost:8443','Basic '+Buffer.from(`user:${secret}`).toString('base64')]]);
    // Once the tunnel is open the client speaks TLS to the target inside it.
    accept=true;
    await assert.rejects(fetchThrough(request(),AbortSignal.timeout(5000)),{code:'proxy_failed'});
    assert.equal(seen.length,2);assert.equal(hello[0],0x16);assert.ok(hello.includes('localhost'));
    assert.ok(!hello.includes('"x":1'));
    // An unreachable proxy fails closed as well.
    const closed=proxyFetch(proxySettings('http://127.0.0.1:1'),LibraryError);
    await assert.rejects(closed(request(),AbortSignal.timeout(5000)),{code:'proxy_failed'});
  } finally {server.closeAllConnections?.();await new Promise(resolve=>server.close(resolve));}
});
