// Original bounded adapter for the primary WatchNext suggestions group.
// SPDX-License-Identifier: GPL-3.0-or-later
import {watchedSuggestion} from './home_feed.mjs';

export function suggestionFeed(info, ErrorType) {
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
    return suggestionFeed(next,ErrorType);
  }}:{})};
}
