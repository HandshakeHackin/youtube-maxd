import test from 'node:test';
import assert from 'node:assert/strict';
import { Library } from './library.mjs';
const video = i => String(i).padStart(11, '0');
const playlist = 'PLsyntheticqueue';
const row = (i, selected=false, id=video(i)) => ({playlistPanelVideoRenderer:{videoId:id, selected,
  navigationEndpoint:{watchEndpoint:{videoId:id, playlistId:playlist, index:i}}}});
const response = (contents, extra={}) => ({success:true,data:{contents:{twoColumnWatchNextResults:{playlist:{playlist:{playlistId:playlist,contents,...extra}}}}}});

test('Finite playlist size is explicit metadata, never the bounded or sparse window length',async()=>{
  for(const totalVideos of [21,'21',100001]) {
    const library=new Library(async()=>({actions:{execute:async()=>response([row(3),row(9,true),row(20)],{totalVideos})}}));
    const result=await library.execute({action:'playback_queue',playlist,id:video(9),index:9});
    assert.equal(result.playlist_size,Number(totalVideos));assert.equal(result.video_ids.length,3);
  }
  for(const totalVideos of [undefined,null,0,-1,20,21.5,true,'21 videos','021','2e2','1,000',100002]) {
    const library=new Library(async()=>({actions:{execute:async()=>response([row(3),row(9,true),row(20)],{totalVideos})}}));
    const result=await library.execute({action:'playback_queue',playlist,id:video(9),index:9});
    assert.ok(!Object.hasOwn(result,'playlist_size'));assert.equal(result.video_ids.length,3);
  }
});

test('Infinite and radio queues never advertise a finite size',async()=>{
  for(const isInfinite of [null,0,'false','true']) {
    const library=new Library(async()=>({actions:{execute:async()=>response([row(9,true)],{totalVideos:100,isInfinite})}}));
    const result=await library.execute({action:'playback_queue',playlist,id:video(9),index:9});
    assert.ok(!Object.hasOwn(result,'playlist_size'));
  }
  for(const radio of [false,true]) {
    const list=radio?'RDsyntheticqueue':playlist;
    const entry=row(9,true);entry.playlistPanelVideoRenderer.navigationEndpoint.watchEndpoint.playlistId=list;
    const library=new Library(async()=>({actions:{execute:async()=>({success:true,data:{playlistPanelRenderer:{
      playlistId:list,contents:[entry],totalVideos:100,isInfinite:!radio,currentIndex:9}}})}}));
    const result=await library.execute({action:'playback_queue',playlist:list,id:video(9),index:9});
    assert.ok(!Object.hasOwn(result,'playlist_size'));
    if(!radio)assert.equal(result.is_infinite,true);
  }
});

test('Index-only navigation requests the exact occurrence without inventing a video ID',async()=>{
  const calls=[];
  const library=new Library(async()=>({actions:{execute:async(path,args)=>{
    calls.push({path,args});return response([row(100,false,video(1)),row(103,true,video(1)),row(109)],{currentIndex:103});
  }}}));
  const result=await library.execute({action:'playback_at',playlist,index:103});
  assert.equal(calls[0].path,'/next');assert.equal(calls[0].args.playlistIndex,103);
  assert.equal(calls[0].args.playlistId,playlist);assert.ok(!Object.hasOwn(calls[0].args,'videoId'));
  assert.equal(result.video_id,video(1));assert.equal(result.index,103);
  assert.deepEqual(result.video_indices,[100,103,109]);
});

test('Index-only navigation requires a uniquely selected playable occurrence and explicit position proof',async()=>{
  for(const change of [
    (rows,panel)=>{delete rows[0].playlistPanelVideoRenderer.navigationEndpoint.watchEndpoint.index;delete panel.currentIndex;},
    (rows)=>{rows[0].playlistPanelVideoRenderer.selected=false;},
    (rows)=>{rows[1].playlistPanelVideoRenderer.selected=true;},
    (rows)=>{rows[0].playlistPanelVideoRenderer.isPlayable=false;},
    (rows,panel)=>{panel.currentIndex=4;},
    (rows,panel)=>{panel.isInfinite=true;},
  ]) {
    const rows=[row(3,true),row(4)];const panel={playlistId:playlist,contents:rows,currentIndex:3};change(rows,panel);
    const library=new Library(async()=>({actions:{execute:async()=>({success:true,data:{playlistPanelRenderer:panel}})}}));
    await assert.rejects(library.execute({action:'playback_at',playlist,index:3}),{code:'remote_playlist_unavailable'});
  }
});

