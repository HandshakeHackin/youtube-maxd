// Account-owned channel and notification lists as bounded catalog feeds.
// SPDX-License-Identifier: GPL-3.0-or-later
import {YTNodes} from 'youtubei.js';

const plain=value=>typeof value==='string'?value:value?.text ?? '';
function bounded(rows,ErrorType) {
  const list=Array.from(rows ?? []);
  if(list.length>4096)throw new ErrorType('upstream_limit','The account list exceeds the item limit.',502);
  return list;
}
export function channelsFeed(feed,channel,ErrorType) {
  const seen=new Set();
  const items=bounded(feed?.channels,ErrorType).map(channel).filter(item=>item && !seen.has(item.id) && seen.add(item.id));
  return {catalog_items:items,has_continuation:!!feed?.has_continuation,
    ...(typeof feed?.getContinuation==='function'?{getContinuation:async()=>
      channelsFeed(await feed.getContinuation(),channel,ErrorType)}:{})};
}
// Only notifications that open a video become rows; repeated videos remain
// separate occurrences. Menus, record-click endpoints and IDs stay private.
export function notificationsFeed(menu,video,ErrorType) {
  if(!Array.isArray(menu?.contents))
    throw new ErrorType('upstream_format','YouTube returned an unsupported notification list.',502);
  const items=bounded(menu.contents,ErrorType).map(node=>{
    const item=video({id:node?.endpoint?.payload?.videoId,title:plain(node?.short_message),
      author:{name:plain(node?.sent_time),id:''},
      thumbnails:node?.video_thumbnails?.length?node.video_thumbnails:node?.thumbnails ?? []});
    return item && {kind:'video',...item,unread:node.read!==true};
  }).filter(Boolean);
  const more=(menu.page?.actions_memo?.getType?.(YTNodes.ContinuationItem) ?? []).length>0 &&
    typeof menu.getContinuation==='function';
  return {catalog_items:items,has_continuation:more,
    ...(more?{getContinuation:async()=>notificationsFeed(await menu.getContinuation(),video,ErrorType)}:{})};
}
