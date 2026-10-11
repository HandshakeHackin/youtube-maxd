// New-content marker of subscribed channels. The pinned GuideEntry parser
// drops the entry's presentation style, so the marker is read from the raw
// guide response. Unknown layouts yield no marker at all.
// SPDX-License-Identifier: GPL-3.0-or-later
import {collect} from './intents.mjs';

const styles=new Map([['GUIDE_ENTRY_PRESENTATION_STYLE_NEW_CONTENT',true],['GUIDE_ENTRY_PRESENTATION_STYLE_NONE',false]]);
const channelId=value=>typeof value==='string' && /^UC[A-Za-z0-9_-]{22}$/.test(value);

export function newContent(raw) {
  const marks=new Map(),conflicts=new Set();
  for(const entry of collect(raw,'guideEntryRenderer',4096)) {
    const id=entry.entryData?.guideEntryData?.guideEntryId ?? entry.navigationEndpoint?.browseEndpoint?.browseId;
    if(!channelId(id) || !styles.has(entry.presentationStyle))continue;
    const mark=styles.get(entry.presentationStyle);
    if(marks.has(id) && marks.get(id)!==mark)conflicts.add(id);
    marks.set(id,mark);
  }
  for(const id of conflicts)marks.delete(id);
  return marks;
}
// One optional guide request; its failure never fails the channel list.
export async function channelMarks(client) {
  try {
    const response=await client.actions.execute('/guide',{});
    return response?.success===true && response.data && typeof response.data==='object'?newContent(response.data):new Map();
  } catch {return new Map();}
}
