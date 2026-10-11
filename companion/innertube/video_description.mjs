// Complete, bounded description text from the pinned player metadata, and the
// details-screen summary built on the same fields.
// SPDX-License-Identifier: GPL-3.0-or-later
const LIMIT=20000,EXCERPT=600;
const line=(value,limit=192)=>typeof value==='string'?value.replaceAll('\0','').slice(0,limit):'';
const count=value=>Number.isSafeInteger(value) && value>=0?String(value):'';
// Never end the bounded text in the middle of a surrogate pair.
function bounded(full,limit) {
  let cut=full.slice(0,limit);
  if(full.length>limit && /[\uD800-\uDBFF]$/.test(cut))cut=cut.slice(0,-1);
  return cut;
}
function describe(info,id,ErrorType,limit=LIMIT) {
  const details=info?.basic_info;
  if(details?.id!==id)
    throw new ErrorType('upstream_format','YouTube returned a description for a different video.',502);
  const full=line(details.short_description,Infinity),description=bounded(full,limit);
  const published=info.page?.[0]?.microformat?.publish_date;
  const channel=details.channel_id;
  return {id,title:line(details.title),channel:line(details.author),
    channel_id:typeof channel==='string' && /^UC[A-Za-z0-9_-]{22}$/.test(channel)?channel:'',
    published:typeof published==='string' && /^\d{4}-\d{2}-\d{2}/.test(published)?published.slice(0,10):'',
    views:count(details.view_count),like_count:count(details.like_count),
    description,truncated:description.length<full.length};
}
export async function videoDescription(client,input,ErrorType) {
  return {api_version:1,kind:'description',...describe(await client.getBasicInfo(input.id),input.id,ErrorType)};
}
// One watch-page request: player metadata plus the owner row, like count and
// related list of the pinned VideoInfo.
export async function videoDetails(client,input,ErrorType,imageUrl) {
  const info=await client.getInfo(input.id),base=describe(info,input.id,ErrorType,EXCERPT),details=info.basic_info;
  const owner=info.secondary_info?.owner;
  const formats=Array.from(info.streaming_data?.adaptive_formats ?? []).slice(0,512);
  const badges=[];
  if(details.is_live===true)badges.push('live');
  if(details.is_upcoming===true)badges.push('upcoming');
  if(formats.some(format=>Number(format?.height)>=2160 || Number(format?.width)>=3840))badges.push('4k');
  const related=Array.from(info.watch_next_feed ?? []).slice(0,512).some(node=>
    /^[A-Za-z0-9_-]{11}$/.test(node?.video_id ?? node?.content_id ?? ''));
  const duration=Number(details.duration);
  return {api_version:1,kind:'video_details',id:base.id,title:base.title,
    channel:{id:base.channel_id,name:base.channel,subscribers:line(owner?.subscriber_count?.toString?.(),64),
      art_url:imageUrl(owner?.author?.thumbnails?.[0]?.url)},
    published:base.published,views:base.views,like_count:base.like_count,
    description:base.description,description_truncated:base.truncated,
    duration_sec:Number.isFinite(duration) && duration>=0 && duration<=1000000?Math.floor(duration):0,
    badges,has_related:related};
}
