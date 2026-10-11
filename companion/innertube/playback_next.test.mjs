import test from 'node:test';
import assert from 'node:assert/strict';
import { Library } from './library.mjs';
const id='abcdefghijk',next='bcdefghijkl';
test('requested next-channel verification uses the selected endpoint identity and profile client',async()=>{
  let calls=0,details={id:next,channel_id:'UC'+'a'.repeat(22)};
  const library=new Library(async profile=>({getBasicInfo:async video=>{assert.equal(profile.id,'account');assert.equal(video,next);calls++;return {basic_info:details};},
    actions:{execute:async()=>({success:true,data:{contents:{twoColumnWatchNextResults:{autoplay:{autoplay:{sets:[{autoplayVideo:{watchEndpoint:{videoId:next}}}]}}}}}})}}),[{id:'account',cookie:'synthetic'}]);
  const request={action:'playback_next',id,profile:'account'};
  assert.equal((await library.execute(request)).next_channel_id,undefined);assert.equal(calls,0);
  assert.equal((await library.execute({...request,require_channel:true})).next_channel_id,details.channel_id);assert.equal(calls,1);
  for(const invalid of [{id,channel_id:details.channel_id},{id:next,channel_id:'bad'},{id:next}]) {
    details=invalid;await assert.rejects(library.execute({...request,require_channel:true}),{code:'playback_next_unavailable'});
  }
  await assert.rejects(library.execute({...request,require_channel:'true'}),{code:'invalid_request'});
});
test('Next playback uses the profile-owned autoplay endpoint rather than suggestion cards',async()=>{
  const calls=[];
  const library=new Library(async profile=>({actions:{execute:async(path,args)=>{
    calls.push({profile:profile.id,path,args});
    return {success:true,data:{contents:{twoColumnWatchNextResults:{
      secondaryResults:{videoId:'12345678901'},autoplay:{autoplay:{sets:[{autoplayVideo:{watchEndpoint:{videoId:next}},nextButtonVideo:{watchEndpoint:{videoId:'12345678901'}}}]}}
    }}}};
  }}}),[{id:'account',name:'Account',cookie:'synthetic'}]);
  assert.deepEqual(await library.execute({action:'playback_next',profile:'account',id}),{api_version:1,video_id:id,next_video_id:next});
  assert.equal(calls[0].profile,'account');assert.equal(calls[0].path,'/next');assert.equal(calls[0].args.videoId,id);
});
test('Missing and self-referencing autoplay endpoints stop without substituting a card',async()=>{
  for(const endpoint of [undefined,{watchEndpoint:{videoId:id}}]) {
    const library=new Library(async()=>({actions:{execute:async()=>({success:true,data:{contents:{twoColumnWatchNextResults:{autoplay:{autoplay:{sets:[{autoplayVideo:endpoint}]}}}}}})}}));
    assert.equal((await library.execute({action:'playback_next',id})).next_video_id,null);
  }
});
test('Next playback rejects invalid identity and playlist context before upstream execution',async()=>{
  const library=new Library(async()=>({actions:{execute:async()=>assert.fail('unexpected upstream request')}}));
  for(const patch of [{id:'bad'},{playlist:'PLabcdefghijk'},{playlist:false},{index:0},{direction:1}])
    await assert.rejects(library.execute({action:'playback_next',id,...patch}),{code:'invalid_request'});
});

test('Playlist next lookup preserves request occurrence and a distinct repeated target occurrence',async()=>{
  const playlist='RDabcdefghijk',calls=[];
  let targetIndex=10;
  const library=new Library(async()=>({actions:{execute:async(path,args)=>{
    calls.push(args);return {success:true,data:{contents:{twoColumnWatchNextResults:{autoplay:{autoplay:{sets:[{
      autoplayVideo:{watchEndpoint:{videoId:id,playlistId:playlist,index:targetIndex}}
    }]}}}}}};
  }}}));
  const request={action:'playback_next',id,playlist,index:9};
  const result=await library.execute(request);
  assert.equal(calls[0].playlistId,playlist);assert.equal(calls[0].playlistIndex,9);
  assert.equal(result.next_video_id,id);assert.deepEqual(result.playback_context,{playlist,index:10});
  targetIndex=9;assert.equal((await library.execute(request)).next_video_id,null);
});

test('name-filtered recommendations verify author metadata and reject missing names',async()=>{
  let author='CAFÉ';
  const library=new Library(async()=>({getBasicInfo:async key=>({basic_info:{id:key,channel_id:'UC'+'a'.repeat(22),author}}),
    actions:{execute:async()=>({success:true,data:{contents:{twoColumnWatchNextResults:{autoplay:{autoplay:{sets:[{autoplayVideo:{watchEndpoint:{videoId:next}}}]}}}}}})}}));
  const request={action:'playback_next',id,require_channel:true,require_name:true};
  assert.equal((await library.execute(request)).next_channel_name,author);
  author='';await assert.rejects(library.execute(request),{code:'playback_next_unavailable'});
  for(const patch of [{require_channel:false},{require_name:1}])
    await assert.rejects(library.execute({...request,...patch}),{code:'invalid_request'});
});