test('Index-only navigation can use an explicit panel index when endpoints omit their indices',async()=>{
  const rows=[row(0),row(1,true)];for(const entry of rows)delete entry.playlistPanelVideoRenderer.navigationEndpoint.watchEndpoint.index;
  const library=new Library(async()=>({actions:{execute:async()=>response(rows,{currentIndex:21})}}));
  const result=await library.execute({action:'playback_at',playlist,index:21});
  assert.equal(result.video_id,video(1));assert.equal(result.window_start,20);
});

test('Index-only navigation rejects supplied video IDs and invalid indices before requesting upstream',async()=>{
  const library=new Library(async()=>({actions:{execute:async()=>{assert.fail('must not request');}}}));
  for(const patch of [{id:video(3)},{id:null},{id:false},{index:-1},{index:3.5},{index:'3'}])
    await assert.rejects(library.execute({action:'playback_at',playlist,index:3,...patch}),{code:'invalid_request'});
});

test('Native playback windows use the selected profile and preserve repeated sparse occurrences',async()=>{
  const profiles=[];
  const library=new Library(async profile=>{profiles.push(profile.id);return {actions:{execute:async()=>
    response([row(3,false,video(9)),row(9,true,video(9)),row(20,false,video(9))],{currentIndex:9})}}},
    [{id:'account',name:'Account',cookie:'synthetic'}]);
  const input={action:'playback_queue',profile:'account',id:video(9),playlist,index:9};
  const result=await library.execute(input);
  assert.deepEqual(profiles,['account']);assert.deepEqual(result.video_indices,[3,9,20]);
  assert.deepEqual(result.video_ids,[video(9),video(9),video(9)]);
  assert.equal(result.index,9);
  await assert.rejects(library.execute({...input,profile:'missing'}),{code:'unknown_profile'});
  await assert.rejects(library.execute({...input,index:8}),{code:'remote_playlist_unavailable'});
});

