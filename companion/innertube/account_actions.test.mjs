import test from 'node:test';
import assert from 'node:assert/strict';
import {Library,LibraryError} from './library.mjs';
import {AccountActions,historyControls,historyPaused} from './account_actions.mjs';
import {YT,YTNodes} from 'youtubei.js';
const id='abcdefghijk',playlist='PLabcdefghijk';
const profiles=[{id:'signed',cookie:'synthetic'},{id:'other',cookie:'synthetic'}];
const control=(icon,token,extra={})=>({buttonRenderer:{text:{runs:[{text:'Localized label'}]},icon:{iconType:icon},...extra,
  navigationEndpoint:{confirmDialogEndpoint:{content:{confirmDialogRenderer:{title:{runs:[{text:'Localized title'}]},
    dialogMessages:[{runs:[{text:'Localized question'}]}],cancelButton:{buttonRenderer:{text:{simpleText:'No'}}},
    confirmButton:{buttonRenderer:{text:{simpleText:'Yes'},serviceEndpoint:{commandMetadata:{webCommandMetadata:{sendPost:true,apiUrl:'/youtubei/v1/feedback'}},
      feedbackEndpoint:{feedbackToken:token}}}}}}}}}});
const history=buttons=>new YT.History({},{data:{contents:{twoColumnBrowseResultsRenderer:{
  tabs:[{tabRenderer:{selected:true,content:{sectionListRenderer:{contents:[]}}}}],
  secondaryContents:{browseFeedActionsRenderer:{contents:buttons}}}}}});

test('history controls come from the pinned page by structured icon and confirmation token',()=>{
  const active=history([control('DELETE','private-clear'),control('PAUSE','private-pause'),
    {buttonRenderer:{text:{simpleText:'Manage'},icon:{iconType:'SETTINGS'},navigationEndpoint:{urlEndpoint:{url:'https://myactivity.google.com/'}}}}]);
  assert.equal(active.feed_actions.contents.length,3);
  assert.deepEqual(historyControls(active),{history_clear:'private-clear',history_pause:'private-pause'});
  assert.equal(historyPaused(active),false);
  const paused=history([control('DELETE','private-clear'),control('PLAY','private-resume')]);
  assert.deepEqual(historyControls(paused),{history_clear:'private-clear',history_resume:'private-resume'});
  assert.equal(historyPaused(paused),true);
  // Ambiguous, disabled, token-less or direct (unconfirmed) buttons expose nothing.
  assert.deepEqual(historyControls(history([control('DELETE','one'),control('DELETE','two'),control('PAUSE','kept')])),{history_pause:'kept'});
  assert.deepEqual(historyControls(history([control('DELETE','x',{isDisabled:true}),control('PAUSE',''),control('PLAY','y'.repeat(8193)),
    {buttonRenderer:{icon:{iconType:'DELETE'},serviceEndpoint:{feedbackEndpoint:{feedbackToken:'direct'}}}}])),{});
  assert.equal(historyPaused(history([control('PAUSE','a'),control('PLAY','b')])),undefined);
  for(const feed of [null,{},{feed_actions:{contents:'bad'}},history([])]) {
    assert.deepEqual(historyControls(feed),{});assert.equal(historyPaused(feed),undefined);
  }
});

