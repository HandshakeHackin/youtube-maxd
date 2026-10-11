// Complete, bounded description text from the pinned player metadata.
// SPDX-License-Identifier: GPL-3.0-or-later
const LIMIT=20000;
const line=(value,limit=192)=>typeof value==='string'?value.replaceAll('\0','').slice(0,limit):'';

export async function videoDescription(client,input,ErrorType) {
  const info=await client.getBasicInfo(input.id),details=info?.basic_info;
  if(details?.id!==input.id)
    throw new ErrorType('upstream_format','YouTube returned a description for a different video.',502);
  const full=line(details.short_description,Infinity);
  let description=full.slice(0,LIMIT);
  // Never end the bounded text in the middle of a surrogate pair.
  if(full.length>LIMIT && /[\uD800-\uDBFF]$/.test(description))description=description.slice(0,-1);
  const published=info.page?.[0]?.microformat?.publish_date;
  const channel=details.channel_id;
  return {api_version:1,kind:'description',id:input.id,title:line(details.title),channel:line(details.author),
    channel_id:typeof channel==='string' && /^UC[A-Za-z0-9_-]{22}$/.test(channel)?channel:'',
    published:typeof published==='string' && /^\d{4}-\d{2}-\d{2}/.test(published)?published.slice(0,10):'',
    views:Number.isSafeInteger(details.view_count) && details.view_count>=0?String(details.view_count):'',
    description,truncated:description.length<full.length};
}
