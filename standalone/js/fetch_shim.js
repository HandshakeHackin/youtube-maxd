// fetch, Request, Response and Headers over the host's blocking HTTPS call.
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Bodies are whole buffers: nothing in the catalog or resolve paths streams.
// With TUBE_JS_FIXTURES set, requests are answered from canned files and the
// network is never touched (the offline test).
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

// A body is kept as a string or bytes until someone reads it.
function bytesOf(body) {
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
    this.headers = new Headers(init.headers);
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

let fixtures = null;
function canned(request) {
  const directory = host.getenv('TUBE_JS_FIXTURES');
  if (!directory) return null;
  fixtures ??= JSON.parse(host.utf8Decode(host.readFile(`${directory}/index.json`)));
  const url = new URL(request.url);
  const entry = fixtures[`${request.method} ${url.hostname}${url.pathname}`];
  if (!entry) throw new TypeError(`fetch failed: no canned response for ${request.method} ${url.hostname}${url.pathname}`);
  const body = entry.file ? host.readFile(`${directory}/${entry.file}`) : new ArrayBuffer(0);
  if (body === undefined) throw new TypeError(`fetch failed: missing fixture ${entry.file}`);
  return new Response(body, { status: entry.status ?? 200, headers: { 'content-type': entry.type ?? 'application/json' }, url: request.url });
}

// Headers a script may not choose; libcurl negotiates encoding and decodes it.
const managed = new Set(['accept-encoding', 'content-length', 'host', 'connection', 'transfer-encoding']);

export async function fetch(input, init) {
  const request = new Request(input, init);
  request.signal?.throwIfAborted();
  const offline = canned(request);
  if (offline) return offline;
  const headers = [];
  request.headers.forEach((value, name) => { if (!managed.has(name)) headers.push([name, value]); });
  const deadline = request.signal?.deadline;
  const answer = host.http({
    method: request.method, url: request.url, headers, body: bytesOf(request._body) ?? undefined,
    follow: request.redirect === 'follow' ? 1 : 0,
    timeout_ms: deadline === undefined ? 15000 : Math.max(1, Math.round(deadline - host.now())),
    max_bytes: init?.max_bytes
  });
  if (request.redirect === 'error' && answer.status >= 300 && answer.status < 400) throw new TypeError('fetch failed: redirect');
  return new Response(answer.body, { status: answer.status, headers: answer.headers, url: answer.url, redirected: answer.url !== request.url });
}

for (const [name, value] of Object.entries({ fetch, Request, Response, Headers }))
  if (typeof globalThis[name] === 'undefined') globalThis[name] = value;
