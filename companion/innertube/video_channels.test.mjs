import test from 'node:test';
import assert from 'node:assert/strict';
import {hydrateWindowChannels} from './video_channels.mjs';
class Failure extends Error {constructor(code,message){super(message);this.code=code;}}
const id='abcdefghijk', channel='UC'+'a'.repeat(22);
test('verified names replace invalid metadata and retain duplicate occurrences',async()=>{
  let calls=0;
  const result=await hydrateWindowChannels({getBasicInfo:async key=>{calls++;return {basic_info:{id:key,channel_id:channel,author:'CAFÉ'}};}},
    {video_ids:[id,id],video_channels:[channel,channel],video_channel_names:['\u0001','']},Failure,true);
  assert.equal(calls,1);assert.deepEqual(result.video_channel_names,['CAFÉ','CAFÉ']);
  for(const author of ['', ' ', 'bad\nname', 'é'.repeat(128)])
    await assert.rejects(hydrateWindowChannels({getBasicInfo:async()=>({basic_info:{id,channel_id:channel,author}})},
      {video_ids:[id]},Failure,true),{code:'channel_unverified'});
  await assert.rejects(hydrateWindowChannels({getBasicInfo:async()=>({basic_info:{id,channel_id:'UC'+'b'.repeat(22),author:'Other'}})},
    {video_ids:[id],video_channels:[channel]},Failure,true),{code:'channel_unverified'});
});
