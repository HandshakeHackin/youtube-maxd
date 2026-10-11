// Posting a top-level comment or a reply, once per confirmation session, and
// the comment-row state the pinned parser exposes. Reply endpoints stay in the
// worker behind an expiring, profile-owned reference.
// SPDX-License-Identifier: GPL-3.0-or-later
import {createHash,randomUUID} from 'node:crypto';
import {Intents,accepted} from './intents.mjs';

export function commentText(value,ErrorType) {
  if(typeof value!=='string' || !value.trim() || Array.from(value).length>1000 || /[\0-\x09\x0b-\x1f\x7f]/.test(value))
    throw new ErrorType('invalid_comment','Enter a comment of 1 to 1000 characters.');
  return value.trim();
}
// The pinned Button always carries an endpoint object; only one with a
// command or an API path can be called.
const callable=endpoint=>!!(endpoint && (endpoint.command || endpoint.metadata?.api_url));
// Viewer and creator state of one parsed comment.
export const commentState=node=>({liked:node.is_liked===true,disliked:node.is_disliked===true,
  hearted:node.is_hearted===true,member:node.is_member===true});
// First-page facts from the parsed comments header.
export function commentHeader(feed,signedIn) {
  const header=feed?.header;
  const count=String(header?.comments_count?.toString?.() || header?.count?.toString?.() || '').replaceAll('\0','').slice(0,32);
  return {comment_count:count==='N/A'?'':count,can_post:!!signedIn && callable(header?.create_renderer?.submit_button?.endpoint)};
}
const fingerprint=text=>createHash('sha256').update(text).digest('hex');

export class CommentPosts {
  constructor(ErrorType,clock=Date.now) {
    this.ErrorType=ErrorType;this.clock=clock;this.targets=new Map();this.intents=new Intents(ErrorType,clock);
  }
  // A reference exists only when the pinned reply dialog carries its endpoint.
  register(owner,node) {
    if(typeof node?.reply!=='function' || !callable(node.reply_command?.dialog?.reply_button?.endpoint))return '';
    for(const [key,entry] of this.targets)if(entry.deadline<=this.clock())this.targets.delete(key);
    while(this.targets.size>=256)this.targets.delete(this.targets.keys().next().value);
    const cursor=randomUUID();
    this.targets.set(cursor,{owner,node,deadline:this.clock()+1800000});
    return cursor;
  }
  async run(owner,client,input) {
    const reply=input.action==='comment_reply',text=commentText(input.text,this.ErrorType);
    if(!client.session?.logged_in)throw new this.ErrorType('account_required','Sign in to the selected account first.',401);
    let target=null;
    if(reply) {
      target=this.targets.get(input.cursor);
      if(!target || target.owner!==owner || target.deadline<=this.clock())
        throw new this.ErrorType('cursor_expired','Reload the comments before replying.',409);
    }
    const identity=`${input.action}:${input.id}:${reply?input.cursor:''}:${fingerprint(text)}`;
    return this.intents.once(owner,input.session,identity,'comment_unconfirmed',async()=>{
      const response=reply?await target.node.reply(text):await client.interact.comment(input.id,text);
      if(!accepted(response) || response.data?.actionResult?.status!=='STATUS_SUCCEEDED')
        throw new this.ErrorType('comment_unconfirmed','YouTube did not confirm the comment.',502);
      return {api_version:1,success:true,id:input.id,session:input.session,reply};
    });
  }
}
