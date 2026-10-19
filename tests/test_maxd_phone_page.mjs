// SPDX-License-Identifier: GPL-3.0-or-later
import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import vm from 'node:vm';
const html=fs.readFileSync(new URL('../native/maxd/phone_remote_page.html',import.meta.url),'utf8');
test('phone remote keeps tokens out of URLs and renders content as text',()=>{
 const script=html.match(/<script>([\s\S]*?)<\/script>/)[1];
 let removed=false;
 const context=vm.createContext({URL,document:{getElementById:()=>({dataset:{}}),querySelectorAll:()=>[]},location:{hash:'#invite',pathname:'/'},history:{replaceState(){removed=true;}},fetch:()=>new Promise(()=>{}),setInterval(){}});
 vm.runInContext(script,context);assert.ok(removed);
 for(const input of ['abcdefghijk','https://youtu.be/abcdefghijk','https://www.youtube.com/watch?v=abcdefghijk&t=10','https://m.youtube.com/shorts/abcdefghijk','https://youtube.com/live/abcdefghijk'])
  assert.equal(vm.runInContext(`idFrom(${JSON.stringify(input)})`,context),'abcdefghijk');
 for(const input of ['https://youtube.com.evil.test/watch?v=abcdefghijk','https://evil.test/abcdefghijk','javascript:alert(1)','https://user:password@youtube.com/watch?v=abcdefghijk','https://youtube.com:444/watch?v=abcdefghijk','not a video'])
  assert.equal(vm.runInContext(`idFrom(${JSON.stringify(input)})`,context),'');
 assert.ok(!script.includes('innerHTML') && !script.includes('localStorage'));
 assert.ok(script.includes("credentials:'omit'") && script.includes('selection:s.selection'));
 assert.ok(script.includes('new Blob([reportText]') && script.includes("api('/report')"));
});
