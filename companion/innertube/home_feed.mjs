// SmartTube VideoGroup.isWatchedSuggestion policy for the primary Home grid.
// SPDX-License-Identifier: GPL-3.0-or-later
export function watchedSuggestion(node) {
  if(node?.is_watched===true)return true;
  for(const overlay of node?.thumbnail_overlays ?? []) {
    const percent=overlay?.percent_duration_watched;
    if(overlay?.type==='ThumbnailOverlayResumePlayback' && typeof percent==='number' && Number.isFinite(percent) &&
       percent>=0 && percent<=100)return percent>95 || percent===10;
  }
  return false;
}
export function filterHomeFeed(feed) {
  // Only direct RichItem videos belong to the primary Home group. Shelf items
  // remain eligible; a layout without this evidence is left intact.
  const primary=new Set(Array.from(feed?.contents?.contents ?? []).filter(node=>node?.type==='RichItem').map(node=>node.content));
  return {videos:Array.from(feed?.videos ?? []).filter(node=>!primary.has(node) || !watchedSuggestion(node)),
    playlists:feed?.playlists ?? [],has_continuation:!!feed?.has_continuation,
    ...(typeof feed?.getContinuation==='function'?{async getContinuation(){return filterHomeFeed(await feed.getContinuation());}}:{})};
}
