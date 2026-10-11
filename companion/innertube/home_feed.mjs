// SmartTube VideoGroup.isWatchedSuggestion policy for the primary Home grid.
// SPDX-License-Identifier: GPL-3.0-or-later
import {YT} from 'youtubei.js';

function homeRows(feed) {
  if(feed?.contents?.contents!==undefined)return feed.contents.contents;
  // The pinned Feed.page_contents getter dereferences an absent first Tab.
  // Read the parsed response lists directly for tabless filter responses.
  const page=feed?.page;
  const updates=[...(page?.on_response_received_actions ?? []),...(page?.on_response_received_endpoints ?? []),
    ...(page?.on_response_received_commands ?? [])].filter(node=>
      ['AppendContinuationItemsAction','reloadContinuationItemsCommand'].includes(node?.type) && Array.isArray(node.contents));
  return updates.length===1?updates[0].contents:[];
}
export function watchPercent(node) {
  if(node?.is_watched===true)return 100;
  for(const overlay of node?.thumbnail_overlays ?? []) {
    const percent=overlay?.percent_duration_watched;
    if(overlay?.type==='ThumbnailOverlayResumePlayback' && typeof percent==='number' && Number.isFinite(percent) &&
       percent>=0 && percent<=100)return percent;
  }
  const image=node?.content_image?.primary_thumbnail ?? node?.content_image;
  for(const overlay of image?.overlays ?? []) {
    if(overlay?.type!=='ThumbnailBottomOverlayView' || overlay.progress_bar?.type!=='ThumbnailOverlayProgressBarView')continue;
    const percent=overlay.progress_bar.start_percent;
    if(typeof percent==='number' && Number.isFinite(percent) && percent>=0 && percent<=100)return percent;
  }
  return -1;
}
export function watchedSuggestion(node) {
  const percent=watchPercent(node);
  return percent>95 || percent===10;
}
export function filterHomeFeed(feed) {
  // Only direct RichItem videos belong to the primary Home group. Shelf items
  // remain eligible; a layout without this evidence is left intact.
  const primary=new Set(Array.from(homeRows(feed)).filter(node=>node?.type==='RichItem').map(node=>node.content));
  return {videos:Array.from(feed?.videos ?? []).filter(node=>!primary.has(node) || !watchedSuggestion(node)),
    playlists:feed?.playlists ?? [],has_continuation:!!feed?.has_continuation,
    ...(typeof feed?.getContinuation==='function'?{async getContinuation(){return filterHomeFeed(await feed.getContinuation());}}:{})};
}

