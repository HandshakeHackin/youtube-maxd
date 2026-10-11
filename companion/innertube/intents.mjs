// One attempt per caller-chosen confirmation session. A confirmed repeat
// returns the same result; an in-flight, unconfirmed or different change under
// the same session is refused and never sent again.
// SPDX-License-Identifier: GPL-3.0-or-later
export class Intents {
  constructor(ErrorType,clock=Date.now) {this.ErrorType=ErrorType;this.clock=clock;this.entries=new Map();}
  // `run(release)` performs the change; calling `release()` before throwing
  // records that nothing was sent, so the session may be used again.
  async once(owner,session,identity,code,run) {
    if(typeof session!=='string' || !/^[A-Za-z0-9_-]{1,64}$/.test(session))
      throw new this.ErrorType('invalid_request','The change needs a confirmation session.');
    for(const [key,entry] of this.entries)if(entry.deadline<=this.clock())this.entries.delete(key);
    const key=`${owner}:${session}`,previous=this.entries.get(key);
    if(previous) {
      if(previous.identity===identity && previous.result)return previous.result;
      throw new this.ErrorType(code,'Check the account before repeating this change.',409);
    }
    if(this.entries.size>=256)throw new this.ErrorType('account_busy','Too many account changes are retained.',429);
    const entry={identity,deadline:this.clock()+1800000};this.entries.set(key,entry);
    entry.result=await run(()=>{if(this.entries.get(key)===entry)this.entries.delete(key);});
    return entry.result;
  }
}
// The pinned client reports a mutation as {success, status_code, data}.
export const accepted=response=>response?.success===true && Number.isInteger(response.status_code) &&
  response.status_code>=200 && response.status_code<300 && !response.data?.error;
// Bounded search of a raw response for every object stored under `name`.
export function collect(root,name,limit=64) {
  const found=[],stack=[[root,0]];
  let remaining=200000;
  while(stack.length) {
    const [node,depth]=stack.pop();
    if(!node || typeof node!=='object')continue;
    if(--remaining<0 || depth>48)return [];
    if(Object.hasOwn(node,name) && node[name] && typeof node[name]==='object') {
      found.push(node[name]);
      if(found.length>limit)return [];
      continue;
    }
    for(const value of Object.values(node))stack.push([value,depth+1]);
  }
  return found;
}
