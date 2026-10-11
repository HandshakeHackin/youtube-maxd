import test from 'node:test';
import assert from 'node:assert/strict';
import {filterHomeFeed,watchedSuggestion} from './home_feed.mjs';
import {Library,normalizeVideo} from './library.mjs';
const id='abcdefghijk';
const video=(percent,extra={})=>({id,thumbnail_overlays:[{type:'ThumbnailOverlayResumePlayback',percent_duration_watched:percent}],...extra});
test('SmartTube watched-suggestion threshold preserves partially watched videos',()=>{
  for(const [percent,expected] of [[0,false],[9,false],[10,true],[11,false],[95,false],[95.1,true],[100,true],[-1,false],[101,false],[NaN,false],['100',false]])
    assert.equal(watchedSuggestion(video(percent)),expected,String(percent));
  assert.equal(watchedSuggestion(video(0,{is_watched:true})),true);
});
test('Home hides watched primary cards while preserving other shelves and unknown layouts',()=>{
  const main=video(100),partial=video(50),shelf=video(100);
  const feed={videos:[main,partial,shelf],contents:{contents:[{type:'RichItem',content:main},{type:'RichItem',content:partial},
    {type:'RichSection',content:{items:[shelf]}}]},has_continuation:false};
  assert.deepEqual(filterHomeFeed(feed).videos,[partial,shelf]);
  assert.deepEqual(filterHomeFeed({...feed,contents:{}}).videos,feed.videos);
});
test('Home continuation and retained group navigation apply the same watched policy',async()=>{
  const first=video(0),watched=video(100),next=video(50,{id:'bcdefghijkl'});
  const tail={videos:[watched,next],contents:{contents:[{type:'RichItem',content:watched},{type:'RichItem',content:next}]},has_continuation:false};
  const feed={videos:[first],contents:{contents:[{type:'RichItem',content:first}]},has_continuation:true,async getContinuation(){return tail;}};
  const library=new Library(async()=>({getHomeFeed:async()=>feed,getPlaylist:async()=>tail}));
  const home=await library.execute({action:'home'}),group=home.items[0].playback_context.group;
  const result=await library.execute({action:'group_window',group,id,index:0,target:1,direction:1});
  assert.equal(result.video_id,next.id);assert.equal(result.index,1);
  const page=await library.execute({action:'continue',cursor:home.cursor});assert.equal(page.items.length,1);
  const playlist=await library.execute({action:'playlist',id:'PLabcdefghijk'});assert.equal(playlist.items.length,2);
});
import {YTNodes,YT,Parser} from 'youtubei.js';
test('pinned RichItem and resume-overlay parser expose the primary Home watched state',()=>{
  const item=new YTNodes.RichItem({content:{videoRenderer:{videoId:id,title:{simpleText:'Watched'},thumbnail:{thumbnails:[]},
    thumbnailOverlays:[{thumbnailOverlayResumePlaybackRenderer:{percentDurationWatched:96}}]}}});
  assert.equal(item.type,'RichItem');assert.equal(watchedSuggestion(item.content),true);
  assert.deepEqual(filterHomeFeed({videos:[item.content],contents:{contents:[item]},has_continuation:false}).videos,[]);
});

test('catalog normalization preserves progress independently of Home filtering',()=>{
  assert.equal(normalizeVideo(video(65)).watch_percent,65);
  assert.equal(normalizeVideo(video(0,{is_watched:true})).watch_percent,100);
  assert.equal(normalizeVideo(video('65')).watch_percent,-1);
});

function modernProgress(percent) {
  return new YTNodes.RichItem({content:{lockupViewModel:{contentId:id,contentType:'LOCKUP_CONTENT_TYPE_VIDEO',
    contentImage:{thumbnailViewModel:{image:{sources:[]},overlays:[{thumbnailBottomOverlayViewModel:{
      progressBar:{thumbnailOverlayProgressBarViewModel:{startPercent:percent}},badges:[]
    }}]}}}}});
}
test('pinned modern Home cards retain progress and share watched filtering thresholds',()=>{
  for(const [percent,expected] of [[0,0],[10,10],[50.5,50.5],[96,96],[100,100],[-1,-1],[101,-1],['96',-1]]) {
    const item=modernProgress(percent),row=normalizeVideo(item.content);
    assert.equal(row.watch_percent,expected);
    const feed=filterHomeFeed({videos:[item.content],contents:{contents:[item]},has_continuation:false});
    assert.equal(feed.videos.length,expected===10||expected>95?0:1);
  }
});

