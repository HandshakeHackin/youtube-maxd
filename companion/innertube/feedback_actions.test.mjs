import test from 'node:test';
import assert from 'node:assert/strict';
import {Library,LibraryError} from './library.mjs';
import {FeedbackActions,feedbackTokens} from './feedback_actions.mjs';
import {YTNodes} from 'youtubei.js';
const id='abcdefghijk';
const profiles=[{id:'signed',cookie:'synthetic'},{id:'other',cookie:'synthetic'}];
const service=(icon,token)=>({menuServiceItemRenderer:{text:{simpleText:'Localized label'},icon:{iconType:icon},
  serviceEndpoint:{feedbackEndpoint:{feedbackToken:token}}}});
const card=(items=[service('NOT_INTERESTED','private-not-interested'),service('REMOVE','private-channel'),
  service('FLAG','private-report')])=>new YTNodes.Video({videoId:id,title:{runs:[{text:'Fixture'}]},thumbnail:{thumbnails:[]},
  menu:{menuRenderer:{items}}});
const home=node=>({videos:[node],contents:{contents:[{type:'RichItem',content:node}]},has_continuation:false});

test('Home cards retain private feedback tokens and submit each choice once',async()=>{
  const sent=[];let processed=true;
  const node=card();
  const library=new Library(async()=>({session:{logged_in:true},getHomeFeed:async()=>home(node),actions:{async execute(path,body) {
    sent.push([path,body]);return {data:{feedbackResponses:[{isProcessed:processed}]}};
  }}}),profiles);
  const guest=await library.execute({action:'home'});
  assert.ok(!('feedback_cursor' in guest.items[0]));
  const page=await library.execute({action:'home',profile:'signed'}),item=page.items[0];
  assert.match(item.feedback_cursor,/^[0-9a-f-]{36}$/);
  assert.equal(item.feedback_not_interested,true);assert.equal(item.feedback_channel,true);
  assert.ok(!JSON.stringify(page).includes('private'));
  assert.equal((await library.execute({action:'home',profile:'signed'})).items[0].feedback_cursor,item.feedback_cursor);
  const request={action:'feedback',profile:'signed',id,cursor:item.feedback_cursor,feedback:'not_interested'};
  await assert.rejects(library.execute({...request,profile:'guest'}),{code:'account_required',status:401});
  await assert.rejects(library.execute({...request,profile:'other'}),{code:'cursor_expired'});
  await assert.rejects(library.execute({...request,id:'12345678901'}),{code:'cursor_expired'});
  await assert.rejects(library.execute({...request,id:'bad'}),{code:'invalid_identity'});
  for(const feedback of ['','report','__proto__',7,undefined])
    await assert.rejects(library.execute({...request,feedback}),{code:'invalid_feedback'});
  assert.equal(sent.length,0);
  const result=await library.execute(request);
  assert.deepEqual(result,{api_version:1,success:true,video_id:id,feedback:'not_interested'});
  assert.strictEqual(await library.execute(request),result);
  assert.deepEqual(sent,[['/feedback',{feedbackTokens:['private-not-interested']}]]);
  processed=false;
  const channel={...request,feedback:'dont_recommend_channel'};
  await assert.rejects(library.execute(channel),{code:'feedback_unconfirmed',status:502});
  processed=true;
  await assert.rejects(library.execute(channel),{code:'feedback_unconfirmed',status:409});
  assert.deepEqual(sent.map(call=>call[1].feedbackTokens[0]),['private-not-interested','private-channel']);
});

test('feedback tokens are recognized by structured icons across classic and modern cards',()=>{
  assert.deepEqual(feedbackTokens(card()),{not_interested:'private-not-interested',dont_recommend_channel:'private-channel'});
  assert.deepEqual(feedbackTokens(card([service('NOT_INTERESTED','one'),service('NOT_INTERESTED','two'),service('REMOVE','kept')])),
    {dont_recommend_channel:'kept'});
  assert.deepEqual(feedbackTokens(card([service('NOT_INTERESTED',''),service('REMOVE','x'.repeat(8193)),service('SHARE','no')])),{});
  const row=(icon,token)=>({listItemViewModel:{title:{content:'Localized'},leadingImage:{sources:[{clientResource:{imageName:icon}}]},
    rendererContext:{commandContext:{onTap:{innertubeCommand:{feedbackEndpoint:{feedbackToken:token}}}}}}});
  const modern={metadata:{menu_button:{on_tap:{payload:{panelLoadingStrategy:{inlineContent:{sheetViewModel:{content:{listViewModel:{
    listItems:[row('ADD_TO_QUEUE_TAIL','queue'),row('NOT_INTERESTED','modern-one'),row('REMOVE','modern-two')]}}}}}}}}}};
  assert.deepEqual(feedbackTokens(modern),{not_interested:'modern-one',dont_recommend_channel:'modern-two'});
  let deep={};const top=deep;for(let i=0;i<40;++i)deep=deep.next={};
  deep.listItemViewModel=row('NOT_INTERESTED','too-deep').listItemViewModel;
  assert.deepEqual(feedbackTokens({metadata:{menu_button:{on_tap:{payload:top}}}}),{});
  for(const node of [null,{},{menu:{items:'bad'}}])assert.deepEqual(feedbackTokens(node),{});
});

test('feedback references are bounded, expire and never repeat an uncertain or concurrent intent',async()=>{
  let now=0,calls=0,finish;
  const actions=new FeedbackActions(LibraryError,()=>now);
  const client={session:{logged_in:true},actions:{execute(){calls++;return new Promise(resolve=>finish=resolve);}}};
  const node=card(),entry=actions.register('signed',node,id),request={id,cursor:entry.cursor,feedback:'not_interested'};
  assert.equal(actions.register('signed',node,id).cursor,entry.cursor);
  assert.notEqual(actions.register('other',node,id).cursor,entry.cursor);
  assert.equal(actions.register('signed',card([]),id),null);assert.equal(actions.register('',node,id),null);
  assert.equal(actions.register('signed',node,'bad'),null);
  await assert.rejects(actions.submit('signed',{session:{logged_in:false}},request),{code:'account_required'});
  const pending=actions.submit('signed',client,request);
  await assert.rejects(actions.submit('signed',client,request),{code:'feedback_unconfirmed'});
  finish({data:{feedbackResponses:[]}});await assert.rejects(pending,{code:'feedback_unconfirmed'});
  await assert.rejects(actions.submit('signed',client,request),{code:'feedback_unconfirmed'});assert.equal(calls,1);
  const failing={session:{logged_in:true},actions:{async execute(){throw new Error('network');}}};
  const second={...request,feedback:'dont_recommend_channel'};
  await assert.rejects(actions.submit('signed',failing,second));
  await assert.rejects(actions.submit('signed',client,second),{code:'feedback_unconfirmed'});assert.equal(calls,1);
  now=1800001;await assert.rejects(actions.submit('signed',client,request),{code:'cursor_expired'});
  const first=actions.register('signed',card(),id).cursor;
  for(let i=0;i<1024;++i)assert.ok(actions.register('signed',card(),id));
  assert.equal(actions.entries.size,1024);assert.ok(!actions.entries.has(first));
});
