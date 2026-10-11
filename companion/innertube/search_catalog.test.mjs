import test from 'node:test';
import assert from 'node:assert/strict';
import {Library} from './library.mjs';
import {YT,Parser,Innertube} from 'youtubei.js';
const pad=n=>String(n).padStart(11,'0');
const video=n=>({type:'Video',id:pad(n),title:'Video '+n});
const channel='UC'+'c'.repeat(22);

test('search forwards validated pinned filters once and pages through the retained feed',async()=>{
  const calls=[];
  const tail={results:[video(40)],has_continuation:false};
  const feed={results:Array.from({length:30},(_,i)=>video(i)),has_continuation:true,async getContinuation(){return tail;}};
  const library=new Library(async()=>({search:async(query,filters)=>{calls.push({query,filters});return feed;}}),[{id:'other',cookie:'synthetic'}]);
  const request={action:'search',query:' cats ',upload_date:'week',duration:'long',type:'video',features:['live','4k'],order:'view_count'};
  let page=await library.execute(request);
  assert.deepEqual(calls,[{query:'cats',filters:{upload_date:'week',type:'video',duration:'over_twenty_mins',
    prioritize:'popularity',features:['4k','live']}}]);
  assert.equal(page.kind,'catalog');assert.equal(page.items.length,24);assert.equal(page.items[0].kind,'video');
  assert.match(page.items[0].playback_context.group,/^[0-9a-f-]{36}$/);
  await assert.rejects(library.execute({action:'continue',cursor:page.cursor,profile:'other'}),{code:'cursor_expired'});
  const ids=page.items.map(item=>item.id);
  while(page.has_more) {page=await library.execute({action:'continue',cursor:page.cursor});ids.push(...page.items.map(item=>item.id));}
  assert.deepEqual(ids,[...Array.from({length:30},(_,i)=>pad(i)),pad(40)]);
  assert.equal(calls.length,1); // Continuations never repeat or rebuild the filtered query.
  await library.execute({action:'search',query:'dogs',upload_date:'any',duration:'any',type:'all',features:[],order:'relevance'});
  assert.deepEqual(calls[1],{query:'dogs',filters:{}});
  for(const [duration,expected] of [['short','under_three_mins'],['medium','three_to_twenty_mins']]) {
    await library.execute({action:'search',query:'dogs',duration});assert.equal(calls.at(-1).filters.duration,expected);
  }
});

test('unknown search options and invalid text never reach YouTube',async()=>{
  let calls=0;
  const library=new Library(async()=>({search:async()=>{calls++;return {results:[]};}}));
  for(const patch of [{upload_date:'hour'},{upload_date:7},{order:'rating'},{order:'upload_date'},{duration:'tiny'},{duration:['any']},
    {type:'artist'},{features:['live','live']},{features:'live'},{features:['bogus']},{features:[4]},{features:Array(12).fill('hd')},
    {query:''},{query:'   '},{query:'x'.repeat(201)},{query:'a\nb'},{query:7},{query:undefined}])
    await assert.rejects(library.execute({action:'search',query:'cats',...patch}),{code:'invalid_search'},JSON.stringify(patch));
  assert.equal(calls,0);
  await assert.rejects(library.execute({action:'search',query:'cats',profile:'missing'}),{code:'unknown_profile'});
});

