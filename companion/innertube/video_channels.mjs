// Verify missing channel identities on the same client/profile as the window.
export const validChannelName=value=>typeof value==='string' && !!value.trim() &&
  Buffer.byteLength(value,'utf8')<=255 && !/[\u0000-\u001f\u007f]/.test(value);
export async function verifiedVideoChannel(client,id,ErrorType,requireName=false) {
  const info=await client.getBasicInfo(id);
  if(info?.basic_info?.id!==id || typeof info.basic_info.channel_id!=='string' ||
     !/^UC[A-Za-z0-9_-]{22}$/.test(info.basic_info.channel_id))
    throw new ErrorType('channel_unverified','The video channel could not be verified.',502);
  if(requireName && !validChannelName(info.basic_info.author))
    throw new ErrorType('channel_unverified','The video channel name could not be verified.',502);
  return {channel:info.basic_info.channel_id,name:validChannelName(info.basic_info.author)?info.basic_info.author:''};
}

export async function hydrateWindowChannels(client,window,ErrorType,requireName=false) {
  if(!window.video_ids)return window; // Proven unavailable-only window.
  const channels=window.video_channels ?? window.video_ids.map(()=>'');
  const names=window.video_channel_names ?? window.video_ids.map(()=>'');
  const missing=[...new Set(window.video_ids.filter((id,i)=>!channels[i] || (requireName && !validChannelName(names[i]))))];
  const found=new Map();let cursor=0,stopped=false;
  const results=await Promise.allSettled(Array.from({length:Math.min(4,missing.length)},async()=>{
    while(!stopped && cursor<missing.length) {
      const id=missing[cursor++];
      try {found.set(id,await verifiedVideoChannel(client,id,ErrorType,requireName));}
      catch(error) {stopped=true;throw error;}
    }
  }));
  const failed=results.find(result=>result.status==='rejected');if(failed)throw failed.reason;
  for(let i=0;i<window.video_ids.length;++i) {
    const entry=found.get(window.video_ids[i]);
    if(entry && channels[i] && entry.channel!==channels[i])
      throw new ErrorType('channel_unverified','The video channel metadata changed.',502);
  }
  return {...window,video_channels:window.video_ids.map((id,i)=>channels[i] || found.get(id).channel),
    ...(requireName?{video_channel_names:window.video_ids.map((id,i)=>validChannelName(names[i])?names[i]:found.get(id)?.name)}:{})};
}
