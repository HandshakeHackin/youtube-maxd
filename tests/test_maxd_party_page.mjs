import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import vm from 'node:vm';
test('the served party page accepts only supported YouTube video links',()=>{
  const html=fs.readFileSync(new URL('../native/maxd/party_page.html',import.meta.url),'utf8');
  const script=html.match(/<script>([\s\S]*?)<\/script>/)[1];
  const context=vm.createContext({URL,document:{getElementById:()=>({})},location:{hash:'#test',pathname:'/'},history:{replaceState(){}},fetch:()=>new Promise(()=>{}),setInterval(){}});
  vm.runInContext(script,context);
  for(const input of ['abcdefghijk','https://youtu.be/abcdefghijk','https://www.youtube.com/watch?v=abcdefghijk&t=10','https://m.youtube.com/shorts/abcdefghijk','https://youtube.com/live/abcdefghijk'])
    assert.equal(vm.runInContext(`idFrom(${JSON.stringify(input)})`,context),'abcdefghijk');
  for(const input of ['https://youtube.com.evil.test/watch?v=abcdefghijk','https://evil.test/abcdefghijk','javascript:alert(1)','https://user:password@youtube.com/watch?v=abcdefghijk','https://youtube.com/@channel','not a video'])
    assert.equal(vm.runInContext(`idFrom(${JSON.stringify(input)})`,context),'');
  assert.ok(!script.includes('innerHTML'));
});
