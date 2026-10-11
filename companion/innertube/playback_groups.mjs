// Profile-owned browsing groups used by SmartTube-style next/previous selection.
// SPDX-License-Identifier: GPL-3.0-or-later
import {randomUUID} from 'node:crypto';
import {hydrateWindowChannels} from './video_channels.mjs';
const videoId=value=>typeof value==='string' && /^[A-Za-z0-9_-]{11}$/.test(value);
export class PlaybackGroups {
  constructor(ErrorType,normalize,clock=()=>Date.now()) {
    this.ErrorType=ErrorType;this.normalize=normalize;this.clock=clock;
    this.groups=new Map();this.continuations=new WeakMap();
  }
  fail(code,message) {throw new this.ErrorType(code,message,409);}
  prune() {
    for(const [key,group] of this.groups)if(group.deadline<=this.clock())this.groups.delete(key);
  }
  get(owner,key) {
    this.prune();const group=this.groups.get(key);
    if(!group || group.owner!==owner)this.fail('group_expired','Reload the source group to continue.');
    group.deadline=this.clock()+21600000;return group;
  }
  async continuation(feed) {
    if(!feed.has_continuation || typeof feed.getContinuation!=='function')
      this.fail('group_unavailable','The group continuation is unavailable.');
    if(!this.continuations.has(feed)) {
      const pending=Promise.resolve().then(()=>feed.getContinuation());
      this.continuations.set(feed,pending);
      pending.catch(()=>{if(this.continuations.get(feed)===pending)this.continuations.delete(feed);});
    }
    return this.continuations.get(feed);
  }
  attach(owner,feed,items,key='',allowNewGroup=false) {
    this.prune();
    if(key && allowNewGroup) {
      const existing=this.groups.get(key);
      if(!existing || (existing.owner===owner && !existing.pages.has(feed) && existing.rows.length+items.length>4096))key='';
    }
    if(!key) {
      while(this.groups.size>=32)this.groups.delete(this.groups.keys().next().value);
      key=randomUUID();this.groups.set(key,{owner,rows:[],pages:new WeakMap(),tail:null,deadline:this.clock()+21600000});
    }
    const group=this.get(owner,key);
    if(group.pages.has(feed))return {key,items:group.pages.get(feed)};
    if(group.rows.length+items.length>4096)this.fail('group_limit','Reload a smaller source group to continue.');
    const start=group.rows.length;
    const rows=items.map((item,i)=>({...item,...(!item.is_folder && videoId(item.id)?{playback_context:{group:key,index:start+i}}:{})}));
    group.rows.push(...rows);group.pages.set(feed,rows);group.tail=feed;
    return {key,items:rows};
  }
  async window(owner,input,client) {
    const position=n=>Number.isInteger(n)&&n>=0&&n<=4095;
    if(typeof input.group!=='string' || !/^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/.test(input.group) || !videoId(input.id) ||
        input.playlist || input.channel || !position(input.index) || !position(input.target??input.index) || ![-1,0,1].includes(input.direction??0) ||
        (input.require_channel!==undefined && typeof input.require_channel!=='boolean') ||
        (input.require_name!==undefined && (typeof input.require_name!=='boolean' || (input.require_name && !input.require_channel))))
      this.fail('invalid_request','Group navigation requires an anchored source occurrence.');
    const group=this.get(owner,input.group),target=input.target??input.index,direction=input.direction??0;
    if(group.rows[input.index]?.id!==input.id)this.fail('group_changed','The source occurrence is no longer available.');
    const upcoming=row=>typeof row.upcoming_start_ms==='number' && Number.isFinite(row.upcoming_start_ms) && row.upcoming_start_ms>0
      ? row.upcoming_start_ms>this.clock() : !!row.is_upcoming;
    const playable=row=>row && !row.is_folder && !upcoming(row) && videoId(row.id);
    let selected=-1,continued=false;
    for(;;) {
      if(direction<0) {for(let i=Math.min(target,group.rows.length-1);i>=0;--i)if(playable(group.rows[i])){selected=i;break;}}
      else {for(let i=target;i<group.rows.length;++i) {
        if(playable(group.rows[i])){selected=i;break;}if(!direction)break;
      }}
      if(selected>=0 || direction<=0 || !group.tail?.has_continuation || continued)break;
      // SmartTube attempts one group continuation before falling back to metadata.
      const feed=await this.continuation(group.tail);
      this.attach(owner,feed,this.normalize(feed),input.group);continued=true;
    }
    if(selected<0)this.fail('group_unavailable','No playable source occurrence is available.');
    const start=Math.max(0,selected-63),end=Math.min(group.rows.length,start+128);
    const positions=[];for(let i=start;i<end;++i)if(playable(group.rows[i]))positions.push(i);
    const response={api_version:1,playlist:'group:'+input.group,anchor_video_id:input.id,anchor_index:input.index,
      video_id:group.rows[selected].id,index:selected,window_start:positions[0],video_indices:positions,
      video_ids:positions.map(i=>group.rows[i].id),video_channels:positions.map(i=>group.rows[i].channel_id||''),
      ...(input.require_name?{video_channel_names:positions.map(i=>group.rows[i].subtitle||'')}: {})};
    if(!group.tail.has_continuation)response.playlist_size=group.rows.length;
    if(selected!==target){response.requested_index=target;response.skipped_unavailable=Math.abs(selected-target);}
    if(!input.require_channel)return response;
    const verified=await hydrateWindowChannels(client,response,this.ErrorType,!!input.require_name);
    if(this.get(owner,input.group)!==group)this.fail('group_expired','Reload the source group to continue.');
    // Reuse verified identities on later windows and all repeated occurrences.
    // Commit only after the whole verification succeeds and ownership survives.
    const identities=new Map(verified.video_ids.map((id,i)=>[id,{channel:verified.video_channels[i],name:verified.video_channel_names?.[i]}]));
    for(const row of group.rows) {
      const identity=identities.get(row.id);
      if(!identity || (row.channel_id && row.channel_id!==identity.channel))continue;
      row.channel_id=identity.channel;
      if(input.require_name)row.subtitle=identity.name;
    }
    return verified;
  }
  removeOwner(owner) {for(const [key,group] of this.groups)if(group.owner===owner)this.groups.delete(key);}
}
