import test from 'node:test';
import assert from 'node:assert/strict';
import { Library } from './library.mjs';
const video = i => String(i).padStart(11, '0');
const playlist = 'PLsyntheticqueue';
const row = (i, selected=false, id=video(i)) => ({playlistPanelVideoRenderer:{videoId:id, selected,
  navigationEndpoint:{watchEndpoint:{videoId:id, playlistId:playlist, index:i}}}});
const response = (contents, extra={}) => ({success:true,data:{contents:{twoColumnWatchNextResults:{playlist:{playlist:{playlistId:playlist,contents,...extra}}}}}});

test('required window channels hydrate missing metadata once per video on the owning profile',async()=>{
  const rows=[row(0,true),row(1),row(2,false,video(1)),row(3)],calls=[];
  const known='UC'+'a'.repeat(22),fetched='UC'+'b'.repeat(22);
  rows[0].playlistPanelVideoRenderer.shortBylineText={runs:[{navigationEndpoint:{browseEndpoint:{browseId:known}}}]};
  const library=new Library(async profile=>({actions:{execute:async()=>response(rows,{currentIndex:0})},getBasicInfo:async id=>{
    assert.equal(profile.id,'account');calls.push(id);return {basic_info:{id,channel_id:fetched}};
  }}),[{id:'account',cookie:'synthetic'}]);
  const input={action:'playback_queue',profile:'account',playlist,index:0,id:video(0)};
  await library.execute(input);assert.equal(calls.length,0);
  const result=await library.execute({...input,require_channel:true});
  assert.deepEqual(calls.sort(),[video(1),video(3)]);
  assert.deepEqual(result.video_channels,[known,fetched,fetched,fetched]);
  assert.deepEqual(result.video_ids,[video(0),video(1),video(1),video(3)]);
});

test('window channel lookup bounds concurrency and rejects identity mismatches without partial results',async()=>{
  const rows=Array.from({length:20},(_,i)=>row(i,i===0));let active=0,maximum=0,calls=0,bad=false;
  const library=new Library(async()=>({actions:{execute:async()=>response(rows,{currentIndex:0})},getBasicInfo:async id=>{
    ++calls;++active;maximum=Math.max(maximum,active);await new Promise(resolve=>setImmediate(resolve));--active;
    return {basic_info:{id:bad?'wrong':id,channel_id:'UC'+'a'.repeat(22)}};
  }}));
  const input={action:'playback_queue',playlist,index:0,id:video(0),require_channel:true};
  const result=await library.execute(input);assert.equal(result.video_channels.length,20);assert.equal(calls,20);assert.equal(maximum,4);
  calls=0;bad=true;await assert.rejects(library.execute(input),{code:'channel_unverified'});
  assert.equal(active,0);assert.equal(calls,4);
  await assert.rejects(library.execute({...input,require_channel:1}),{code:'invalid_request'});
});

test('playlist windows retain channel IDs at their exact duplicate-video occurrences',async()=>{
  const rows=[row(0,true),row(1,false,video(0)),row(2)];
  rows[0].playlistPanelVideoRenderer.shortBylineText={runs:[{navigationEndpoint:{browseEndpoint:{browseId:'UC'+'a'.repeat(22)}}}]};
  rows[1].playlistPanelVideoRenderer.longBylineText={runs:[{navigationEndpoint:{browseEndpoint:{browseId:'UC'+'b'.repeat(22)}}}]};
  rows[2].playlistPanelVideoRenderer.shortBylineText={runs:[{navigationEndpoint:{browseEndpoint:{browseId:'invalid'}}}]};
  const library=new Library(async()=>({actions:{execute:async()=>response(rows,{currentIndex:0})}}));
  const result=await library.execute({action:'playback_queue',playlist,index:0,id:video(0)});
  assert.deepEqual(result.video_channels,['UC'+'a'.repeat(22),'UC'+'b'.repeat(22),'']);
  assert.deepEqual(result.video_ids,[video(0),video(0),video(2)]);
});

test('Directional lookup skips only consecutive confirmed unavailable occurrences',async()=>{
  for(const direction of [-1,1]) {
    const rows=[row(1),row(2),row(3,true),row(4),row(5)];
    for(const offset of [1,2,3])rows[offset].playlistPanelVideoRenderer.isPlayable=false;
    const library=new Library(async()=>({actions:{execute:async()=>response(rows,{currentIndex:3})}}));
    const result=await library.execute({action:'playback_at',playlist,index:3,direction});
    assert.equal(result.index,direction===1?5:1);
    assert.equal(result.requested_index,3);assert.equal(result.skipped_unavailable,2);
    assert.equal(result.video_id,video(result.index));
    await assert.rejects(library.execute({action:'playback_at',playlist,index:3}),{code:'remote_playlist_unavailable'});
  }
  const rows=[row(3,true),row(5)];rows[0].playlistPanelVideoRenderer.isPlayable=false;
  const library=new Library(async()=>({actions:{execute:async()=>response(rows,{currentIndex:3})}}));
  await assert.rejects(library.execute({action:'playback_at',playlist,index:3,direction:1}),{code:'remote_playlist_unavailable'});
});

test('Directional lookup rejects malformed directions and non-indexed use',async()=>{
  const library=new Library(async()=>({actions:{execute:async()=>{assert.fail('must not request');}}}));
  for(const direction of [null,true,'1',2,-2,0.5])
    await assert.rejects(library.execute({action:'playback_at',playlist,index:3,direction}),{code:'invalid_request'});
  await assert.rejects(library.execute({action:'playback_queue',playlist,index:3,id:video(3),direction:1}),{code:'invalid_request'});
});

test('Unavailable-only windows continue monotonically or end at an explicit finite bound',async()=>{
  for(const [indices,selectedIndex,direction,size,next] of [
    [[3,4,5],3,1,8,6],[[3,4,5],5,-1,8,2],
    [[3,4,5],3,1,6,null],[[0,1,2],2,-1,6,null]
  ]) {
    const rows=indices.map(i=>{const entry=row(i,i===selectedIndex);entry.playlistPanelVideoRenderer.isPlayable=false;return entry;});
    const library=new Library(async()=>({actions:{execute:async()=>response(rows,{currentIndex:selectedIndex,totalVideos:size})}}));
    const result=await library.execute({action:'playback_at',playlist,index:selectedIndex,direction});
    assert.equal(result.unavailable_only,true);assert.equal(result.requested_index,selectedIndex);
    assert.equal(result.skipped_unavailable,3);assert.equal(result.playlist_size,size);
    assert.equal(result.exhausted,next===null);
    if(next===null)assert.ok(!Object.hasOwn(result,'next_index'));else assert.equal(result.next_index,next);
    assert.ok(!Object.hasOwn(result,'video_id'));
  }
  for(const totalVideos of [undefined,'unknown',5]) {
    const rows=[row(3,true),row(4),row(5)];for(const entry of rows)entry.playlistPanelVideoRenderer.isPlayable=false;
    const library=new Library(async()=>({actions:{execute:async()=>response(rows,{currentIndex:3,totalVideos})}}));
    await assert.rejects(library.execute({action:'playback_at',playlist,index:3,direction:1}),{code:'remote_playlist_unavailable'});
  }
});

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
