// Validated search options, mixed result cards and bounded query completions.
// SPDX-License-Identifier: GPL-3.0-or-later

// Only values the pinned YouTube.js 18.1.0 SearchFilters encoder accepts.
const dates={any:'',today:'today',week:'week',month:'month',year:'year'};
const durations={any:'',short:'under_three_mins',medium:'three_to_twenty_mins',long:'over_twenty_mins'};
const types={all:'',video:'video',channel:'channel',playlist:'playlist',movie:'movie',shorts:'shorts'};
const orders={relevance:'',view_count:'popularity'};
const features=new Set(['hd','subtitles','creative_commons','3d','live','purchased','4k','360','location','hdr','vr180']);
const pick=(value,table)=>typeof value==='string' && Object.hasOwn(table,value);
const label=(value,limit=192)=>String((typeof value==='string'?value:value?.text) ?? '').replaceAll('\0','').slice(0,limit);

export function searchQuery(value,ErrorType,allowEmpty=false) {
  if(typeof value!=='string' || value.length>200 || /[\0-\x1f\x7f]/.test(value) || (!allowEmpty && !value.trim()))
    throw new ErrorType('invalid_search','Enter search text of at most 200 characters.');
  return value.trim();
}
export function searchFilters(input,ErrorType) {
  const date=input.upload_date ?? 'any',duration=input.duration ?? 'any',type=input.type ?? 'all';
  const order=input.order ?? 'relevance',wanted=input.features ?? [];
  if(!pick(date,dates) || !pick(duration,durations) || !pick(type,types) || !pick(order,orders) ||
     !Array.isArray(wanted) || wanted.length>features.size || new Set(wanted).size!==wanted.length ||
     wanted.some(value=>typeof value!=='string' || !features.has(value)))
    throw new ErrorType('invalid_search','Select supported search options.');
  return {...(dates[date]?{upload_date:dates[date]}:{}),...(types[type]?{type:types[type]}:{}),
    ...(durations[duration]?{duration:durations[duration]}:{}),...(orders[order]?{prioritize:orders[order]}:{}),
    ...(wanted.length?{features:[...wanted].sort()}:{})};
}
export function normalizeChannel(node,imageUrl) {
  const id=node?.id ?? node?.author?.id;
  if(typeof id!=='string' || !/^UC[A-Za-z0-9_-]{22}$/.test(id))return null;
  // The pinned Channel parser notes that the legacy subscriber field now
  // carries the handle while the video-count field carries subscribers.
  const legacy=label(node.subscriber_count,64);
  const subscribers=node.subscribers!==undefined?label(node.subscribers,64):
    legacy.startsWith('@') || !legacy?label(node.video_count,64):legacy;
  let art=String(node.author?.thumbnails?.[0]?.url ?? '');
  if(art.startsWith('//'))art='https:'+art;
  return {kind:'channel',id:`channel:${id}`,channel_id:id,title:label(node.author?.name ?? node.title),
    subtitle:subscribers,overview:label(node.description_snippet,320),art_url:imageUrl(art),is_folder:true};
}
// Upstream order is retained across videos, channels and playlists. Every
// page of a query reuses the same parsed feed, so options survive paging.
export function searchFeed(feed,video,playlist,channel,ErrorType) {
  const source=feed?.results ?? [...(feed?.videos ?? []),...(feed?.channels ?? []),...(feed?.playlists ?? [])];
  const rows=[];
  const visit=(nodes,depth)=>{
    const list=Array.from(nodes ?? []);
    if(list.length>4096 || rows.length+list.length>4096)
      throw new ErrorType('upstream_limit','The search page exceeds the item limit.',502);
    for(const node of list) {
      if(!node || node.is_ad || node.is_promoted)continue;
      if(node.type==='Shelf') {if(!depth)visit(node.content?.items,1);continue;}
      if(['Channel','GridChannel'].includes(node.type)) {rows.push(channel(node));continue;}
      if(['Playlist','GridPlaylist','GridShow'].includes(node.type) ||
         (node.type==='LockupView' && ['PLAYLIST','ALBUM','PODCAST','SHOW'].includes(node.content_type))) {
        const item=playlist(node);rows.push(item && {kind:'playlist',...item});continue;
      }
      const item=video(node);rows.push(item && {kind:'video',...item});
    }
  };
  visit(source,0);
  const seen=new Set();
  const items=rows.filter(item=>item && !seen.has(item.id) && seen.add(item.id));
  return {catalog_items:items,has_continuation:!!feed?.has_continuation,
    ...(typeof feed?.getContinuation==='function'?{getContinuation:async()=>
      searchFeed(await feed.getContinuation(),video,playlist,channel,ErrorType)}:{})};
}
export async function searchSuggestions(client,input,ErrorType) {
  const query=searchQuery(input.query ?? '',ErrorType,true);
  let values;
  try {values=await client.getSearchSuggestions(query);}
  catch(error) {
    if(error instanceof ErrorType)throw error;
    throw new ErrorType('upstream_format','YouTube did not return search suggestions.',502);
  }
  if(!Array.isArray(values) || values.length>1024)
    throw new ErrorType('upstream_format','YouTube did not return search suggestions.',502);
  const seen=new Set(),suggestions=[];
  for(const value of values) {
    if(typeof value!=='string')continue;
    const entry=value.replace(/[\0-\x1f\x7f]+/g,' ').trim().slice(0,200);
    if(!entry || seen.has(entry))continue;
    seen.add(entry);suggestions.push(entry);
    if(suggestions.length>=15)break;
  }
  return {api_version:1,kind:'search_suggestions',query,suggestions};
}
