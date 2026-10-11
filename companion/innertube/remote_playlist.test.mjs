import test from 'node:test';
import assert from 'node:assert/strict';
import { Library } from './library.mjs';
const video = i => String(i).padStart(11, '0');
const playlist = 'PLsyntheticqueue';
const row = (i, selected=false, id=video(i)) => ({playlistPanelVideoRenderer:{videoId:id, selected,
  navigationEndpoint:{watchEndpoint:{videoId:id, playlistId:playlist, index:i}}}});
const response = (contents, extra={}) => ({success:true,data:{contents:{twoColumnWatchNextResults:{playlist:{playlist:{playlistId:playlist,contents,...extra}}}}}});
test('WatchNext preserves global indices, repeats and bounded windows in the selected profile',async()=>{
  const calls=[]; const profiles=[];
  const library=new Library(async profile=>{profiles.push(profile.id);return {actions:{execute:async(path,args)=>{
    calls.push({path,args}); return response(Array.from({length:500},(_,i)=>row(i,i===250,i===250?video(1):video(i))),{currentIndex:250});
  }}}},[{id:'account',name:'Account',cookie:'synthetic'}]);
  const result=await library.execute({action:'remote_playlist',profile:'account',id:video(1),playlist,index:250});
  assert.deepEqual(profiles,['account']);assert.equal(calls[0].path,'/next');
  assert.equal(calls[0].args.playlistIndex,250);assert.equal(calls[0].args.playlistId,playlist);
  assert.equal(result.window_start,218);assert.equal(result.video_ids.length,128);assert.equal(result.video_ids[32],video(1));
  assert.equal(result.index,250);
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
