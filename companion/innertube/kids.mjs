// YouTube Kids home through the pinned Kids client (`FEkids_home`): one folder
// per category tab, opened on demand. Profile-owned, expiring references.
// SPDX-License-Identifier: GPL-3.0-or-later
import {randomUUID} from 'node:crypto';

const label=value=>String(value?.toString?.() ?? '').replaceAll('\0','').trim().slice(0,192);

export class KidsCatalog {
  constructor(ErrorType,normalizeVideo,clock=Date.now) {
    this.ErrorType=ErrorType;this.normalizeVideo=normalizeVideo;this.clock=clock;this.entries=new Map();
  }
  async root(owner,client) {
    if(typeof client?.kids?.getHomeFeed!=='function')
      throw new this.ErrorType('catalog_unavailable','The Kids section is unavailable.',404);
    const home=await client.kids.getHomeFeed(),tabs=home?.header?.category_tabs;
    if(!Array.isArray(tabs) || !tabs.length || tabs.length>32 || typeof home.selectCategoryTab!=='function')
      throw new this.ErrorType('upstream_format','YouTube returned an unsupported Kids layout.',502);
    for(const [key,entry] of this.entries)if(entry.deadline<=this.clock())this.entries.delete(key);
    const items=[];
    for(const tab of tabs) {
      const title=label(tab?.title);
      if(!title || !tab.endpoint)continue;
      while(this.entries.size>=128)this.entries.delete(this.entries.keys().next().value);
      const key=randomUUID();
      this.entries.set(key,{owner,home,tab,deadline:this.clock()+600000});
      items.push({id:`section:kids:${key}`,title,subtitle:'YouTube Kids',art_url:'',is_folder:true});
    }
    if(!items.length)throw new this.ErrorType('upstream_format','YouTube returned an unsupported Kids layout.',502);
    return {catalog_items:items,has_continuation:false};
  }
  async open(owner,key) {
    const entry=this.entries.get(key);
    if(!entry || entry.owner!==owner || entry.deadline<=this.clock())
      throw new this.ErrorType('cursor_expired','Reload this section to open the row.',409);
    if(!entry.feed) {
      const page=await entry.home.selectCategoryTab(entry.tab);
      const nodes=Array.from(page?.videos ?? []);
      if(nodes.length>4096)throw new this.ErrorType('upstream_limit','The section row exceeds the item limit.',502);
      const seen=new Set();
      // The Kids continuation is not requested: the generic feed mixin would
      // send it with the ordinary client context.
      entry.feed={catalog_items:nodes.map(node=>this.normalizeVideo(node)).filter(item=>item && !seen.has(item.id) && seen.add(item.id)),
        has_continuation:false};
    }
    return entry.feed;
  }
}