test('Both queue consumers reject contradictory endpoint indices and selected occurrences',async()=>{
  for(const action of ['playback_queue','remote_playlist']) {
    for(const corrupt of [
      rows=>{rows[0].playlistPanelVideoRenderer.navigationEndpoint.watchEndpoint.playlistIndex=7;},
      rows=>{rows[0].playlistPanelVideoRenderer.selected=false;rows[1].playlistPanelVideoRenderer.selected=true;}
    ]) {
      const rows=[row(0,true),row(1)];corrupt(rows);
      const library=new Library(async()=>({actions:{execute:async()=>response(rows)}}));
      await assert.rejects(library.execute({action,id:video(0),playlist,index:0}),{code:'remote_playlist_unavailable'});
    }
  }
});
test('WatchNext preserves global indices, repeats and bounded windows in the selected profile',async()=>{
  const calls=[]; const profiles=[];
  const library=new Library(async profile=>{profiles.push(profile.id);return {actions:{execute:async(path,args)=>{
    calls.push({path,args}); return response(Array.from({length:500},(_,i)=>row(i,i===250,i===250?video(1):video(i))),{currentIndex:250,totalVideos:'500'});
  }}}},[{id:'account',name:'Account',cookie:'synthetic'}]);
  const result=await library.execute({action:'remote_playlist',profile:'account',id:video(1),playlist,index:250});
  assert.deepEqual(profiles,['account']);assert.equal(calls[0].path,'/next');
  assert.equal(calls[0].args.playlistIndex,250);assert.equal(calls[0].args.playlistId,playlist);
  assert.equal(result.window_start,218);assert.equal(result.video_ids.length,128);assert.equal(result.video_ids[32],video(1));
  assert.equal(result.index,250);
  assert.equal(result.playlist_size,500);
});
test('TV panel, wrapped rows and absent endpoint indices retain the selected occurrence',async()=>{
  const rows=[row(100),row(101,true),row(102)];
  for(const item of rows)delete item.playlistPanelVideoRenderer.navigationEndpoint.watchEndpoint.index;
  rows[2]={playlistPanelVideoWrapperRenderer:{primaryRenderer:rows[2]}};
  const library=new Library(async()=>({actions:{execute:async()=>({success:true,data:{tv:{playlistPanelRenderer:{playlistId:playlist,contents:rows}}}})}}));
  const result=await library.execute({action:'remote_playlist',id:video(101),playlist,index:101});
  assert.equal(result.window_start,100);assert.deepEqual(result.video_ids,[video(100),video(101),video(102)]);
});
test('Deep unrelated video menus do not invalidate a usable queue',async()=>{
  const item=row(0,true);let menu=item.playlistPanelVideoRenderer;
  for(let i=0;i<60;i++){menu.menu={};menu=menu.menu;}
  const library=new Library(async()=>({actions:{execute:async()=>response([item,row(1)])}}));
  const result=await library.execute({action:'remote_playlist',id:video(0),playlist,index:0});
  assert.deepEqual(result.video_ids,[video(0),video(1)]);
});
test('Infinite radio resets are rebased at the requested occurrence without changing ordinary playlists',async()=>{
  const radio='RDsyntheticqueue';
  const rows=[row(0,true,video(20)),row(1,false,video(21))];
  for(const item of rows)item.playlistPanelVideoRenderer.navigationEndpoint.watchEndpoint.playlistId=radio;
  const library=new Library(async()=>({actions:{execute:async()=>({success:true,data:{playlistPanelRenderer:{playlistId:radio,currentIndex:0,isInfinite:true,contents:rows}}})}}));
  const result=await library.execute({action:'remote_playlist',id:video(20),playlist:radio,index:20});
  assert.equal(result.window_start,20);assert.equal(result.radio_rebased,true);assert.deepEqual(result.video_ids,[video(20),video(21)]);
});
test('Sparse queues and unavailable rows preserve original occurrence indices',async()=>{
  for (const contents of [[row(1,true),row(3)], [row(1,true),{playlistPanelVideoRenderer:{isPlayable:false}},row(3)]]) {
    const library=new Library(async()=>({actions:{execute:async()=>response(contents)}}));
    const result=await library.execute({action:'remote_playlist',id:video(1),playlist,index:1});
    assert.deepEqual(result.video_ids,[video(1),video(3)]);
    assert.deepEqual(result.video_indices,[1,3]);
    assert.equal(result.window_start,1);
  }
});
test('Malformed, conflicting and oversized queues fail without accepting arbitrary indices',async()=>{
  for(const value of [response([row(1,true),row(1)]),response([row(1,true),row(2,true)]),response([row(1,true)],{currentIndex:2}),
    response([row(1,true),{unknownRenderer:{videoId:video(2)}}]),response(Array.from({length:4097},(_,i)=>row(i,i===1))),
    {success:false,data:{}}, {success:true,data:{a:{playlistId:playlist,contents:[row(1,true)]},b:{playlistId:playlist,contents:[row(1,true)]}}}]) {
    const library=new Library(async()=>({actions:{execute:async()=>value}}));
    await assert.rejects(library.execute({action:'remote_playlist',id:video(1),playlist,index:1}),{code:'remote_playlist_unavailable'});
  }
  let calls=0; const library=new Library(async()=>({actions:{execute:async()=>{calls++;}}}));
  for(const index of [-1,1.5,100001,'1'])await assert.rejects(library.execute({action:'remote_playlist',id:video(1),playlist,index}),{code:'invalid_request'});
  assert.equal(calls,0);
  await assert.rejects(library.execute({action:'remote_playlist',profile:'missing',id:video(1),playlist,index:1}),{code:'unknown_profile'});
});