test('catalog normalization retains only valid parsed upcoming dates',()=>{
  assert.equal(normalizeVideo({id,upcoming:new Date(2000),is_upcoming:true}).upcoming_start_ms,2000);
  for(const upcoming of [new Date(NaN),new Date(-1),'2000',2000])
    assert.equal(normalizeVideo({id,upcoming,is_upcoming:true}).upcoming_start_ms,undefined);
});

test('modern live badges survive actual parser normalization independently of localized text',async()=>{
  for(const kind of ['thumbnailBottomOverlayViewModel','thumbnailOverlayBadgeViewModel']) {
    const make=(style,label)=>new YTNodes.LockupView({contentId:id,contentType:'LOCKUP_CONTENT_TYPE_VIDEO',
      contentImage:{thumbnailViewModel:{image:{sources:[]},overlays:[{[kind]:{
        [kind==='thumbnailBottomOverlayViewModel'?'badges':'thumbnailBadges']:[{thumbnailBadgeViewModel:{
          text:label,badgeStyle:style}}]
      }}]}}});
    const live=make('THUMBNAIL_OVERLAY_BADGE_STYLE_LIVE','En directo');
    assert.equal(normalizeVideo(live).is_live,true);
    assert.equal(normalizeVideo(make('THUMBNAIL_OVERLAY_BADGE_STYLE_DEFAULT','LIVE')).is_live,false);
    const library=new Library(async()=>({getHomeFeed:async()=>({videos:[live],has_continuation:false})}));
    const page=await library.execute({action:'home'});
    assert.equal(page.items[0].is_live,true);assert.equal(page.items[0].playback_context.index,0);
  }
  assert.equal(normalizeVideo({id,is_live:true}).is_live,true);
  assert.equal(normalizeVideo({id}).is_live,false);
});

const homeVideo=(key,percent)=>({richItemRenderer:{content:{videoRenderer:{videoId:key,title:{simpleText:key},thumbnail:{thumbnails:[]},
  thumbnailOverlays:[{thumbnailOverlayResumePlaybackRenderer:{percentDurationWatched:percent}}]}}}});
function homeSection(items) {
  return new YTNodes.RichSection({content:{richShelfRenderer:{title:{simpleText:'Keep watching'},contents:items}}});
}
test('Home shelves retain watched videos without duplicating them in the primary grid',async()=>{
  const primary=new YTNodes.RichItem(homeVideo(id,100).richItemRenderer);
  const section=homeSection([homeVideo('bcdefghijkl',100),homeVideo('bcdefghijkl',100)]);
  const videos=[primary.content,...section.content.contents.map(row=>row.content)];
  const feed={videos,contents:{contents:[primary,section]},has_continuation:false};
  const library=new Library(async()=>({getHomeFeed:async()=>feed}),[{id:'account',cookie:''}]);
  const root=await library.execute({action:'home'});assert.equal(root.items.length,1);assert(root.items[0].is_folder);
  const key=root.items[0].id.slice(15);
  await assert.rejects(library.execute({action:'suggestions_filter',profile:'account',id:key}),{code:'cursor_expired'});
  const page=await library.execute({action:'suggestions_filter',id:key});
  assert.deepEqual(page.items.map(row=>row.watch_percent),[100,100]);
  assert.deepEqual(page.items.map(row=>row.playback_context.index),[0,1]);
});

