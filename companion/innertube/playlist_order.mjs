// Sort order of the account's own playlist. The pinned playlist parser drops
// the page's sort menu, so it is read from the raw browse response: a sort
// sub-menu whose items each carry one ACTION_SET_PLAYLIST_VIDEO_ORDER edit for
// this playlist. Only a value the page offered is ever sent.
// SPDX-License-Identifier: GPL-3.0-or-later
import {Intents,accepted,collect} from './intents.mjs';

// SmartTube's five orders (MediaItemService.PLAYLIST_ORDER_*), which it sends
// as the edit action's numeric order. Other values (manual) are not offered.
export const orderNames=['added_newest','added_oldest','popular','published_newest','published_oldest'];
const byValue=new Map(orderNames.map((name,index)=>[index+1,name]));
const playlistId=value=>typeof value==='string' && /^(WL|LL|[A-Za-z0-9_-]{10,100})$/.test(value);

export function playlistOrders(raw,id) {
  const offered=new Map(),conflicts=new Set();
  let selected='',selections=0;
  for(const menu of collect(raw,'sortFilterSubMenuRenderer',8)) {
    for(const item of Array.isArray(menu.subMenuItems)?menu.subMenuItems.slice(0,16):[]) {
      const edit=item?.serviceEndpoint?.playlistEditEndpoint,actions=edit?.actions;
      if(edit?.playlistId!==id || !Array.isArray(actions) || actions.length!==1 ||
         actions[0]?.action!=='ACTION_SET_PLAYLIST_VIDEO_ORDER')continue;
      const name=byValue.get(actions[0].playlistVideoOrder);
      if(!name)continue;
      if(offered.has(name))conflicts.add(name);
      offered.set(name,actions[0].playlistVideoOrder);
      if(item.selected===true) {selected=name;selections++;}
    }
  }
  for(const name of conflicts)offered.delete(name);
  return {offered,order:selections===1 && offered.has(selected)?selected:''};
}
export class PlaylistOrders {
  constructor(ErrorType,clock=Date.now) {this.ErrorType=ErrorType;this.intents=new Intents(ErrorType,clock);}
  async read(client,id) {
    const response=await client.actions.execute('/browse',{browseId:`VL${id}`});
    if(!accepted(response) || !response.data || typeof response.data!=='object')
      throw new this.ErrorType('upstream_failure','YouTube did not return this playlist.',502);
    return playlistOrders(response.data,id);
  }
  async run(owner,client,input) {
    const id=input.playlist,wanted=input.playlist_order ?? '';
    if(!playlistId(id) || (wanted!=='' && !orderNames.includes(wanted)))
      throw new this.ErrorType('invalid_request','Select a playlist and a supported order.');
    if(!client.session?.logged_in)throw new this.ErrorType('account_required','Sign in to the selected account first.',401);
    if(!wanted) {
      const {offered,order}=await this.read(client,id);
      return {api_version:1,kind:'playlist_order',playlist_id:id,editable:offered.size>0,
        orders:orderNames.filter(name=>offered.has(name)),order};
    }
    return this.intents.once(owner,input.session,`${id}:${wanted}`,'playlist_unconfirmed',async release=>{
      let offered;
      try {({offered}=await this.read(client,id));} catch(error) {release();throw error;}
      if(!offered.has(wanted)) {
        release();
        throw new this.ErrorType('playlist_order_unavailable','YouTube did not offer this order for the playlist.',404);
      }
      const response=await client.actions.execute('browse/edit_playlist',{playlistId:id,
        actions:[{action:'ACTION_SET_PLAYLIST_VIDEO_ORDER',playlistVideoOrder:offered.get(wanted)}]});
      if(!accepted(response) || response.data?.status!=='STATUS_SUCCEEDED')
        throw new this.ErrorType('playlist_unconfirmed','YouTube did not confirm the playlist order.',502);
      return {api_version:1,success:true,playlist_id:id,order:wanted};
    });
  }
}
