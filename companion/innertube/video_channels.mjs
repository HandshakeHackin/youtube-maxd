// Verify missing channel identities on the same client/profile as the window.
export async function verifiedVideoChannel(client,id,ErrorType) {
  const info=await client.getBasicInfo(id);
  if(info?.basic_info?.id!==id || typeof info.basic_info.channel_id!=='string' ||
     !/^UC[A-Za-z0-9_-]{22}$/.test(info.basic_info.channel_id))
    throw new ErrorType('channel_unverified','The video channel could not be verified.',502);
  return info.basic_info.channel_id;
}

export async function hydrateWindowChannels(client,window,ErrorType) {
  if(!window.video_ids)return window; // Proven unavailable-only window.
  const channels=window.video_channels ?? window.video_ids.map(()=>'');
  const missing=[...new Set(window.video_ids.filter((id,i)=>!channels[i]))];
  const found=new Map();let cursor=0,stopped=false;
  const results=await Promise.allSettled(Array.from({length:Math.min(4,missing.length)},async()=>{
    while(!stopped && cursor<missing.length) {
      const id=missing[cursor++];
      try {found.set(id,await verifiedVideoChannel(client,id,ErrorType));}
      catch(error) {stopped=true;throw error;}
    }
  }));
  const failed=results.find(result=>result.status==='rejected');if(failed)throw failed.reason;
  return {...window,video_channels:window.video_ids.map((id,i)=>channels[i] || found.get(id))};
}
