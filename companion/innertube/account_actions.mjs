// One-attempt account changes: saving another channel's playlist to the
// library and the account's watch-history controls. A caller-supplied
// confirmation session identifies the intent; an unconfirmed or in-flight
// attempt is never sent again. Tokens stay inside the worker.
// SPDX-License-Identifier: GPL-3.0-or-later
const playlistId=value=>typeof value==='string' && /^[A-Za-z0-9_-]{10,100}$/.test(value) && !['WL','LL'].includes(value);
// Structured icons of the History page controls, never their localized labels.
const controls={history_clear:'DELETE',history_pause:'PAUSE',history_resume:'PLAY'};
const states={history_clear:'cleared',history_pause:'paused',history_resume:'active'};
const token=value=>typeof value==='string' && value.length>0 && value.length<=8192;

// Each control is a button that opens a confirmation dialog whose confirm
// button carries one feedback token. Anything else exposes no control.
export function historyControls(feed) {
  const found={},conflicts=new Set();
  const nodes=feed?.feed_actions?.contents;
  for(const node of Array.isArray(nodes)?nodes.slice(0,32):[]) {
    if(node?.type!=='Button' || node.is_disabled)continue;
    const dialog=node.endpoint?.dialog,confirm=dialog?.type==='ConfirmDialog'?dialog.confirm_button?.endpoint:null;
    const value=confirm?.name==='feedbackEndpoint'?confirm.payload?.feedbackToken:undefined;
    for(const [kind,icon] of Object.entries(controls)) {
      if(node.icon_type!==icon || !token(value))continue;
      if(found[kind]!==undefined && found[kind]!==value)conflicts.add(kind);
      found[kind]=value;
    }
  }
  for(const kind of conflicts)delete found[kind];
  return found;
}
// True/false only when exactly one of pause and resume is offered.
export function historyPaused(feed) {
  const found=historyControls(feed);
  return !!found.history_pause===!!found.history_resume?undefined:!!found.history_resume;
}
export class AccountActions {
  constructor(ErrorType,clock=Date.now) {this.ErrorType=ErrorType;this.clock=clock;this.entries=new Map();}
  async run(owner,client,input) {
    const kind=input.action,playlist=kind.startsWith('playlist_'),code=playlist?'playlist_unconfirmed':'history_unconfirmed';
    if(typeof input.session!=='string' || !/^[A-Za-z0-9_-]{1,64}$/.test(input.session) ||
       (playlist?!playlistId(input.playlist):!Object.hasOwn(controls,kind)))
      throw new this.ErrorType('invalid_request','The account change needs a confirmation session and a valid target.');
    if(!client.session?.logged_in)
      throw new this.ErrorType('account_required','Sign in to the selected account first.',401);
    for(const [key,entry] of this.entries)if(entry.deadline<=this.clock())this.entries.delete(key);
    const key=`${owner}:${input.session}`,identity=playlist?`${kind}:${input.playlist}`:kind,previous=this.entries.get(key);
    if(previous) {
      if(previous.identity===identity && previous.result)return previous.result;
      throw new this.ErrorType(code,'Check the account before repeating this change.',409);
    }
    if(this.entries.size>=256)throw new this.ErrorType('account_busy','Too many account changes are retained.',429);
    const entry={identity,deadline:this.clock()+1800000};this.entries.set(key,entry);
    if(playlist) {
      const save=kind==='playlist_save';
      const response=await client.playlist[save?'addToLibrary':'removeFromLibrary'](input.playlist);
      if(response?.success!==true || !Number.isInteger(response.status_code) || response.status_code<200 ||
         response.status_code>=300 || response.data?.error)
        throw new this.ErrorType(code,'YouTube did not confirm the playlist change.',502);
      entry.result={api_version:1,success:true,playlist_id:input.playlist,saved:save};return entry.result;
    }
    const chosen=historyControls(await client.getHistory())[kind];
    if(!chosen) {
      // Nothing was sent, so this intent may be retried after a reload.
      if(this.entries.get(key)===entry)this.entries.delete(key);
      throw new this.ErrorType('history_control_unavailable','YouTube did not offer this history control.',404);
    }
    const response=await client.actions.execute('/feedback',{feedbackTokens:[chosen]});
    const replies=response?.data?.feedbackResponses;
    if(!Array.isArray(replies) || replies.length!==1 || replies[0]?.isProcessed!==true)
      throw new this.ErrorType(code,'YouTube did not confirm the history change.',502);
    entry.result={api_version:1,success:true,history:states[kind]};return entry.result;
  }
}
