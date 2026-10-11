import {randomUUID} from 'node:crypto';

// Recommendation feedback tokens stay inside the worker. A Home card exposes
// only an expiring, profile-owned reference and the available choices.
// Choices are recognized by structured icon identity, never by localized text.
const icons={not_interested:'NOT_INTERESTED',dont_recommend_channel:'REMOVE'};
const token=value=>typeof value==='string' && value.length>0 && value.length<=8192;
function sheetItems(root) {
  const found=[],stack=[[root,0]];
  let remaining=4000;
  while(stack.length) {
    const [node,depth]=stack.pop();
    if(!node || typeof node!=='object')continue;
    if(--remaining<0 || depth>24)return [];
    if(node.listItemViewModel && typeof node.listItemViewModel==='object') {found.push(node.listItemViewModel);continue;}
    for(const value of Object.values(node))stack.push([value,depth+1]);
  }
  return found;
}
export function feedbackTokens(node) {
  const tokens={},conflicts=new Set();
  const add=(icon,value)=>{
    for(const [kind,name] of Object.entries(icons)) {
      if(icon!==name || !token(value))continue;
      if(tokens[kind]!==undefined && tokens[kind]!==value)conflicts.add(kind);
      tokens[kind]=value;
    }
  };
  const classic=node?.menu?.items;
  for(const item of Array.isArray(classic)?classic.slice(0,64):[])
    add(item?.icon_type,item?.endpoint?.payload?.feedbackToken);
  for(const item of sheetItems(node?.metadata?.menu_button?.on_tap?.payload))
    add(item.leadingImage?.sources?.[0]?.clientResource?.imageName,
      item.rendererContext?.commandContext?.onTap?.innertubeCommand?.feedbackEndpoint?.feedbackToken);
  // Two different tokens for one choice are ambiguous; expose neither.
  for(const kind of conflicts)delete tokens[kind];
  return tokens;
}
export class FeedbackActions {
  constructor(ErrorType,clock=Date.now) {this.ErrorType=ErrorType;this.clock=clock;this.entries=new Map();}
  register(owner,node,id) {
    if(!owner || !node || typeof node!=='object' || !/^[A-Za-z0-9_-]{11}$/.test(id ?? ''))return null;
    for(const [key,entry] of this.entries) {
      if(entry.deadline<=this.clock())this.entries.delete(key);
      else if(entry.owner===owner && entry.node===node && entry.id===id)return {cursor:key,tokens:entry.tokens};
    }
    const tokens=feedbackTokens(node);
    if(!Object.keys(tokens).length)return null;
    while(this.entries.size>=1024)this.entries.delete(this.entries.keys().next().value);
    const cursor=randomUUID();
    this.entries.set(cursor,{owner,node,id,tokens,deadline:this.clock()+1800000,attempts:{}});
    return {cursor,tokens};
  }
  decorate(owner,node,item) {
    const entry=item && !item.is_folder?this.register(owner,node,item.id):null;
    return entry?{...item,feedback_cursor:entry.cursor,feedback_not_interested:!!entry.tokens.not_interested,
      feedback_channel:!!entry.tokens.dont_recommend_channel}:item;
  }
  async submit(owner,client,input) {
    const entry=this.entries.get(input.cursor),kind=input.feedback;
    if(!entry || entry.owner!==owner || entry.id!==input.id || entry.deadline<=this.clock())
      throw new this.ErrorType('cursor_expired','Reload Home before sending this feedback.',409);
    if(typeof kind!=='string' || !Object.hasOwn(icons,kind) || !entry.tokens[kind])
      throw new this.ErrorType('invalid_feedback','Select an available feedback choice.');
    if(!client.session?.logged_in)
      throw new this.ErrorType('account_required','Sign in to the selected account first.',401);
    const previous=entry.attempts[kind];
    if(previous) {
      if(previous.result)return previous.result;
      throw new this.ErrorType('feedback_unconfirmed','Check Home before repeating this feedback.',409);
    }
    const attempt=entry.attempts[kind]={};
    // Submit exactly the token retained for this card and choice.
    const response=await client.actions.execute('/feedback',{feedbackTokens:[entry.tokens[kind]]});
    const replies=response?.data?.feedbackResponses;
    if(!Array.isArray(replies) || replies.length!==1 || replies[0]?.isProcessed!==true)
      throw new this.ErrorType('feedback_unconfirmed','YouTube did not confirm this feedback.',502);
    attempt.result={api_version:1,success:true,video_id:entry.id,feedback:kind};return attempt.result;
  }
}