test('pause, resume and clear send exactly the offered token once per confirmation',async()=>{
  const sent=[];let processed=true,buttons=[control('DELETE','private-clear'),control('PAUSE','private-pause')],loads=0;
  const library=new Library(async()=>({session:{logged_in:true},getHistory:async()=>{loads++;return history(buttons);},
    actions:{async execute(path,body){sent.push([path,body]);return {data:{feedbackResponses:[{isProcessed:processed}]}};}}}),profiles);
  const listed=await library.execute({action:'history',profile:'signed'});
  assert.equal(listed.history_paused,false);assert.ok(!JSON.stringify(listed).includes('private'));
  const request={action:'history_pause',profile:'signed',session:'intent-1'};
  await assert.rejects(library.execute({...request,profile:'guest'}),{code:'account_required',status:401});
  await assert.rejects(library.execute({...request,session:''}),{code:'invalid_request'});
  await assert.rejects(library.execute({...request,session:'bad session'}),{code:'invalid_request'});
  assert.equal(sent.length,0);
  const result=await library.execute(request);
  assert.deepEqual(result,{api_version:1,success:true,history:'paused'});
  assert.strictEqual(await library.execute(request),result);
  assert.deepEqual(sent,[['/feedback',{feedbackTokens:['private-pause']}]]);
  // The same confirmation cannot be reused for another change.
  await assert.rejects(library.execute({...request,action:'history_clear'}),{code:'history_unconfirmed',status:409});
  // A control that is not offered sends nothing and may be retried after a reload.
  const resume={action:'history_resume',profile:'signed',session:'intent-2'};
  await assert.rejects(library.execute(resume),{code:'history_control_unavailable',status:404});
  buttons=[control('DELETE','private-clear'),control('PLAY','private-resume')];
  assert.deepEqual(await library.execute(resume),{api_version:1,success:true,history:'active'});
  assert.equal((await library.execute({action:'history',profile:'signed'})).history_paused,true);
  processed=false;
  const clear={action:'history_clear',profile:'signed',session:'intent-3'};
  await assert.rejects(library.execute(clear),{code:'history_unconfirmed',status:502});
  processed=true;
  await assert.rejects(library.execute(clear),{code:'history_unconfirmed',status:409});
  assert.deepEqual(await library.execute({...clear,session:'intent-4'}),{api_version:1,success:true,history:'cleared'});
  assert.deepEqual(sent.map(call=>call[1].feedbackTokens[0]),['private-pause','private-resume','private-clear','private-clear']);
  // Another profile owns its own confirmations.
  assert.deepEqual(await library.execute({...clear,profile:'other',session:'intent-4'}),{api_version:1,success:true,history:'cleared'});
});

test('saving and removing a foreign playlist is confirmed, scoped and never repeated',async()=>{
  const calls=[];let reply={success:true,status_code:200,data:{}},finish=null;
  const client={session:{logged_in:true},playlist:{
    addToLibrary:async key=>{calls.push(['save',key]);return finish?new Promise(resolve=>finish=resolve):reply;},
    removeFromLibrary:async key=>{calls.push(['remove',key]);return reply;}}};
  const library=new Library(async()=>client,profiles);
  const save={action:'playlist_save',profile:'signed',playlist,session:'save-1'};
  await assert.rejects(library.execute({...save,profile:'guest'}),{code:'account_required'});
  for(const patch of [{playlist:'WL'},{playlist:'LL'},{playlist:'short'},{session:''}])
    await assert.rejects(library.execute({...save,...patch}),error=>['invalid_request','invalid_identity'].includes(error.code));
  assert.equal(calls.length,0);
  const saved=await library.execute(save);
  assert.deepEqual(saved,{api_version:1,success:true,playlist_id:playlist,saved:true});
  assert.strictEqual(await library.execute(save),saved);
  await assert.rejects(library.execute({...save,playlist:'PLzyxwvutsrqp'}),{code:'playlist_unconfirmed',status:409});
  await assert.rejects(library.execute({...save,action:'playlist_unsave'}),{code:'playlist_unconfirmed',status:409});
  assert.deepEqual(await library.execute({...save,action:'playlist_unsave',session:'remove-1'}),
    {api_version:1,success:true,playlist_id:playlist,saved:false});
  assert.deepEqual(calls,[['save',playlist],['remove',playlist]]);
  for(const [session,bad] of [['bad-1',{success:false,status_code:200}],['bad-2',{success:true,status_code:403}],
      ['bad-3',{success:true,status_code:200,data:{error:{code:400}}}],['bad-4',null]]) {
    reply=bad;
    await assert.rejects(library.execute({...save,session}),{code:'playlist_unconfirmed',status:502});
    reply={success:true,status_code:200,data:{}};
    await assert.rejects(library.execute({...save,session}),{code:'playlist_unconfirmed',status:409});
  }
  // A concurrent duplicate is refused while the first attempt is in flight.
  finish=()=>{};
  const pending=library.execute({...save,session:'slow'});
  await new Promise(resolve=>setImmediate(resolve));
  await assert.rejects(library.execute({...save,session:'slow'}),{code:'playlist_unconfirmed',status:409});
  finish(reply);assert.equal((await pending).saved,true);
  assert.equal(calls.filter(call=>call[0]==='save').length,6);
});

