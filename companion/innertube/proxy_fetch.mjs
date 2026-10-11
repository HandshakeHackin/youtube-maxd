// HTTPS requests through a host-configured HTTP(S) proxy: one CONNECT tunnel
// per request, TLS to the target inside it. Never falls back to a direct
// connection and never reports the proxy address or its credentials.
// SPDX-License-Identifier: GPL-3.0-or-later
import http from 'node:http';
import https from 'node:https';
import tls from 'node:tls';
import zlib from 'node:zlib';
import {Readable} from 'node:stream';

export function proxySettings(value) {
  let url;
  try {url=new URL(value);} catch {throw new Error('invalid proxy');}
  if(!['http:','https:'].includes(url.protocol) || !url.hostname || !['','/'].includes(url.pathname) || url.search || url.hash)
    throw new Error('invalid proxy');
  const user=decodeURIComponent(url.username),password=decodeURIComponent(url.password);
  return {secure:url.protocol==='https:',host:url.hostname.replace(/^\[|\]$/g,''),
    port:Number(url.port) || (url.protocol==='https:'?443:80),
    authorization:user || password?'Basic '+Buffer.from(`${user}:${password}`).toString('base64'):''};
}
function tunnel(settings,host,port,signal) {
  return new Promise((resolve,reject)=>{
    const request=(settings.secure?https:http).request({host:settings.host,port:settings.port,method:'CONNECT',
      path:`${host}:${port}`,agent:false,signal,
      headers:{Host:`${host}:${port}`,...(settings.authorization?{'Proxy-Authorization':settings.authorization}:{})}});
    request.once('connect',(response,socket,head)=>{
      if(response.statusCode!==200 || head.length) {socket.destroy();reject(new Error('proxy refused'));}
      else resolve(socket);
    });
    request.once('error',reject);
    request.end();
  });
}
const decoders={gzip:zlib.createGunzip,'x-gzip':zlib.createGunzip,deflate:zlib.createInflate,br:zlib.createBrotliDecompress};

export function proxyFetch(settings,ErrorType) {
  return async(request,signal)=>{
    try {
      const url=new URL(request.url);
      const body=request.body?Buffer.from(await request.arrayBuffer()):null;
      const socket=await tunnel(settings,url.hostname,Number(url.port) || 443,signal);
      const headers={...Object.fromEntries(request.headers),'accept-encoding':'gzip, br',
        ...(body?{'content-length':String(body.length)}:{})};
      // `agent: false` would make Node open its own direct connection and
      // ignore `createConnection`, so the tunnel is the agent's only socket
      // source and name resolution is refused outright.
      const agent=new https.Agent({keepAlive:false,maxSockets:1});
      agent.createConnection=()=>tls.connect({socket,servername:url.hostname});
      const reply=await new Promise((resolve,reject)=>{
        const outgoing=https.request({host:url.hostname,port:Number(url.port) || 443,path:url.pathname+url.search,
          method:request.method,headers,agent,signal,
          lookup:(_name,_options,callback)=>callback(new Error('direct connection refused'))});
        outgoing.once('response',resolve);outgoing.once('error',reject);
        outgoing.end(body ?? undefined);
      });
      const fields=new Headers();
      for(const [name,value] of Object.entries(reply.headers))
        for(const entry of Array.isArray(value)?value:[value])if(entry!==undefined)fields.append(name,entry);
      const encoding=String(reply.headers['content-encoding'] ?? '').toLowerCase();
      let stream=reply;
      if(encoding && encoding!=='identity') {
        if(!Object.hasOwn(decoders,encoding))throw new Error('unsupported encoding');
        stream=reply.pipe(decoders[encoding]());
        reply.once('error',error=>stream.destroy(error));
        fields.delete('content-encoding');fields.delete('content-length');
      }
      const empty=request.method==='HEAD' || [204,205,304].includes(reply.statusCode);
      if(empty)reply.resume();
      return new Response(empty?null:Readable.toWeb(stream),{status:reply.statusCode,statusText:reply.statusMessage,headers:fields});
    } catch(error) {
      if(error instanceof ErrorType)throw error;
      throw new ErrorType('proxy_failed','The configured proxy could not complete this request.',502);
    }
  };
}
