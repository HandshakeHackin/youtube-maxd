// Original bounded adapter for the primary WatchNext suggestions group.
// SPDX-License-Identifier: GPL-3.0-or-later
import {watchedSuggestion} from './home_feed.mjs';
import {randomUUID} from 'node:crypto';

export class SuggestionFilters {
  constructor(ErrorType,normalize,clock=()=>Date.now()) {
    this.ErrorType=ErrorType;this.normalize=normalize;this.clock=clock;this.entries=new Map();
  }
  prune() {for(const [key,entry] of this.entries)if(entry.deadline<=this.clock())this.entries.delete(key);}
  root(owner,info,client) {
    const feed=suggestionFeed(info,this.ErrorType);
    const chips=info.related_chip_cloud?.chips ?? [];
    if(!Array.isArray(chips) || chips.length>64)
      throw new this.ErrorType('upstream_limit','Too many suggestion filters.',502);
    this.prune();const folders=[];
    for(const chip of chips) {
      if(chip?.type!=='ChipCloudChip' || chip.is_selected || typeof chip.endpoint?.call!=='function')continue;
      const title=String(chip.text ?? '').replaceAll('\0','').slice(0,192);
      if(!title)continue;
      while(this.entries.size>=256)this.entries.delete(this.entries.keys().next().value);
      const key=randomUUID();this.entries.set(key,{owner,chip,client,deadline:this.clock()+120000});
      folders.push({id:'library:filter:'+key,title,subtitle:'Related video filter',is_folder:true});
    }
    return {...feed,catalog_items:[...folders,...feed.videos.map(this.normalize).filter(Boolean)]};
  }
  async open(owner,key) {
    this.prune();const entry=this.entries.get(key);
    if(!entry || entry.owner!==owner)throw new this.ErrorType('cursor_expired','Reload Related videos to select this filter.',409);
    if(!entry.pending)entry.pending=(async()=>{
      const result=await entry.chip.endpoint.call(entry.client.actions,{parse:true});
      if(this.entries.get(key)!==entry || entry.deadline<=this.clock())
        throw new this.ErrorType('cursor_expired','Reload Related videos to select this filter.',409);
      return this.response(result,entry.client,'reloadContinuationItemsCommand');
    })();
    const pending=entry.pending;
    try {return await pending;} catch(error) {if(entry.pending===pending)entry.pending=null;throw error;}
  }
  response(response,client,type) {
    const updates=response?.on_response_received_endpoints;
    const candidates=Array.isArray(updates)?updates.filter(node=>node?.type===type &&
      (type==='reloadContinuationItemsCommand'?node.target_id==='watch-next-feed':!node.target || node.target==='watch-next-feed')):[];
    if(candidates.length!==1 || !Array.isArray(candidates[0].contents))
      throw new this.ErrorType('upstream_format','YouTube returned an unsupported filtered feed.',502);
    const rows=candidates[0].contents;
    const continuations=rows.filter(node=>node?.type==='ContinuationItem');
    if(continuations.length>1 || (continuations.length && typeof continuations[0].endpoint?.call!=='function'))
      throw new this.ErrorType('upstream_format','YouTube returned an unsupported filtered continuation.',502);
    const token=continuations[0];
    return suggestionFeed({watch_next_feed:rows,wn_has_continuation:!!token,
      getWatchNextContinuation:async()=>{
        const next=await token.endpoint.call(client.actions,{parse:true});
        // Return a feed directly below; do not reuse mutable VideoInfo state.
        return this.response(next,client,'AppendContinuationItemsAction');
      }},this.ErrorType,true);
  }
}

export function suggestionFeed(info, ErrorType, continuationIsFeed=false) {
  const rows=info?.watch_next_feed;
  if(!Array.isArray(rows) || rows.length>4096)
    throw new ErrorType('upstream_format','YouTube returned an unsupported suggestions layout.',502);
  // Keep only direct video cards. Shelves, ads and playlist cards have separate
  // group semantics and must not accidentally become automatic video targets.
  const videos=rows.filter(node=>['Video','CompactVideo','LockupView'].includes(node?.type) &&
    !node.is_ad && !node.is_promoted && (!node.content_type || node.content_type==='VIDEO') &&
    !watchedSuggestion(node));
  const more=info.wn_has_continuation===true;
  if(more && typeof info.getWatchNextContinuation!=='function')
    throw new ErrorType('upstream_format','YouTube returned an unsupported suggestions continuation.',502);
  return {videos,has_continuation:more,...(more?{async getContinuation(){
    const next=await info.getWatchNextContinuation();
    return continuationIsFeed?next:suggestionFeed(next,ErrorType);
  }}:{})};
}