test('Home filters preserve profile ownership and return independent groups without changing the original feed',async()=>{
  const chip=new YTNodes.ChipCloudChip({text:{simpleText:'Music'},isSelected:false,navigationEndpoint:{browseEndpoint:{browseId:'FEwhat_to_watch'}}});
  let calls=0;
  const source=video(0),filtered=video(45,{id:'bcdefghijkl'});
  const target={videos:[filtered],contents:{contents:[{type:'RichItem',content:filtered}]},has_continuation:false};
  const feed={videos:[source],contents:{contents:[{type:'RichItem',content:source}]},filter_nodes:{primary_filters:[chip]},
    has_continuation:false,async applyFilter(selected){assert.equal(selected,chip);calls++;return target;}};
  const library=new Library(async profile=>{assert.equal(profile.id,'account');return {getHomeFeed:async()=>feed};},[{id:'account',cookie:'synthetic'}]);
  const root=await library.execute({action:'home',profile:'account'}),key=root.items[0].id.slice(15);
  const request={action:'suggestions_filter',profile:'account',id:key};
  const [a,b]=await Promise.all([library.execute(request),library.execute(request)]);
  assert.equal(calls,1);assert.deepEqual(a,b);assert.equal(a.items[0].id,filtered.id);
  assert.notEqual(a.items[0].playback_context.group,root.items[1].playback_context.group);
  assert.equal(feed.videos[0],source);
});

test('Home continuation shelves remain separate and preserve their own next-page occurrences',async()=>{
  let calls=0;
  const section=homeSection([homeVideo('bcdefghijkl',100),{continuationItemRenderer:{continuationEndpoint:{continuationCommand:{token:'home-shelf-next'}}}}]);
  section.content.contents.at(-1).endpoint.call=async()=>{
    calls++;return {on_response_received_actions:[new YTNodes.AppendContinuationItemsAction({continuationItems:[homeVideo('bcdefghijkl',100)]})]};
  };
  const source=video(0),tail={videos:[section.content.contents[0].content],contents:{contents:[section]},has_continuation:false};
  const feed={videos:[source],contents:{contents:[{type:'RichItem',content:source}]},has_continuation:true,async getContinuation(){return tail;}};
  const library=new Library(async()=>({getHomeFeed:async()=>feed}));
  const root=await library.execute({action:'home'});
  const page=await library.execute({action:'continue',cursor:root.cursor});assert(page.items[0].is_folder);
  const child=await library.execute({action:'suggestions_filter',id:page.items[0].id.slice(15)});
  const [window,nextPage]=await Promise.all([
    library.execute({action:'group_window',group:child.items[0].playback_context.group,id:'bcdefghijkl',index:0,target:1,direction:1}),
    library.execute({action:'continue',cursor:child.cursor})]);
  assert.equal(calls,1);assert.equal(window.index,1);assert.equal(nextPage.items[0].watch_percent,100);
  await assert.rejects(library.execute({action:'group_window',group:root.items[0].playback_context.group,id,index:0,target:1,direction:1}),{code:'group_unavailable'});
});

test('Home source ordering takes precedence over the parser memo type order',async()=>{
  const first=video(0),second=video(40,{id:'bcdefghijkl'});
  const feed={videos:[second,first],contents:{contents:[{type:'RichItem',content:first},{type:'RichItem',content:second},{type:'RichItem',content:first}]},has_continuation:false};
  const library=new Library(async()=>({getHomeFeed:async()=>feed}));
  const root=await library.execute({action:'home'});
  assert.deepEqual(root.items.map(row=>row.id),[id,second.id,id]);
  assert.deepEqual(root.items.map(row=>row.playback_context.index),[0,1,2]);
});

test('actual pinned HomeFeed filter responses retain primary watched filtering',async()=>{
  const feed=new YT.HomeFeed({}, {data:{contents:{richGridRenderer:{contents:[homeVideo(id,0)],header:{
    feedFilterChipBarRenderer:{contents:[{chipCloudChipRenderer:{text:{simpleText:'Music'},isSelected:false,
      navigationEndpoint:{browseEndpoint:{browseId:'FEwhat_to_watch'}}}}]}
  }}}}});
  const chip=feed.filter_nodes.primary_filters[0];
  chip.endpoint.call=async()=>Parser.parseResponse({onResponseReceivedActions:[{appendContinuationItemsAction:{
    continuationItems:[homeVideo(id,100),homeVideo('bcdefghijkl',45)]
  }}]});
  const library=new Library(async()=>({getHomeFeed:async()=>feed}));
  const root=await library.execute({action:'home'});
  const filtered=await library.execute({action:'suggestions_filter',id:root.items[0].id.slice(15)});
  assert.deepEqual(filtered.items.map(row=>row.id),['bcdefghijkl']);
  assert.equal(filtered.items[0].watch_percent,45);assert.equal(root.items[1].id,id);
});
