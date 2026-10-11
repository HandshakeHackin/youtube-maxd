// Original WatchNext autoplay endpoint adapter. SPDX-License-Identifier: GPL-3.0-or-later
const videoId=value=>typeof value==='string' && /^[A-Za-z0-9_-]{11}$/.test(value);
export async function playbackNext(client,input,ErrorType) {
  if(!videoId(input.id) || (input.playlist ?? '')!=='' || input.index!=null || (input.direction ?? 0)!==0)
    throw new ErrorType('invalid_request','A next-video request requires only the playing video.');
  const response=await client.actions.execute('/next',{videoId:input.id,racyCheckOk:true,contentCheckOk:true});
  if(!response?.success || !response.data?.contents?.twoColumnWatchNextResults)
    throw new ErrorType('playback_next_unavailable','YouTube did not return next-video information.',502);
  // Match the pinned YouTube.js VideoInfo.autoplay_video_endpoint policy. Never
  // substitute an arbitrary recommendation card, ad, or next-button endpoint.
  const sets=response.data.contents.twoColumnWatchNextResults.autoplay?.autoplay?.sets;
  const endpoint=Array.isArray(sets)?sets[0]?.autoplayVideo?.watchEndpoint:undefined;
  if(endpoint && !videoId(endpoint.videoId))
    throw new ErrorType('playback_next_unavailable','YouTube returned an invalid next-video endpoint.',502);
  return {api_version:1,video_id:input.id,next_video_id:endpoint && endpoint.videoId!==input.id?endpoint.videoId:null};
}
