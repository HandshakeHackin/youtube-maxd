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
import {YTNodes} from 'youtubei.js';
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
