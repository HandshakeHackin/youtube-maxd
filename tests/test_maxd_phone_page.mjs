// SPDX-License-Identifier: GPL-3.0-or-later
import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import vm from 'node:vm';
const html=fs.readFileSync(new URL('../native/maxd/phone_remote_page.html',import.meta.url),'utf8');
const script=html.match(/<script>([\s\S]*?)<\/script>/)[1];
const flush=()=>new Promise(resolve=>setImmediate(resolve));
function deferred(){let resolve,reject;const promise=new Promise((yes,no)=>{resolve=yes;reject=no;});return {promise,resolve,reject};}
function response(value,status=200){return {ok:status>=200&&status<300,status,json:async()=>value};}
function page({invite='',token='test-session',handler}={}){
 const ids=new Map(),timers=new Map(),calls=[];let serial=0,removed=false;
 const document={activeElement:null};
 class Element{
  constructor(tag='div',id=''){this.tag=tag;this.id=id;this.dataset={};this.children=[];this.hidden=false;this.disabled=false;this.value='';this.textContent='';this.replacements=0;this.attributes={};}
  get disabled(){return this._disabled||false;}
  set disabled(value){this._disabled=value;if(value&&document.activeElement===this)document.activeElement=document.body;}
  append(...items){this.children.push(...items);}
  replaceChildren(...items){this.children=items;++this.replacements;}
  setAttribute(k,v){this.attributes[k]=v;}
  focus(){document.activeElement=this;}
  click(){if(!this.disabled)this.onclick?.();}
 }
 for(const id of ['status','controls','reconnect','playback','queue','query','video','search','add','capture','report','download','rewind','forward'])ids.set(id,new Element('div',id));
 document.body=new Element('body');
 const controls=ids.get('controls');controls.hidden=true;
 for(const id of ['queue','capture','rewind','forward','search','add'])controls.append(ids.get(id));
 for(const action of ['pause','resume']){const b=new Element('button');b.dataset.action=action;controls.append(b);}
 for(const key of ['UP','DOWN','LEFT','RIGHT','ENTER','BACK']){const b=new Element('button');b.dataset.key=key;controls.append(b);}
 for(const id of ['capture','rewind','forward','reconnect','download'])ids.get(id).tag='button';
 ids.get('search').append(new Element('button'));ids.get('add').append(new Element('button'));
 const walk=e=>[e,...e.children.flatMap(walk)];
 document.getElementById=id=>ids.get(id);
 document.createElement=tag=>new Element(tag);
 document.querySelectorAll=selector=>walk(controls).filter(e=>selector==='#controls button'?e.tag==='button':selector==='[data-action]'?!!e.dataset.action:selector==='[data-key]'?!!e.dataset.key:selector==='[data-action],#rewind,#forward'?!!e.dataset.action||e.id==='rewind'||e.id==='forward':false);
 let state={active:true,paused:false,busy:false,live:false,position:30,duration:4000,selection:1,sequence:0,queue:[]};
 const store=new Map(token?[['maxd-remote',token]]:[]);
 const h={ids,timers,calls,document,get removed(){return removed;},state,handler:handler||((path)=>response(path==='/pair'?{token:'test-paired'}:path==='/command'?{ok:true}:path==='/report'?{version:'test'}:state))};
 const context=vm.createContext({URL,AbortController,document,location:{hash:invite,pathname:'/'},history:{replaceState(){removed=true;}},sessionStorage:{getItem:k=>store.get(k),setItem:(k,v)=>store.set(k,v),removeItem:k=>store.delete(k)},
  setTimeout(fn,delay){timers.set(++serial,{fn,delay});return serial;},clearTimeout(id){timers.delete(id);},setInterval(){},
  fetch(path,options){calls.push({path,body:JSON.parse(options.body),options});const pending=Promise.resolve(h.handler(path,options));
   return new Promise((resolve,reject)=>{const abort=()=>{const e=Error('aborted');e.name='AbortError';reject(e);};options.signal.addEventListener('abort',abort,{once:true});pending.then(resolve,reject).finally(()=>options.signal.removeEventListener('abort',abort));});}
 });
 vm.runInContext(script,context);h.eval=source=>vm.runInContext(source,context);h.store=store;return h;
}
test('phone remote keeps tokens out of URLs and renders content as text',async()=>{
 const h=page({invite:'#invite',handler:()=>new Promise(()=>{})});assert.ok(h.removed);
 for(const input of ['abcdefghijk','https://youtu.be/abcdefghijk','https://www.youtube.com/watch?v=abcdefghijk&t=10','https://m.youtube.com/shorts/abcdefghijk','https://youtube.com/live/abcdefghijk'])assert.equal(h.eval(`idFrom(${JSON.stringify(input)})`),'abcdefghijk');
 for(const input of ['https://youtube.com.evil.test/watch?v=abcdefghijk','https://evil.test/abcdefghijk','javascript:alert(1)','https://user:password@youtube.com/watch?v=abcdefghijk','https://youtube.com:444/watch?v=abcdefghijk','not a video'])assert.equal(h.eval(`idFrom(${JSON.stringify(input)})`),'');
 assert.ok(!script.includes('innerHTML')&&!script.includes('localStorage'));
 assert.ok(script.includes("credentials:'omit'")&&script.includes('selection:s.selection'));
 assert.ok(script.includes('new Blob([reportText]')&&script.includes("api('/report')"));
 assert.equal(h.calls[0].options.headers.Authorization,undefined);
 assert.equal(h.eval('clock(3661)'),'1:01:01');assert.equal(h.eval('clock(59)'),'0:59');assert.equal(h.eval('clock(Infinity)'),'0:00');
});
test('rapid taps send one control and restore buttons when it finishes',async()=>{
 const h=page();await flush();const gate=deferred();h.handler=path=>path==='/state'?gate.promise:response({ok:true});
 const button=h.document.querySelectorAll('[data-action]')[0];button.focus();
 const first=h.eval("command('pause')"),second=h.eval("command('resume')");assert.equal(await second,false);
 assert.equal(h.document.querySelectorAll('[data-action]')[0].disabled,true);
 gate.resolve(response(h.state));assert.equal(await first,true);
 assert.equal(h.calls.filter(c=>c.path==='/command').length,1);
 assert.equal(h.calls.find(c=>c.path==='/command').body.action,'pause');
 assert.equal(h.document.querySelectorAll('[data-action]')[0].disabled,false);assert.equal(h.document.activeElement,button);
});
test('media controls do not silently retarget a video that changed',async()=>{
 const h=page();await flush();h.handler=()=>response({...h.state,selection:2});
 assert.equal(await h.eval("command('seek','',60)"),false);
 assert.equal(h.calls.filter(c=>c.path==='/command').length,0);assert.match(h.ids.get('status').textContent,/video changed/);
});
test('an old poll cannot overwrite the state after a command',async()=>{
 const h=page();await flush();const gate=deferred();let count=0;
 h.handler=path=>path==='/command'?response({ok:true}):++count===1?gate.promise:response({...h.state,paused:true});
 const poll=h.eval('refresh()');await h.eval("command('pause')");gate.resolve(response({...h.state,paused:false}));await poll;
 assert.equal(h.eval('state.paused'),true);
});
test('an old failed poll cannot disconnect a successful command',async()=>{
 const h=page();await flush();const gate=deferred();let count=0;
 h.handler=path=>path==='/command'?response({ok:true}):++count===1?gate.promise:response(h.state);
 const poll=h.eval('refresh()');await h.eval("command('pause')");gate.reject(Error('old request failed'));await poll;
 assert.equal(h.eval('connected'),true);assert.equal(h.ids.get('status').dataset.error,'false');
});
test('unchanged queue polls keep buttons and focus; changed rows restore focus',async()=>{
 const h=page();await flush();h.state.queue=[{id:'abcdefghijk',title:'First'}];await h.eval('refresh()');
 const queue=h.ids.get('queue'),button=queue.children[0].children[1].children[1];button.focus();const replacements=queue.replacements;
 await h.eval('refresh()');assert.equal(queue.replacements,replacements);assert.equal(h.document.activeElement,button);
 h.state.queue[0].title='Updated';await h.eval('refresh()');assert.equal(h.document.activeElement.dataset.queueAction,'remove');assert.equal(h.document.activeElement.dataset.video,'abcdefghijk');
});
test('a timed out command is not replayed on reconnect',async()=>{
 const h=page();await flush();h.handler=path=>path==='/command'?new Promise(()=>{}):response(h.state);
 const control=h.eval("command('pause')");await flush();for(const timer of [...h.timers.values()])timer.fn();
 assert.equal(await control,false);assert.match(h.ids.get('status').textContent,/timed out/);assert.equal(h.ids.get('reconnect').hidden,false);
 h.handler=()=>response(h.state);await h.eval('refresh()');assert.equal(h.ids.get('reconnect').hidden,true);
 assert.equal(h.calls.filter(c=>c.path==='/command').length,1);
});
test('expired session clears cached report and requires a new QR',async()=>{
 const h=page();await flush();h.eval("reportText='old report';$('report').textContent=reportText;$('download').hidden=false");
 h.handler=()=>response({error:'expired'},401);await h.eval('refresh()');
 assert.equal(h.eval('reportText'),'');assert.equal(h.store.has('maxd-remote'),false);assert.equal(h.ids.get('controls').hidden,true);assert.equal(h.ids.get('download').hidden,true);assert.match(h.ids.get('status').textContent,/new QR/);
});
test('successful search clears sent text without erasing a new draft',async()=>{
 const h=page();await flush();h.ids.get('query').value='first';await h.eval("command('search','first')");assert.equal(h.ids.get('query').value,'');
 const gate=deferred();h.ids.get('query').value='sent';let count=0;h.handler=path=>path==='/command'?response({ok:true}):++count===1?gate.promise:response(h.state);
 const control=h.eval("command('search','sent')");h.ids.get('query').value='new draft';gate.resolve(response(h.state));await control;assert.equal(h.ids.get('query').value,'new draft');
});
