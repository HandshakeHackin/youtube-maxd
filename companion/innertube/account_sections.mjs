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
    return item && {kind:'video',...item,...(typeof node.read==='boolean'?{unread:!node.read}:{})};
  }).filter(Boolean);
  const more=(menu.page?.actions_memo?.getType?.(YTNodes.ContinuationItem) ?? []).length>0 &&
    typeof menu.getContinuation==='function';
  return {catalog_items:items,has_continuation:more,
    ...(more?{getContinuation:async()=>notificationsFeed(await menu.getContinuation(),video,ErrorType)}:{})};
}

export async function notificationCount(client,ErrorType) {
  const response=await client.actions.execute('/notification/get_unseen_count');
  const data=response?.data;
  const values=[];
  if(Object.hasOwn(data ?? {},'unseenCount'))values.push(data.unseenCount);
  if(Array.isArray(data?.actions) && data.actions.length<=64)
    for(const action of data.actions)
      if(Object.hasOwn(action?.updateNotificationsUnseenCountAction ?? {},'unseenCount'))
        values.push(action.updateNotificationsUnseenCountAction.unseenCount);
  const counts=values.map(value=>typeof value==='string' && /^(0|[1-9][0-9]{0,9})$/.test(value)?Number(value):value);
  if(response?.success!==true || !Number.isInteger(response.status_code) || response.status_code<200 ||
     response.status_code>=300 || data?.error || !counts.length ||
     counts.some(value=>!Number.isInteger(value) || value<0 || value>2147483647 || value!==counts[0]))
    throw new ErrorType('upstream_format','YouTube did not return a valid notification count.',502);
  return {api_version:1,kind:'notification_count',unseen_count:counts[0]};
}
