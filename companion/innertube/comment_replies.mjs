// First page and continuations of one comment's replies.
// SPDX-License-Identifier: GPL-3.0-or-later

// The pinned CommentThread.getReplies reads the first continuation from
// `sub_threads` only. The live watch page keeps it in the reply renderer's
// `contents`, and answers with plain comment views followed by a "more
// replies" button (checked live on 2026-10-11 as Guest).
const token=node=>node?.type==='ContinuationItem';
const endpointOf=node=>node?.button?.endpoint ?? node?.endpoint;

function rows(response,client,ErrorType) {
  const updates=Array.from(response?.on_response_received_endpoints ?? []).filter(node=>node?.type==='AppendContinuationItemsAction');
  const contents=updates.length===1?Array.from(updates[0].contents ?? []):null;
  if(!contents || contents.length>4096)
    throw new ErrorType('upstream_format','YouTube did not return a reply list.',502);
  const more=contents.filter(token),next=endpointOf(more[0]);
  if(more.length>1 || (more.length && typeof next?.call!=='function'))
    throw new ErrorType('upstream_format','YouTube returned an unsupported reply continuation.',502);
  const replies=contents.filter(node=>['CommentView','CommentThread'].includes(node?.type));
  // Rating and answering a reply need the session the page was read with.
  for(const node of replies)if(typeof node.setActions==='function')node.setActions(client.actions);
  return {replies,has_continuation:more.length===1,...(more.length?{getContinuation:async()=>
    rows(await next.call(client.actions,{parse:true}),client,ErrorType)}:{})};
}

export async function threadReplies(thread,client,ErrorType) {
  const data=thread?.comment_replies_data;
  const first=Array.from(data?.contents ?? []).find(token);
  if(!first || thread.is_prepopulated || Array.from(data?.sub_threads ?? []).some(token))return thread.getReplies();
  const endpoint=endpointOf(first);
  if(typeof endpoint?.call!=='function')
    throw new ErrorType('upstream_format','YouTube returned an unsupported reply continuation.',502);
  return rows(await endpoint.call(client.actions,{parse:true}),client,ErrorType);
}
