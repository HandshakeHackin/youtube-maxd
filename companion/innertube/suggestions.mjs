// Original bounded adapter for the primary WatchNext suggestions group.
// SPDX-License-Identifier: GPL-3.0-or-later
import {watchedSuggestion} from './home_feed.mjs';
import {randomUUID} from 'node:crypto';

export class SuggestionFilters {
  constructor(ErrorType,normalize,clock=()=>Date.now()) {
    this.ErrorType=ErrorType;this.normalize=normalize;this.clock=clock;this.entries=new Map();
  }
  prune() {for(const [key,entry] of this.entries)if(entry.deadline<=this.clock())this.entries.delete(key);}
  register(owner,title,subtitle,load) {
    this.prune();while(this.entries.size>=256)this.entries.delete(this.entries.keys().next().value);
    const key=randomUUID();this.entries.set(key,{owner,load,deadline:this.clock()+120000});
    return {id:'library:filter:'+key,title,subtitle,is_folder:true};
  }
  decorate(owner,feed,client,depth=0) {
    const folders=[];
    for(const shelf of feed.source_rows ?? []) {
      if(shelf?.type!=='Shelf' || shelf.is_ad || shelf.is_promoted ||
         !['HorizontalList','ExpandedShelfContents','VerticalList'].includes(shelf.content?.type))continue;
      const title=String(shelf.title ?? '').replaceAll('\0','').slice(0,192);
      const rows=shelf.content.items;
      if(!title || !Array.isArray(rows) || !rows.length)continue;
      if(depth>=4 || rows.length>4096 || folders.length>=64)
        throw new this.ErrorType('upstream_limit','The suggestion sections exceed the browsing limit.',502);
      // Snapshot the shelf independently of later mutation of the source feed.
      const snapshot=Array.from(rows);
      folders.push(this.register(owner,title,'Related video section',()=>this.rowsFeed(owner,snapshot,client,depth+1)));
    }
    return {...feed,catalog_items:[...folders,...feed.videos.map(this.normalize).filter(Boolean)],
      ...(feed.has_continuation?{getContinuation:async()=>this.decorate(owner,await feed.getContinuation(),client,depth)}:{})};
  }
  root(owner,info,client) {
    const feed=this.decorate(owner,suggestionFeed(info,this.ErrorType),client);
    const chips=info.related_chip_cloud?.chips ?? [];
    if(!Array.isArray(chips) || chips.length>64)
      throw new this.ErrorType('upstream_limit','Too many suggestion filters.',502);
    this.prune();const folders=[];
    for(const chip of chips) {
      if(chip?.type!=='ChipCloudChip' || chip.is_selected || typeof chip.endpoint?.call!=='function')continue;
      const title=String(chip.text ?? '').replaceAll('\0','').slice(0,192);
      if(!title)continue;
      folders.push(this.register(owner,title,'Related video filter',async()=>
        this.response(await chip.endpoint.call(client.actions,{parse:true}),client,'reloadContinuationItemsCommand',owner)));
    }
    return {...feed,catalog_items:[...folders,...feed.catalog_items]};
  }
  async open(owner,key) {
    this.prune();const entry=this.entries.get(key);
    if(!entry || entry.owner!==owner)throw new this.ErrorType('cursor_expired','Reload Related videos to select this filter.',409);
    if(!entry.pending)entry.pending=(async()=>{
      const result=await entry.load();
      if(this.entries.get(key)!==entry || entry.deadline<=this.clock())
        throw new this.ErrorType('cursor_expired','Reload Related videos to select this filter.',409);
      return result;
    })();
    const pending=entry.pending;
    try {return await pending;} catch(error) {if(entry.pending===pending)entry.pending=null;throw error;}
  }
  response(response,client,type,owner,depth=0) {
    const updates=response?.on_response_received_endpoints;
    const candidates=Array.isArray(updates)?updates.filter(node=>node?.type===type &&
      (type==='reloadContinuationItemsCommand'?node.target_id==='watch-next-feed':!node.target || node.target==='watch-next-feed')):[];
    if(candidates.length!==1 || !Array.isArray(candidates[0].contents))
      throw new this.ErrorType('upstream_format','YouTube returned an unsupported filtered feed.',502);
    return this.rowsFeed(owner,candidates[0].contents,client,depth);
  }
  rowsFeed(owner,rows,client,depth=0) {
    const continuations=rows.filter(node=>node?.type==='ContinuationItem');
    if(continuations.length>1 || (continuations.length && typeof continuations[0].endpoint?.call!=='function'))
      throw new this.ErrorType('upstream_format','YouTube returned an unsupported filtered continuation.',502);
    const token=continuations[0];
    const feed=suggestionFeed({watch_next_feed:rows,wn_has_continuation:!!token,
      getWatchNextContinuation:async()=>{
        const next=await token.endpoint.call(client.actions,{parse:true});
        // Return a feed directly below; do not reuse mutable VideoInfo state.
        return this.response(next,client,'AppendContinuationItemsAction',owner,depth);
      }},this.ErrorType,true);
    // The continuation above already returns a decorated page.
    const decorated=this.decorate(owner,{...feed,has_continuation:false},client,depth);
    return {...decorated,has_continuation:feed.has_continuation,
      ...(feed.has_continuation?{getContinuation:feed.getContinuation}:{})};
  }
}

export function suggestionFeed(info, ErrorType, continuationIsFeed=false) {
  const rows=info?.watch_next_feed;
  if(!Array.isArray(rows) || rows.length>4096)
    throw new ErrorType('upstream_format','YouTube returned an unsupported suggestions layout.',502);
  // Keep only direct video cards. Shelves, ads and playlist cards have separate
  // group semantics and must not accidentally become automatic video targets.
  const videos=rows.filter(node=>['Video','CompactVideo','GridVideo','LockupView'].includes(node?.type) &&
    !node.is_ad && !node.is_promoted && (!node.content_type || node.content_type==='VIDEO') &&
    !watchedSuggestion(node));
  const more=info.wn_has_continuation===true;
  if(more && typeof info.getWatchNextContinuation!=='function')
    throw new ErrorType('upstream_format','YouTube returned an unsupported suggestions continuation.',502);
  return {source_rows:Array.from(rows),videos,has_continuation:more,...(more?{async getContinuation(){
    const next=await info.getWatchNextContinuation();
    return continuationIsFeed?next:suggestionFeed(next,ErrorType);
  }}:{})};
}
