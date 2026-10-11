// Channel Videos navigation uses source positions, never synthesized playlist IDs.
export async function channelWindow(client,input,normalize,ErrorType) {
  const index=n=>Number.isInteger(n)&&n>=0&&n<=100000;
  const target=input.target ?? input.index, direction=input.direction ?? 0;
  if(!/^UC[A-Za-z0-9_-]{22}$/.test(input.channel ?? '') || !/^[A-Za-z0-9_-]{11}$/.test(input.id ?? '') ||
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
  if(!selected)throw new ErrorType('channel_window_unavailable','No playable video was found at the requested channel position.',404);
  const rows=[...previous,...(selected.index===target?[selected]:[]),...following]
    .filter((row,i,all)=>all.findIndex(other=>other.index===row.index)===i);
  if(!rows.some(row=>row.index===selected.index))rows.push(selected);
  rows.sort((a,b)=>a.index-b.index);
  const response={api_version:1,playlist:'channel:'+input.channel,video_id:selected.id,index:selected.index,
    anchor_video_id:input.id,anchor_index:input.index,window_start:rows[0].index,
    video_ids:rows.map(row=>row.id),video_indices:rows.map(row=>row.index)};
  if(ended && offset<=100001)response.playlist_size=offset;
  if(selected.index!==target)Object.assign(response,{requested_index:target,skipped_unavailable:Math.abs(selected.index-target)});
  return response;
}
