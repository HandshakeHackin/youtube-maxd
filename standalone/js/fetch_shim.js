// fetch, Request, Response and Headers over the host's non-blocking HTTPS
// client. SPDX-License-Identifier: GPL-3.0-or-later
//
// Bodies are whole buffers: nothing in the catalog or resolve paths streams.
// The one exception is an answer that stays open (the phone connection's long
// poll): `fetch(url, { onChunk, idle_ms })` hands each piece of the body to
// `onChunk(bytes, status)` as it arrives and resolves, with an empty body, when
// the answer ends; `idle_ms` is the longest silence; what `onChunk` throws ends
// the transfer and rejects the fetch.
// With TUBE_JS_FIXTURES set, requests are answered from canned files and the
// network is never touched (the offline tests); `uncanned` exempts a request
// that a test serves from a stand-in on this machine.
import { URL, URLSearchParams } from './url_shim.js';
const host = globalThis.__host;

export class Headers {
  #map = new Map();
  constructor(init) {
    if (!init) return;
    if (init instanceof Headers) init.forEach((value, name) => this.append(name, value));
    else if (typeof init[Symbol.iterator] === 'function') for (const [name, value] of init) this.append(name, value);
    else for (const name of Object.keys(init)) this.append(name, init[name]);
  }
  append(name, value) {
    const key = String(name).toLowerCase(), old = this.#map.get(key);
    this.#map.set(key, old === undefined ? String(value) : `${old}, ${value}`);
  }
  set(name, value) { this.#map.set(String(name).toLowerCase(), String(value)); }
  get(name) { return this.#map.get(String(name).toLowerCase()) ?? null; }
  has(name) { return this.#map.has(String(name).toLowerCase()); }
  delete(name) { this.#map.delete(String(name).toLowerCase()); }
  forEach(callback, self) { for (const [name, value] of this.#map) callback.call(self, value, name, this); }
  entries() { return this.#map.entries(); }
  keys() { return this.#map.keys(); }
  values() { return this.#map.values(); }
  getSetCookie() { return []; }   // the service keeps no cookie jar
  [Symbol.iterator]() { return this.#map.entries(); }
}

// "name: value\n" lines, as the host hands headers over.
export function headerLines(text) {
  const headers = new Headers();
  for (const line of String(text ?? '').split('\n')) {
    const at = line.indexOf(':');
    if (at > 0) headers.append(line.slice(0, at).trim(), line.slice(at + 1).trim());
  }
  return headers;
}

// A body is kept as a string or bytes until someone reads it.
export function bytesOf(body) {
  if (body === undefined || body === null) return null;
  if (typeof body === 'string') return new Uint8Array(host.utf8Encode(body));
  if (body instanceof ArrayBuffer) return new Uint8Array(body);
  if (ArrayBuffer.isView(body)) return new Uint8Array(body.buffer, body.byteOffset, body.byteLength);
  if (body instanceof URLSearchParams) return new Uint8Array(host.utf8Encode(body.toString()));
  throw new TypeError('Unsupported body type.');
}
const reader = Base => class extends Base {
  bodyUsed = false;
  #take() { this.bodyUsed = true; return this._body; }
  async arrayBuffer() { const bytes = bytesOf(this.#take()); return bytes ? bytes.slice().buffer : new ArrayBuffer(0); }
  async bytes() { return new Uint8Array(await this.arrayBuffer()); }
  async text() { const body = this.#take(); return typeof body === 'string' ? body : body ? host.utf8Decode(bytesOf(body)) : ''; }
  async json() { return JSON.parse(await this.text()); }
  get body() { return null; }     // no streams; see README
};

export class Request extends reader(Object) {
  constructor(input, init = {}) {
    super();
    const from = input instanceof Request ? input : null;
    this.url = from ? from.url : new URL(String(input)).href;
    this.method = String(init.method ?? from?.method ?? 'GET').toUpperCase();
    this.headers = new Headers(init.headers ?? from?.headers);
    this._body = init.body ?? from?._body ?? null;
    this.redirect = init.redirect ?? from?.redirect ?? 'follow';
    this.signal = init.signal ?? from?.signal ?? null;
    this.credentials = init.credentials ?? from?.credentials ?? 'omit';
    if (this._body instanceof URLSearchParams && !this.headers.has('content-type'))
      this.headers.set('content-type', 'application/x-www-form-urlencoded;charset=UTF-8');
  }
  clone() { return new Request(this); }
}

export class Response extends reader(Object) {
  constructor(body = null, init = {}) {
    super();
    this._body = body;
    this.status = init.status ?? 200;
    this.statusText = init.statusText ?? '';
    this.headers = init.headers instanceof Headers ? init.headers : new Headers(init.headers);
    this.url = init.url ?? '';
    this.redirected = !!init.redirected;
    this.type = 'basic';
  }
  get ok() { return this.status >= 200 && this.status < 300; }
  clone() { return new Response(this._body, this); }
  static json(value, init = {}) {
    return new Response(JSON.stringify(value), { ...init, headers: { 'content-type': 'application/json', ...init.headers } });
  }
}

// Canned answers: index.json maps "METHOD host/path" to one entry or a list;
// an entry may require a text in the address (`url_contains`) or the request
// body (`body_contains`, one text or a list that must all occur) or in one
// request header (`header_contains`, as "name: value"); `times` lets an entry
// answer that many requests and then step aside for the next one that fits.
// The first that fits answers. `fixtureRequests` is what the tests read back:
// the key, whether a bearer was sent, and the entry's `tag`.
let fixtures = null;
export const fixtureLog = [];
export const fixtureRequests = [];
function canned(request) {
  const directory = host.getenv('TUBE_JS_FIXTURES');
  if (!directory) return null;
  fixtures ??= JSON.parse(host.utf8Decode(host.readFile(`${directory}/index.json`)));
  const url = new URL(request.url), key = `${request.method} ${url.hostname}${url.pathname}`;
  const body = typeof request._body === 'string' ? request._body : request._body ? host.utf8Decode(bytesOf(request._body)) : '';
  const lines = [];
  request.headers.forEach((value, name) => lines.push(`${name}: ${value}`));
  const entry = [fixtures[key] ?? []].flat().find(candidate =>
    [candidate.body_contains ?? []].flat().every(part => body.includes(part)) && (!candidate.url_contains || request.url.includes(candidate.url_contains)) &&
    (!candidate.header_contains || lines.some(line => line.includes(candidate.header_contains))) &&
    (candidate.times === undefined || (candidate.used ?? 0) < candidate.times));
  if (fixtureLog.length < 256) fixtureLog.push(key);
  if (fixtureRequests.length < 4096) fixtureRequests.push({ key, bearer: /^Bearer /.test(request.headers.get('authorization') ?? ''), tag: entry?.tag ?? '' });
  if (entry) entry.used = (entry.used ?? 0) + 1;
  if (!entry) throw new TypeError(`fetch failed: no canned response for ${key}`);
  const data = entry.file ? host.readFile(`${directory}/${entry.file}`) : new ArrayBuffer(0);
  if (data === undefined) throw new TypeError(`fetch failed: missing fixture ${entry.file}`);
  return new Response(data, { status: entry.status ?? 200, headers: { 'content-type': entry.type ?? 'application/json' }, url: request.url });
}

// Transfers in flight: id -> settle functions. The host calls back once each.
const pending = new Map();
globalThis.__tubeHttpDone = (id, answer, error) => {
  const transfer = pending.get(id);
  if (!transfer) return;
  pending.delete(id);
  transfer.signal?.removeEventListener('abort', transfer.abort);
  if (!answer) transfer.reject(new TypeError(`fetch failed: ${error}`));
  else transfer.resolve(new Response(answer.body, { status: answer.status, headers: answer.headers, url: answer.url, redirected: answer.redirected }));
};
// A streaming transfer's pieces, in order and before its completion.
globalThis.__tubeHttpChunk = (id, status, bytes) => {
  const transfer = pending.get(id);
  if (!transfer?.onChunk) return;
  try { transfer.onChunk(new Uint8Array(bytes), status); }
  catch (error) {   // the reader refused the piece: the transfer ends with its reason
    pending.delete(id);
    transfer.signal?.removeEventListener('abort', transfer.abort);
    host.httpCancel(id);
    transfer.reject(error);
  }
};

// Headers a script may not choose; the client negotiates encoding (whatever
// its build can decode: never brotli on the console) and decodes it.
const managed = new Set(['accept-encoding', 'content-length', 'host', 'connection', 'transfer-encoding', 'expect']);
const REDIRECT = { follow: 0, manual: 1, error: 2 };

export async function fetch(input, init) {
  const request = new Request(input, init);
  request.signal?.throwIfAborted();
  const onChunk = typeof init?.onChunk === 'function' ? init.onChunk : null;
  const offline = init?.uncanned ? null : canned(request);
  if (offline) {
    if (!onChunk) return offline;
    const bytes = bytesOf(offline._body);   // a canned stream is one piece
    if (bytes?.length) onChunk(bytes, offline.status);
    return new Response(null, { status: offline.status, headers: offline.headers, url: offline.url });
  }
  const headers = [];
  request.headers.forEach((value, name) => { if (!managed.has(name)) headers.push([name, value]); });
  const deadline = request.signal?.deadline;
  return new Promise((resolve, reject) => {
    const id = host.httpStart({ method: request.method, url: request.url, headers, body: bytesOf(request._body) ?? undefined,
      redirect: REDIRECT[request.redirect] ?? 0,
      timeout_ms: onChunk ? init.idle_ms ?? 65000 : deadline === undefined ? 15000 : Math.max(1, Math.round(deadline - host.now())),
      max_bytes: init?.max_bytes, stream: !!onChunk });
    const abort = () => { if (pending.delete(id)) { host.httpCancel(id); reject(request.signal.reason); } };
    pending.set(id, { resolve, reject, signal: request.signal, abort, onChunk });
    request.signal?.addEventListener('abort', abort, { once: true });
  });
}

for (const [name, value] of Object.entries({ fetch, Request, Response, Headers }))
  if (typeof globalThis[name] === 'undefined') globalThis[name] = value;
