import {randomUUID} from 'node:crypto';

// Feedback tokens and parser objects stay inside the worker. The UI receives
// only an expiring reference to a video from its own account history page.
export class HistoryActions {
  constructor(ErrorType,clock=Date.now) {this.ErrorType=ErrorType;this.clock=clock;this.entries=new Map();this.watched=new Map();}
  async markWatched(owner,client,input) {
    if(!client.session?.logged_in)
      throw new this.ErrorType('account_required','Sign in to the selected account first.',401);
    if(!/^[A-Za-z0-9_-]{1,64}$/.test(input.session ?? ''))
      throw new this.ErrorType('invalid_progress','A unique confirmation session is required.');
    for(const [key,entry] of this.watched)if(entry.deadline<=this.clock())this.watched.delete(key);
    const key=`${owner}:${input.session}`, previous=this.watched.get(key);
    if(previous) {
      if(previous.id!==input.id)throw new this.ErrorType('invalid_progress','The confirmation belongs to another video.');
      if(previous.result)return previous.result;
      throw new this.ErrorType('history_unconfirmed','Check account history before repeating this change.',409);
    }
    if(this.watched.size>=256)throw new this.ErrorType('history_busy','Too many history changes are retained.',429);
    const entry={id:input.id,deadline:this.clock()+1800000};this.watched.set(key,entry);
    const info=await client.getBasicInfo(input.id), details=info?.basic_info;
    if(details?.id!==input.id || !Number.isFinite(details.duration) || details.duration<=0 ||
       details.duration>1000000 || details.is_live || details.is_upcoming)
      throw new this.ErrorType('history_unconfirmed','A finished video with a known duration is required.',409);
    const started=await info.addToWatchHistory();
    if(started?.ok!==true)throw new this.ErrorType('history_unconfirmed','YouTube did not confirm the history change.',502);
    const updated=await info.updateWatchTime(details.duration);
    if(updated?.ok!==true)throw new this.ErrorType('history_unconfirmed','YouTube did not confirm the watched position.',502);
    entry.result={api_version:1,success:true,video_id:input.id,position_sec:details.duration};return entry.result;
  }
  register(owner,feed,id) {
    if(typeof feed?.removeVideo!=='function' || !/^[A-Za-z0-9_-]{11}$/.test(id ?? ''))return '';
    for(const [key,entry] of this.entries) {
      if(entry.deadline<=this.clock())this.entries.delete(key);
      else if(entry.owner===owner && entry.feed===feed && entry.id===id)return key;
    }
    if(this.entries.size>=256)return '';
    const cursor=randomUUID();
    this.entries.set(cursor,{owner,feed,id,deadline:this.clock()+120000,attempted:false});return cursor;
  }
  async remove(owner,client,input) {
    const entry=this.entries.get(input.cursor);
    if(!entry || entry.owner!==owner || entry.id!==input.id || entry.deadline<=this.clock())
      throw new this.ErrorType('cursor_expired','Reload account history before removing this video.',409);
    if(!client.session?.logged_in)
      throw new this.ErrorType('account_required','Sign in to the selected account first.',401);
    if(entry.attempted) {
      if(entry.result)return entry.result;
      throw new this.ErrorType('history_unconfirmed','Check account history before retrying this removal.',409);
    }
    entry.attempted=true;
    // Bound removal to the exact retained page; never search another feed for
    // a replacement token after the user chose this video.
    const confirmed=await entry.feed.removeVideo(entry.id,1);
    if(confirmed!==true)
      throw new this.ErrorType('history_unconfirmed','YouTube did not confirm history removal.',502);
    entry.result={api_version:1,success:true,video_id:entry.id};return entry.result;
  }
}
