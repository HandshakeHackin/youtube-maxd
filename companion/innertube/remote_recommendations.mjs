// Remote-only fallback discovery; native ownership decides whether to consume it.
// SPDX-License-Identifier: GPL-3.0-or-later
import {Parser} from 'youtubei.js';

export function remoteRecommendationGroup(data,input,owner,groups,normalize,ErrorType) {
  const raw=data?.contents?.twoColumnWatchNextResults?.secondaryResults?.secondaryResults?.results;
  if(raw===undefined)return {};
  if(!Array.isArray(raw) || raw.length>4095)
    throw new ErrorType('playback_next_unavailable','Unsupported remote recommendation group.',502);
  const parsed=Parser.parseArray(raw);
  // Follow the pinned VideoInfo primary watch-next group selection. Do not
  // descend into unrelated shelves or use next-button/ad endpoints.
  const rows=parsed.find(node=>node?.type==='ItemSection')?.contents ?? parsed;
  if(!Array.isArray(rows) || rows.length>4095)
    throw new ErrorType('playback_next_unavailable','Remote recommendation group is too large.',502);
  const items=rows.filter(node=>['Video','CompactVideo','GridVideo','LockupView'].includes(node?.type))
    .map(normalize).filter(item=>item && item.id!==input.id);
  if(!items.length)return {};
  // A synthetic current-video anchor allows the existing verified group-window
  // path to skip blocked channels and retain occurrence identities.
  const feed={has_continuation:false};
  const attached=groups.attach(owner,feed,[{id:input.id,is_folder:false},...items]);
  return {fallback_context:{group:attached.key,index:0},fallback_count:items.length};
}
