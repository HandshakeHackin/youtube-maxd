import test from 'node:test';
import assert from 'node:assert/strict';
import {channelWindow} from './channel_window.mjs';
const channel='UC'+'a'.repeat(22), id='abcdefghijk';
const input={channel,id,index:25};
class Failure extends Error {constructor(code,message){super(message);this.code=code;}}
const normalize=node=>node.id?node:null;
function client(rows,size=24) {
  function page(offset) {return {videos:rows.slice(offset,offset+size),has_continuation:offset+size<rows.length,
    async getContinuation(){return page(offset+size);}};}
  return {async getChannel(key){assert.equal(key,channel);return {async getVideos(){return page(0);}};}};
}
test('channel windows preserve duplicate IDs and sparse positions across upstream pages',async()=>{
  const rows=Array.from({length:100},()=>({id}));rows[24]={};rows[26]={id,is_upcoming:true};
  const result=await channelWindow(client(rows),input,normalize,Failure);
  assert.equal(result.index,25);assert.equal(result.anchor_index,25);
  assert.equal(result.anchor_video_id,id);assert.equal(result.playlist,'channel:'+channel);
  assert.ok(result.video_indices.includes(23)&&result.video_indices.includes(27));
  assert.ok(!result.video_indices.includes(24)&&!result.video_indices.includes(26));
  assert.ok(result.video_ids.every(value=>value===id));
  assert.deepEqual(result.video_channels,result.video_ids.map(()=>channel));
});
test('indexed channel navigation validates the original anchor and skips only in the requested direction',async()=>{
  const rows=Array.from({length:50},()=>({id}));rows[30]={};rows[31]={};
  for(const [direction,index] of [[1,32],[-1,29]]) {
    const result=await channelWindow(client(rows),{...input,target:30,direction},normalize,Failure);
    assert.equal(result.index,index);assert.equal(result.requested_index,30);
    assert.equal(result.skipped_unavailable,Math.abs(index-30));assert.equal(result.playlist_size,50);
  }
  await assert.rejects(channelWindow(client(rows),{...input,target:30},normalize,Failure),{code:'channel_window_unavailable'});
  rows[25]={id:'12345678901'};
  await assert.rejects(channelWindow(client(rows),{...input,target:30,direction:1},normalize,Failure),{code:'channel_changed'});
});
test('channel window retains a bounded neighborhood when seeking across a large group',async()=>{
  const rows=Array.from({length:500},()=>({id}));
  for(const [anchor,target] of [[400,5],[5,400]]) {
    const result=await channelWindow(client(rows),{...input,index:anchor,target},normalize,Failure);
    assert.ok(result.video_ids.length<=128);assert.equal(result.index,target);
    assert.ok(result.video_indices.includes(target));assert.equal(result.anchor_index,anchor);
  }
});
test('invalid channel requests fail before opening a channel',async()=>{
  const upstream={getChannel(){throw Error('must not be called');}};
  for(const patch of [{channel:'bad'},{index:true},{target:-1},{id:'bad'},{playlist:'PLabcdefghijk'},{direction:2}])
    await assert.rejects(channelWindow(upstream,{...input,...patch},normalize,Failure),{code:'invalid_request'});
});
test('empty continuation loops terminate and finite one-video groups report their size',async()=>{
  const loop={videos:[],has_continuation:true,async getContinuation(){return this;}};
  await assert.rejects(channelWindow({async getChannel(){return {async getVideos(){return loop;}};}},input,normalize,Failure),{code:'channel_window_unavailable'});
  const result=await channelWindow(client([{id}]),{...input,index:0},normalize,Failure);
  assert.deepEqual(result.video_indices,[0]);assert.equal(result.playlist_size,1);
});
test('unavailable-only channel tails prove exhaustion while retaining the original anchor',async()=>{
  for(const direction of [-1,1]) {
    const rows=Array.from({length:50},()=>({}));rows[25]={id};
    const target=25+direction;
    const result=await channelWindow(client(rows),{...input,target,direction},normalize,Failure);
    assert.equal(result.unavailable_only,true);assert.equal(result.exhausted,true);
    assert.equal(result.anchor_video_id,id);assert.equal(result.anchor_index,25);
    assert.equal(result.requested_index,target);assert.equal(result.playlist_size,50);
    assert.equal(result.skipped_unavailable,direction>0?24:25);
    rows[25]={id:'12345678901'};
    await assert.rejects(channelWindow(client(rows),{...input,target,direction},normalize,Failure),{code:'channel_changed'});
  }
});