test('retained account confirmations are bounded, expire and require a signed-in session',async()=>{
  let now=0;
  const actions=new AccountActions(LibraryError,()=>now);
  const client={session:{logged_in:true},playlist:{addToLibrary:async()=>({success:true,status_code:200})}};
  const request={action:'playlist_save',playlist,session:'first'};
  await assert.rejects(actions.run('signed',{session:{logged_in:false}},request),{code:'account_required'});
  const first=await actions.run('signed',client,request);
  for(let i=0;i<255;++i)await actions.run('signed',client,{...request,session:'s'+i});
  await assert.rejects(actions.run('signed',client,{...request,session:'overflow'}),{code:'account_busy',status:429});
  assert.strictEqual(await actions.run('signed',client,request),first);
  now=1800001;
  assert.notStrictEqual(await actions.run('signed',client,request),first);assert.equal(actions.entries.size,1);
  await assert.rejects(actions.run('signed',client,{action:'history_delete',session:'x'}),{code:'invalid_request'});
});

test('a Subscriptions card can be hidden once through its retained feedback reference',async()=>{
  const sent=[];
  const item=(icon,token)=>({menuServiceItemRenderer:{text:{simpleText:'Localized'},icon:{iconType:icon},
    serviceEndpoint:{feedbackEndpoint:{feedbackToken:token}}}});
  const card=new YTNodes.Video({videoId:id,title:{runs:[{text:'Fixture'}]},thumbnail:{thumbnails:[]},
    menu:{menuRenderer:{items:[item('WATCH_LATER','private-later'),item('HIDE','private-hide')]}}});
  const bare=new YTNodes.Video({videoId:'12345678901',title:{runs:[{text:'No menu'}]},thumbnail:{thumbnails:[]}});
  const library=new Library(async()=>({session:{logged_in:true},getSubscriptionsFeed:async()=>({videos:[card,bare],has_continuation:false}),
    actions:{async execute(path,body){sent.push([path,body]);return {data:{feedbackResponses:[{isProcessed:true}]}};}}}),profiles);
  const page=await library.execute({action:'subscriptions',profile:'signed'}),row=page.items[0];
  assert.match(row.feedback_cursor,/^[0-9a-f-]{36}$/);
  assert.deepEqual([row.feedback_hide,row.feedback_not_interested,row.feedback_channel],[true,false,false]);
  assert.ok(!('feedback_cursor' in page.items[1]));assert.ok(!JSON.stringify(page).includes('private'));
  assert.deepEqual(row.playback_context,{group:row.playback_context.group,index:0});
  const request={action:'feedback',profile:'signed',id,cursor:row.feedback_cursor,feedback:'hide'};
  await assert.rejects(library.execute({...request,feedback:'not_interested'}),{code:'invalid_feedback'});
  await assert.rejects(library.execute({...request,profile:'other'}),{code:'cursor_expired'});
  const result=await library.execute(request);
  assert.deepEqual(result,{api_version:1,success:true,video_id:id,feedback:'hide'});
  assert.strictEqual(await library.execute(request),result);
  assert.deepEqual(sent,[['/feedback',{feedbackTokens:['private-hide']}]]);
});
