// Channel Videos navigation uses source positions, never synthesized playlist IDs.
import {YT} from 'youtubei.js';
import {hydrateWindowChannels} from './video_channels.mjs';

// Channel page sections the pinned Channel parser can open, in display order.
const tabs={home:['has_home','getHome'],videos:['has_videos','getVideos'],shorts:['has_shorts','getShorts'],
  live:['has_live_streams','getLiveStreams'],playlists:['has_playlists','getPlaylists']};
const orders=['newest','popular','oldest'];
const label=value=>String(value ?? '').replaceAll('\0','').slice(0,192);
// Order chips are recognized by position (newest, popular, oldest) because
// their labels are localized; any other chip layout offers no order.
function orderChips(feed) {
  let chips;
  try {chips=Array.from(feed?.filter_nodes?.primary_filters ?? []);} catch {return [];}
  const selected=chip=>chip.type==='ChipView'?chip.selected===true:chip.is_selected===true;
  if(chips.length<2 || chips.length>3 || chips.some(chip=>!['ChipView','ChipCloudChip'].includes(chip?.type) ||
     typeof chip.endpoint?.call!=='function') || !selected(chips[0]) || chips.slice(1).some(selected))return [];
  return chips;
}
export function mappedFeed(feed,video,playlist) {
  return {catalog_items:[...Array.from(feed?.videos ?? []).map(video),...Array.from(feed?.playlists ?? []).map(playlist)].filter(Boolean),
    has_continuation:!!feed?.has_continuation,
    ...(typeof feed?.getContinuation==='function'?{getContinuation:async()=>mappedFeed(await feed.getContinuation(),video,playlist)}:{})};
}
// The Home section becomes one folder per shelf; cards outside shelves follow.
export function channelHome(owner,feed,client,catalog,video) {
  const rows=[];
  for(const section of Array.from(feed?.current_tab?.content?.contents ?? []).slice(0,256))
    rows.push(...(section?.type==='ItemSection'?Array.from(section.contents ?? []):[section]));
  const {folders,nested}=catalog.folders(owner,rows,client,0);
  return {catalog_items:[...folders,...Array.from(feed?.videos ?? []).filter(node=>!nested.has(node)).map(video).filter(Boolean)],
    has_continuation:false};
}
// Opens one section (or an in-channel search) and reports what the channel offers.
export async function channelPage(client,input,ErrorType,query='',
    wrap=(actions,page)=>new YT.ChannelListContinuation(actions,page,true)) {
  const search=input.action==='channel_search',tab=input.tab || 'videos',order=input.channel_sort || 'newest';
  if(typeof input.channel!=='string' || !/^UC[A-Za-z0-9_-]{22}$/.test(input.channel) || input.id || input.playlist ||
     (search?!!(input.tab || input.channel_sort):!Object.hasOwn(tabs,tab) || !orders.includes(order)))
    throw new ErrorType('invalid_request','Select a channel and one of its sections.');
  const channel=await client.getChannel(input.channel);
  const available=Object.keys(tabs).filter(name=>channel[tabs[name][0]]===true);
  const info={id:input.channel,title:label(channel.metadata?.title),tabs:available,search:channel.has_search===true};
  if(search) {
    if(!info.search)throw new ErrorType('channel_search_unavailable','This channel cannot be searched.',404);
    return {feed:await channel.search(query),indexed:false,info:{...info,tab:'search',sort:'',sorts:[]}};
  }
  if(!available.includes(tab))throw new ErrorType('channel_tab_unavailable','This channel has no such section.',404);
  let feed=await channel[tabs[tab][1]]();
  const chips=['videos','shorts','live'].includes(tab)?orderChips(feed):[],sorts=orders.slice(0,chips.length);
  if(order!=='newest') {
    if(!sorts.includes(order))
      throw new ErrorType('channel_sort_unavailable','This channel section does not offer that order.',404);
    feed=wrap(client.actions,await chips[orders.indexOf(order)].endpoint.call(client.actions,{parse:true}));
  }
  // Only the default Videos order matches the positions channel_window walks.
  return {feed,indexed:tab==='videos' && order==='newest',info:{...info,tab,sort:sorts.length?order:'',sorts}};
}
export async function channelWindow(client,input,normalize,ErrorType) {
  if(input.require_channel!==undefined && typeof input.require_channel!=='boolean')
    throw new ErrorType('invalid_request','Channel verification must be a boolean.');
  if(input.require_name!==undefined && (typeof input.require_name!=='boolean' || (input.require_name && input.require_channel!==true)))
    throw new ErrorType('invalid_request','Channel-name verification requires channel verification.');
  const index=n=>Number.isInteger(n)&&n>=0&&n<=100000;
  const target=input.target ?? input.index, direction=input.direction ?? 0;
  if(typeof input.channel!=='string' || typeof input.id!=='string' ||
      !/^UC[A-Za-z0-9_-]{22}$/.test(input.channel) || !/^[A-Za-z0-9_-]{11}$/.test(input.id) ||
      !index(input.index) || !index(target) || ![-1,0,1].includes(direction) || input.playlist)
    throw new ErrorType('invalid_request','Channel navigation requires a channel and anchored video occurrence.');
  let feed=await (await client.getChannel(input.channel)).getVideos();
  let offset=0,anchor=false,selected=null,previous=[],following=[],ended=false,empty=0;
  const limit=Math.min(100000,Math.max(input.index,target+(direction>0?4095:0))+64);
  while(true) {
    const nodes=Array.from(feed?.videos ?? []);
    if(nodes.length>4096 || (!nodes.length && ++empty>4))
      throw new ErrorType('channel_window_unavailable','The channel returned an invalid video page.',502);
    for(const node of nodes) {
      const position=offset++;
      if(position>100000) break;
      const row=normalize(node);
      if(position===input.index) anchor=row?.id===input.id;
      if(!row || node.is_upcoming) continue;
      const entry={id:row.id,index:position};
      if(position<target) {
        previous.push(entry);if(previous.length>63)previous.shift();
        if(direction<0 && target-position<4096)selected=entry;
      } else if(position===target) selected=entry;
      else {
        if(direction>0 && !selected && position-target<4096)selected=entry;
        if(following.length<64)following.push(entry);
      }
    }
    if(!feed.has_continuation) {ended=true;break;}
    if(offset>limit || (anchor && selected && offset>Math.max(input.index,selected.index)+63))break;
    if(typeof feed.getContinuation!=='function')
      throw new ErrorType('channel_window_unavailable','The channel continuation is missing.',502);
    feed=await feed.getContinuation();
  }
  if(!anchor)throw new ErrorType('channel_changed','The selected channel position has changed. Reload the channel.',409);
  if(!selected && direction && ended && offset<=100001 && target<offset) {
    const skipped=direction>0?offset-target:target+1;
    if(skipped<=4096)return {api_version:1,playlist:'channel:'+input.channel,
      anchor_video_id:input.id,anchor_index:input.index,unavailable_only:true,
      requested_index:target,skipped_unavailable:skipped,playlist_size:offset,exhausted:true};
  }
  if(!selected)throw new ErrorType('channel_window_unavailable','No playable video was found at the requested channel position.',404);
  const rows=[...previous,...(selected.index===target?[selected]:[]),...following]
    .filter((row,i,all)=>all.findIndex(other=>other.index===row.index)===i);
  if(!rows.some(row=>row.index===selected.index))rows.push(selected);
  rows.sort((a,b)=>a.index-b.index);
  const response={api_version:1,playlist:'channel:'+input.channel,video_id:selected.id,index:selected.index,
    anchor_video_id:input.id,anchor_index:input.index,window_start:rows[0].index,
    video_ids:rows.map(row=>row.id),video_indices:rows.map(row=>row.index),video_channels:rows.map(()=>input.channel)};
  if(ended && offset<=100001)response.playlist_size=offset;
  if(selected.index!==target)Object.assign(response,{requested_index:target,skipped_unavailable:Math.abs(selected.index-target)});
  return input.require_name?hydrateWindowChannels(client,response,ErrorType,true):response;
}
