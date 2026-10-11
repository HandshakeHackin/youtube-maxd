import test from 'node:test';
import assert from 'node:assert/strict';
import { Library } from './library.mjs';
const id='abcdefghijk',next='bcdefghijkl';
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
  for(const patch of [{id:'bad'},{playlist:'PLabcdefghijk'},{index:0},{direction:1}])
    await assert.rejects(library.execute({action:'playback_next',id,...patch}),{code:'invalid_request'});
});
