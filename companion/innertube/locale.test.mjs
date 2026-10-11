import test from 'node:test';
import assert from 'node:assert/strict';
import {Library,LibraryError} from './library.mjs';
import {requestLocale,languages,countries} from './locale.mjs';
import {createProfileClient} from './device_auth.mjs';
const pad=n=>String(n).padStart(11,'0');

test('content locale accepts only the reference languages and countries',()=>{
  assert.equal(languages.size,49);assert.equal(countries.size,89);
  assert.deepEqual(requestLocale({},LibraryError),{lang:'',location:'',key:''});
  assert.deepEqual(requestLocale({hl:'',gl:''},LibraryError),{lang:'',location:'',key:''});
  assert.deepEqual(requestLocale({hl:'fr',gl:'CA'},LibraryError),{lang:'fr',location:'CA',key:'|fr|CA'});
  for(const [tag,sent] of [['he','iw'],['nb','no'],['mo','ro'],['pt-BR','pt'],['pt-PT','pt-PT'],['en-NZ','en-GB'],['zh-TW','zh-TW']])
    assert.equal(requestLocale({hl:tag},LibraryError).lang,sent);
  // A shipped language without a YouTube interface keeps the default client.
  assert.equal(requestLocale({hl:'tjl'},LibraryError).key,'');
  assert.equal(requestLocale({hl:'tjl',gl:'TH'},LibraryError).key,'||TH');
  for(const patch of [{hl:'xx'},{hl:'EN'},{hl:'en_US'},{hl:7},{hl:'__proto__'},{gl:'us'},{gl:'XX'},{gl:'GB-SCT'},{gl:['US']},{hl:'fr|x'}])
    assert.throws(()=>requestLocale(patch,LibraryError),{code:'invalid_locale'});
});

test('each locale has its own client and a cursor continues with the client that opened it',async()=>{
  const created=[];
  const feed=(tag,offset)=>({videos:Array.from({length:30},(_,i)=>({id:pad(offset+i),title:tag})),
    has_continuation:!offset,async getContinuation(){return feed(tag,100);}});
  const library=new Library(async(profile,locale)=>{
    const tag=locale?`${locale.lang}/${locale.location}`:'default';created.push(tag);
    return {search:async()=>feed(tag,0)};
  });
  const plain=await library.execute({action:'search',query:'cats'});
  const french=await library.execute({action:'search',query:'cats',hl:'fr',gl:'BE'});
  const hebrew=await library.execute({action:'search',query:'cats',hl:'he'});
  await library.execute({action:'search',query:'dogs',hl:'fr',gl:'BE'});
  await library.execute({action:'search',query:'dogs',hl:'',gl:''});
  assert.deepEqual(created,['default','fr/BE','iw/']);
  assert.deepEqual([plain,french,hebrew].map(page=>page.items[0].title),['default','fr/BE','iw/']);
  // The continuation ignores the locale of the continue request itself.
  let page=french;
  for(let i=0;i<3 && page.has_more;++i)page=await library.execute({action:'continue',cursor:page.cursor,hl:'de',gl:'DE'});
  assert.equal(page.items.at(-1).title,'fr/BE');assert.deepEqual(created,['default','fr/BE','iw/']);
  await assert.rejects(library.execute({action:'search',query:'cats',hl:'xx'}),{code:'invalid_locale'});
  await assert.rejects(library.playback({action:'catalog',profile:'guest',kind:'search',query:'cats',gl:'ZZ'}),{code:'invalid_locale'});
  const routed=await library.playback({action:'catalog',profile:'guest',kind:'search',query:'cats',hl:'ja',gl:'JP'});
  assert.equal(routed.items[0].title,'ja/JP');
  for(let i=0;i<40;++i)await library.execute({action:'search',query:'cats',hl:'fr',gl:Array.from(countries)[i]});
  assert.ok(Array.from(library.clients.keys()).filter(key=>key.includes('|')).length<=16);
  assert.ok(library.clients.has('guest'));
});

test('the profile client sends the locale to the pinned session options only when requested',async()=>{
  const seen=[];
  const Innertube={create:async options=>{seen.push(options);return {session:{on(){},signIn:async()=>{}}};}};
  await createProfileClient({id:'guest',cookie:''},Innertube,fetch);
  await createProfileClient({id:'guest',cookie:''},Innertube,fetch,()=>{},{lang:'iw',location:'IL'});
  await createProfileClient({id:'guest',cookie:''},Innertube,fetch,()=>{},{lang:'',location:'JP'});
  assert.ok(!('lang' in seen[0]) && !('location' in seen[0]));
  assert.deepEqual([seen[1].lang,seen[1].location],['iw','IL']);
  assert.ok(!('lang' in seen[2]));assert.equal(seen[2].location,'JP');
});
