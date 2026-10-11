// SmartTube "Hide unwanted content" switches as request flags on list actions.
// Classification uses structured parser evidence only, never localized text.
// SPDX-License-Identifier: GPL-3.0-or-later
import {watchedSuggestion} from './home_feed.mjs';

export const hideFlags=['shorts_all','mixes','watched_watch_later','watched_home','watched_subscriptions',
  'watched_notifications','shorts_subscriptions','shorts_search','shorts_home','shorts_channel','shorts_history',
  'shorts_trending','streams_subscriptions','upcoming_subscriptions','upcoming_home','upcoming_channel'];
const sections={home:'home',search:'search',subscriptions:'subscriptions',history:'history',watch_later:'watch_later',
  notifications:'notifications',channel:'channel',channel_search:'channel'};

// Absent flags keep the behaviour of earlier clients; an explicit list (even an
// empty one) is the complete selection.
export function hideSet(value,ErrorType) {
  if(value===undefined || value===null)return null;
  if(!Array.isArray(value) || value.length>hideFlags.length || new Set(value).size!==value.length ||
     value.some(flag=>typeof flag!=='string' || !hideFlags.includes(flag)))
    throw new ErrorType('invalid_filter','Select supported content filters.');
  return new Set(value);
}
export function hideSection(input) {
  if(input?.action==='topics' || input?.action==='topic_open')return input.section==='trending'?'trending':'';
  if(input?.action==='catalog')return ['search','channel'].includes(input.kind)?input.kind:'';
  return sections[input?.action] ?? '';
}
const shortEndpoint=endpoint=>endpoint?.name==='reelWatchEndpoint' ||
  (typeof endpoint?.metadata?.url==='string' && endpoint.metadata.url.startsWith('/shorts/'));
export function isShort(node) {
  if(!node || typeof node!=='object')return false;
  if(node.is_short===true || ['ShortsLockupView','ReelItem'].includes(node.type) || node.content_type==='SHORT')return true;
  if(shortEndpoint(node.endpoint) || shortEndpoint(node.on_tap_endpoint) ||
     shortEndpoint(node.renderer_context?.command_context?.on_tap))return true;
  return Array.from(node.thumbnail_overlays ?? []).some(overlay=>
    overlay?.type==='ThumbnailOverlayTimeStatus' && overlay.style==='SHORTS');
}
// Shorts shelf tiles carry their identity on the tap endpoint.
export function shortsCard(node,author) {
  if(node?.type!=='ShortsLockupView')return node?.type==='ReelItem'?{id:node.id,title:node.title,thumbnails:node.thumbnails,author,is_short:true}:node;
  return {id:node.on_tap_endpoint?.payload?.videoId,title:node.overlay_metadata?.primary_text,
    thumbnails:node.thumbnail,author,is_short:true};
}
export const isMix=item=>item?.is_folder===true && typeof item.id==='string' && /^library:playlist:RD[A-Za-z0-9_-]+$/.test(item.id);
export const isWatched=item=>watchedSuggestion({is_watched:item?.played===true,
  thumbnail_overlays:[{type:'ThumbnailOverlayResumePlayback',percent_duration_watched:item?.watch_percent}]});
// Returns null when nothing can be hidden, otherwise a predicate over
// normalized rows. Watched Home cards are handled by the Home grid itself.
export function hides(flags,section) {
  if(!flags?.size)return null;
  const on=name=>flags.has(name);
  return item=>{
    if(!item || typeof item!=='object')return false;
    if(item.is_folder)return on('mixes') && isMix(item);
    if(item.is_short && (on('shorts_all') || on('shorts_'+section)))return true;
    if(item.is_upcoming && on('upcoming_'+section))return true;
    if(item.is_live && on('streams_'+section))return true;
    return section!=='home' && on('watched_'+section) && isWatched(item);
  };
}