// Home rows after the primary grid keep watched cards (SmartTube group position > 0).
export class HomeCatalog {
  constructor(ErrorType,normalizeVideo,normalizePlaylist,references) {
    this.ErrorType=ErrorType;this.video=normalizeVideo;this.playlist=normalizePlaylist;this.references=references;
  }
  unwrap(node) {return ['RichItem','RichSection'].includes(node?.type)?node.content:node;}
  shelf(node) {
    node=this.unwrap(node);
    if(node?.type==='RichShelf')return {title:node.title,rows:node.contents};
    if(node?.type==='Shelf' && ['HorizontalList','ExpandedShelfContents','VerticalList'].includes(node.content?.type))
      return {title:node.title,rows:node.content.items};
    return null;
  }
  check(rows,depth) {
    if(!Array.isArray(rows) || rows.length>4096 || depth>4)
      throw new this.ErrorType('upstream_limit','The Home section exceeds the browsing limit.',502);
  }
  collect(rows,depth=0,result=new Set()) {
    this.check(rows,depth);
    for(const value of rows) {
      const node=this.unwrap(value),shelf=this.shelf(node);
      result.add(node);
      if(shelf)this.collect(shelf.rows,depth+1,result);
    }
    return result;
  }
  folders(owner,rows,client,depth) {
    this.check(rows,depth);const folders=[],nested=new Set();
    for(const node of rows) {
      const shelf=this.shelf(node);
      if(!shelf)continue;
      const snapshot=Array.from(this.collect(shelf.rows,depth+1));
      for(const item of snapshot)nested.add(item);
      const title=String(shelf.title ?? '').replaceAll('\0','').slice(0,192);
      if(!title || !shelf.rows.length)continue;
      if(folders.length>=64)throw new this.ErrorType('upstream_limit','Too many Home sections.',502);
      const children=Array.from(shelf.rows);
      folders.push(this.references.register(owner,title,'Home section',()=>this.section(owner,children,client,depth+1)));
    }
    return {folders,nested};
  }
  root(owner,feed,client,withFilters=true) {
    const rows=homeRows(feed);
    const {folders,nested}=this.folders(owner,rows,client,0);
    if(withFilters && typeof feed.applyFilter==='function') {
      const chips=feed.filter_nodes?.primary_filters ?? [];
      if(!Array.isArray(chips) || chips.length>64)throw new this.ErrorType('upstream_limit','Too many Home filters.',502);
      for(const chip of chips) {
        if(!['ChipCloudChip','ChipView','ListItemView'].includes(chip?.type) || chip.is_disabled || chip.is_selected || chip.selected)continue;
        const title=String(chip.text ?? chip.title ?? '').replaceAll('\0','').slice(0,192);
        if(title)folders.push(this.references.register(owner,title,'Home filter',async()=>
          this.root(owner,await feed.applyFilter(chip),client)));
      }
      const secondary=feed.filter_nodes?.secondary_filters ?? [];
      if(!Array.isArray(secondary) || secondary.length>64)throw new this.ErrorType('upstream_limit','Too many secondary Home filters.',502);
      const selected=chips.filter(chip=>['ChipCloudChip','ChipView','ListItemView'].includes(chip?.type) &&
        !chip.is_disabled && (chip.is_selected || chip.selected));
      if(selected.length===1 && typeof feed.getFilteredFeed==='function')for(const chip of secondary) {
        if(chip?.type!=='ChipView' || chip.selected || typeof chip.tap_command?.call!=='function')continue;
        const title=String(chip.text ?? '').replaceAll('\0','').slice(0,192);
        if(title)folders.push(this.references.register(owner,title,'Home secondary filter',async()=>{
          const filtered=await feed.getFilteredFeed(selected[0],chip);
          return this.root(owner,new YT.HomeFeed(feed.actions,filtered.page,true),client);
        }));
      }
    }
    const filtered=filterHomeFeed(feed);
    const primary=rows.filter(node=>node?.type==='RichItem').map(node=>node.content),primarySet=new Set(primary);
    const available=new Set(filtered.videos);
    const ordered=[...primary.filter(node=>available.has(node)),...filtered.videos.filter(node=>!primarySet.has(node))];
    const items=[...folders,...ordered.filter(node=>!nested.has(node)).map(this.video).filter(Boolean),
      ...Array.from(feed.playlists ?? []).filter(node=>!nested.has(node)).map(this.playlist).filter(Boolean)];
    return {catalog_items:items,has_continuation:!!feed.has_continuation,
      ...(typeof feed.getContinuation==='function'?{getContinuation:async()=>this.root(owner,await feed.getContinuation(),client,false)}:{})};
  }
  section(owner,rows,client,depth) {
    const {folders}=this.folders(owner,rows,client,depth);
    const nodes=rows.map(node=>this.unwrap(node));
    const videos=nodes.filter(node=>['Video','GridVideo','CompactVideo','LockupView','PlaylistVideo'].includes(node?.type));
    const playlists=nodes.filter(node=>['Playlist','GridPlaylist'].includes(node?.type) ||
      (node?.type==='LockupView' && ['PLAYLIST','ALBUM','PODCAST','SHOW'].includes(node.content_type)));
    const continuations=nodes.filter(node=>node?.type==='ContinuationItem');
    if(continuations.length>1 || (continuations.length && typeof continuations[0].endpoint?.call!=='function'))
      throw new this.ErrorType('upstream_format','Unsupported Home section continuation.',502);
    return {catalog_items:[...folders,...videos.map(this.video).filter(Boolean),...playlists.map(this.playlist).filter(Boolean)],
      has_continuation:continuations.length===1,...(continuations.length?{getContinuation:async()=>{
        const response=await continuations[0].endpoint.call(client.actions,{parse:true});
        const updates=response?.on_response_received_actions ?? response?.on_response_received_endpoints;
        const matches=Array.isArray(updates)?updates.filter(node=>node?.type==='AppendContinuationItemsAction'):[];
        if(matches.length!==1)throw new this.ErrorType('upstream_format','Unsupported Home section response.',502);
        return this.section(owner,matches[0].contents,client,depth);
      }}:{})};
  }
}
