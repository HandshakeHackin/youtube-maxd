// Original WatchNext autoplay endpoint adapter. SPDX-License-Identifier: GPL-3.0-or-later
const videoId=value=>typeof value==='string' && /^[A-Za-z0-9_-]{11}$/.test(value);
const playlistId=value=>typeof value==='string' && /^(WL|LL|[A-Za-z0-9_-]{10,100})$/.test(value);
const index=value=>Number.isInteger(value) && value>=0 && value<=100000;
export async function playbackNext(client,input,ErrorType) {
  const playlist=input.playlist ?? '';
  if(input.require_channel!==undefined && typeof input.require_channel!=='boolean')
    throw new ErrorType('invalid_request','Channel metadata selection must be a boolean.');
  if(!videoId(input.id) || typeof playlist!=='string' || (playlist ? !playlistId(playlist) || !index(input.index) : input.index!=null) || (input.direction ?? 0)!==0)
    throw new ErrorType('invalid_request','A next-video request requires a playing video and optional playlist occurrence.');
  const response=await client.actions.execute('/next',{videoId:input.id,
    ...(playlist?{playlistId:playlist,playlistIndex:input.index}:{}),racyCheckOk:true,contentCheckOk:true});
  if(!response?.success || !response.data?.contents?.twoColumnWatchNextResults)
    throw new ErrorType('playback_next_unavailable','YouTube did not return next-video information.',502);
  // Match the pinned YouTube.js VideoInfo.autoplay_video_endpoint policy. Never
  // substitute an arbitrary recommendation card, ad, or next-button endpoint.
  const sets=response.data.contents.twoColumnWatchNextResults.autoplay?.autoplay?.sets;
  const endpoint=Array.isArray(sets)?sets[0]?.autoplayVideo?.watchEndpoint:undefined;
  if(endpoint && !videoId(endpoint.videoId))
    throw new ErrorType('playback_next_unavailable','YouTube returned an invalid next-video endpoint.',502);
  const nextIndex=endpoint?.index ?? endpoint?.playlistIndex;
  const context=playlistId(endpoint?.playlistId) && index(nextIndex) &&
    !(endpoint.index!==undefined && endpoint.playlistIndex!==undefined && endpoint.index!==endpoint.playlistIndex)
    ? {playlist:endpoint.playlistId,index:nextIndex}:null;
  const distinct=endpoint && (endpoint.videoId!==input.id || (context && (context.playlist!==playlist || context.index!==input.index)));
  let channel='';
  if(distinct && input.require_channel) {
    const info=await client.getBasicInfo(endpoint.videoId);
    if(info?.basic_info?.id!==endpoint.videoId || typeof info.basic_info.channel_id!=='string' ||
       !/^UC[A-Za-z0-9_-]{22}$/.test(info.basic_info.channel_id))
      throw new ErrorType('playback_next_unavailable','The next video channel could not be verified.',502);
    channel=info.basic_info.channel_id;
  }
  return {api_version:1,video_id:input.id,next_video_id:distinct?endpoint.videoId:null,
    ...(channel?{next_channel_id:channel}:{}),
    ...(distinct && context?{playback_context:context}:{})};
}
