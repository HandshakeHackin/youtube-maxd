import test from 'node:test';
import assert from 'node:assert/strict';
import {Library} from './library.mjs';
test('local family slots isolate signed-out clients and cannot borrow OAuth credentials',async()=>{
  const seen=[];
  const library=new Library(async profile=>{seen.push(structuredClone(profile));return {identity:profile.id};},[{id:'parent',name:'Parent',cookie:'synthetic'}]);
  const profiles=(await library.execute({action:'profiles'})).profiles;
  const family=profiles.filter(p=>p.id.startsWith('maxd_family_'));
  assert.equal(family.length,4);assert.ok(family.every(p=>p.account_configured===false));
  const one=await library.client('maxd_family_1'),two=await library.client('maxd_family_2');
  assert.notEqual(one,two);assert.equal(await library.client('maxd_family_1'),one);
  assert.ok(seen.every(p=>p.cookie==='' && p.oauth===undefined));
  await assert.rejects(library.playback({action:'watch',profile:'maxd_family_1'}),{code:'playback_account_context'});
  await assert.rejects(library.client('maxd_family_5'),{code:'unknown_profile'});
  assert.throws(()=>new Library(()=>{},[{id:'maxd_family_1',cookie:'synthetic'}]),{code:'invalid_profiles'});
});