test('pinned search parser keeps channel and playlist cards in upstream order',async()=>{
  const item=(id,title)=>({videoRenderer:{videoId:id,title:{runs:[{text:title}]},thumbnail:{thumbnails:[]}}});
  const raw={contents:{twoColumnSearchResultsRenderer:{primaryContents:{sectionListRenderer:{contents:[{itemSectionRenderer:{contents:[
    item(pad(1),'First'),
    {channelRenderer:{channelId:channel,title:{simpleText:'Channel <b>name</b>'},
      thumbnail:{thumbnails:[{url:'//yt3.ggpht.com/avatar=s88',width:88,height:88}]},
      subscriberCountText:{simpleText:'@handle'},videoCountText:{simpleText:'1.2M subscribers'},
      descriptionSnippet:{runs:[{text:'About this channel'}]},navigationEndpoint:{browseEndpoint:{browseId:channel}}}},
    {lockupViewModel:{contentId:'PLabcdefghijk',contentType:'LOCKUP_CONTENT_TYPE_PLAYLIST',
      metadata:{lockupMetadataViewModel:{title:{content:'A playlist'}}}}},
    {adSlotRenderer:{}},
    {shelfRenderer:{title:{simpleText:'Latest'},content:{verticalListRenderer:{items:[item(pad(2),'Shelf video')]}}}},
    item(pad(1),'Repeated'),item(pad(3),'Last')]}}]}}}}};
  const feed=new YT.Search({},Parser.parseResponse(raw),true);
  const library=new Library(async()=>({search:async()=>feed}));
  const page=await library.execute({action:'search',query:'mixed'});
  assert.deepEqual(page.items.map(row=>[row.kind,row.id]),[['video',pad(1)],['channel','channel:'+channel],
    ['playlist','library:playlist:PLabcdefghijk'],['video',pad(2)],['video',pad(3)]]);
  const card=page.items[1];
  assert.deepEqual(card,{kind:'channel',id:'channel:'+channel,channel_id:channel,title:'Channel <b>name</b>',
    subtitle:'1.2M subscribers',overview:'About this channel',art_url:'https://yt3.ggpht.com/avatar=s88',is_folder:true});
  assert.ok(page.items[2].is_folder && !page.items[2].playback_context);
  assert.equal(page.items[0].playback_context.index,0);assert.equal(page.items[3].playback_context.index,3);
});

test('search suggestions are bounded, literal and scoped to the selected profile',async()=>{
  const owners=[];
  const values=['cats',' cats\tvideo ','cats','',7,null,'x'.repeat(300),...Array.from({length:30},(_,i)=>'cat '+i)];
  const library=new Library(async profile=>({getSearchSuggestions:async query=>{owners.push([profile.id,query]);return values;}}),
    [{id:'signed',cookie:'synthetic'}]);
  const result=await library.execute({action:'search_suggestions',query:'cat',profile:'signed'});
  assert.equal(result.api_version,1);assert.equal(result.kind,'search_suggestions');assert.equal(result.query,'cat');
  assert.equal(result.suggestions.length,15);
  assert.deepEqual(result.suggestions.slice(0,4),['cats','cats video','x'.repeat(200),'cat 0']);
  assert.deepEqual((await library.execute({action:'search_suggestions',query:''})).suggestions.length,15);
  assert.deepEqual(owners,[['signed','cat'],['guest','']]);
  for(const query of ['x'.repeat(201),'a\nb',7])
    await assert.rejects(library.execute({action:'search_suggestions',query}),{code:'invalid_search'});
  await assert.rejects(library.execute({action:'search_suggestions',query:'cat',profile:'missing'}),{code:'unknown_profile'});
  for(const reply of [async()=>{throw new Error('private upstream text');},async()=>'cats',async()=>Array(1025).fill('a')]) {
    const failing=new Library(async()=>({getSearchSuggestions:reply}));
    await assert.rejects(failing.execute({action:'search_suggestions',query:'cat'}),{code:'upstream_format',status:502});
  }
});

test('pinned completion request uses the YouTube suggestion origin and parses its reply',async()=>{
  const urls=[];
  const client=await Innertube.create({retrieve_player:false,retrieve_innertube_config:false,generate_session_locally:true,
    fetch:async input=>{
      const url=new URL(input instanceof Request?input.url:input);urls.push(url);
      return new Response('window.google.ac.h(["cat",[["cat videos",0,[512]],["cat songs",0]],{"k":1}])');
    }});
  const library=new Library(async()=>client);
  const result=await library.execute({action:'search_suggestions',query:'cat'});
  assert.deepEqual(result.suggestions,['cat videos','cat songs']);
  assert.equal(urls.length,1);assert.equal(urls[0].hostname,'suggestqueries-clients6.youtube.com');
  assert.equal(urls[0].searchParams.get('q'),'cat');
});
