// Compact URL and URLSearchParams for hierarchical (https) addresses.
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Follows the WHATWG behaviour the service relies on: relative resolution,
// dot-segment removal, live `searchParams`, form-urlencoded serialisation.
// Not covered: IDNA host conversion, IPv6 normalisation, non-special schemes
// beyond storing them opaquely.
const ports = { 'http:': '80', 'https:': '443', 'ws:': '80', 'wss:': '443', 'ftp:': '21' };
const percent = (text, unsafe) => text.replace(unsafe, character => encodeURIComponent(character)
  .replace(/[!'()*]/g, c => `%${c.charCodeAt(0).toString(16).toUpperCase()}`));
const pathUnsafe = /[^\x21\x24-\x3b\x3d\x40-\x5f\x61-\x7a\x7c\x7e]/gu;   // also encodes " # < > ? ` { }
const queryUnsafe = /[^\x21\x24-\x26\x28-\x3b\x3d\x3f-\x7e]/gu;           // also encodes " # ' < >
const hashUnsafe = /[^\x21\x23-\x3b\x3d\x3f-\x5f\x61-\x7e]/gu;
const keepEscapes = (text, unsafe) => text.split(/(%[0-9A-Fa-f]{2})/).map((part, index) => index % 2 ? part : percent(part, unsafe)).join('');

function formDecode(text) {
  const spaced = text.replace(/\+/g, ' ');
  try { return decodeURIComponent(spaced); }
  catch { return spaced.replace(/(?:%[0-9A-Fa-f]{2})+/g, run => { try { return decodeURIComponent(run); } catch { return run; } }); }
}
const formEncode = text => encodeURIComponent(text).replace(/%20/g, '+')
  .replace(/[!'()~]/g, c => `%${c.charCodeAt(0).toString(16).toUpperCase()}`);

export class URLSearchParams {
  #list = [];
  #owner = null;
  constructor(init = '', owner = null) {
    this.#owner = owner;
    if (typeof init === 'string' || init instanceof String) this.#parse(String(init));
    else if (init instanceof URLSearchParams) this.#list = init.#list.map(pair => pair.slice());
    else if (init && typeof init[Symbol.iterator] === 'function') for (const [name, value] of init) this.#list.push([String(name), String(value)]);
    else if (init && typeof init === 'object') for (const name of Object.keys(init)) this.#list.push([name, String(init[name])]);
  }
  #parse(text) {
    if (text.startsWith('?')) text = text.slice(1);
    for (const part of text.split('&')) {
      if (!part) continue;
      const at = part.indexOf('=');
      this.#list.push(at < 0 ? [formDecode(part), ''] : [formDecode(part.slice(0, at)), formDecode(part.slice(at + 1))]);
    }
  }
  #changed() { this.#owner?.(this.toString()); }
  _reset(text) { this.#list = []; this.#parse(text); }
  get size() { return this.#list.length; }
  append(name, value) { this.#list.push([String(name), String(value)]); this.#changed(); }
  delete(name) { this.#list = this.#list.filter(pair => pair[0] !== String(name)); this.#changed(); }
  get(name) { const found = this.#list.find(pair => pair[0] === String(name)); return found ? found[1] : null; }
  getAll(name) { return this.#list.filter(pair => pair[0] === String(name)).map(pair => pair[1]); }
  has(name) { return this.#list.some(pair => pair[0] === String(name)); }
  set(name, value) {
    const first = this.#list.findIndex(pair => pair[0] === String(name));
    if (first < 0) this.#list.push([String(name), String(value)]);
    else {
      this.#list[first][1] = String(value);
      this.#list = this.#list.filter((pair, index) => index <= first || pair[0] !== String(name));
    }
    this.#changed();
  }
  sort() { this.#list.sort((a, b) => a[0] < b[0] ? -1 : a[0] > b[0] ? 1 : 0); this.#changed(); }
  forEach(callback, self) { for (const [name, value] of this.#list) callback.call(self, value, name, this); }
  *entries() { for (const pair of this.#list) yield pair.slice(); }
  *keys() { for (const pair of this.#list) yield pair[0]; }
  *values() { for (const pair of this.#list) yield pair[1]; }
  [Symbol.iterator]() { return this.entries(); }
  toString() { return this.#list.map(([name, value]) => `${formEncode(name)}=${formEncode(value)}`).join('&'); }
}

function normalizePath(path) {
  const out = [];
  const parts = path.split('/');
  parts.forEach((part, index) => {
    const dot = part.replace(/%2e/gi, '.');
    const last = index === parts.length - 1;
    if (dot === '..') { if (out.length > 1) out.pop(); if (last) out.push(''); }
    else if (dot === '.') { if (last) out.push(''); }
    else out.push(part);
  });
  const joined = out.join('/');
  return joined.startsWith('/') ? joined : `/${joined}`;
}

function parse(input, base) {
  const text = String(input).trim().replace(/[\t\n\r]/g, '');
  const absolute = /^([A-Za-z][A-Za-z0-9+.-]*):(.*)$/s.exec(text);
  let scheme, rest, from = null;
  if (absolute && (ports[`${absolute[1].toLowerCase()}:`] === undefined || /^[\\/]{2}/.test(absolute[2]) || !base)) {
    scheme = `${absolute[1].toLowerCase()}:`; rest = absolute[2];
  } else {
    if (base === undefined || base === null) throw new TypeError(`Invalid URL: ${text}`);
    from = base instanceof URL ? base : new URL(String(base));
    scheme = from.protocol; rest = absolute ? absolute[2] : text;
  }
  const record = { protocol: scheme, username: '', password: '', hostname: '', port: '', pathname: '', search: '', hash: '' };
  if (ports[scheme] === undefined) {   // opaque: data:, blob:, about:
    const split = /^([^?#]*)(\?[^#]*)?(#.*)?$/s.exec(rest);
    record.pathname = split[1]; record.search = split[2] && split[2] !== '?' ? split[2] : ''; record.hash = split[3] && split[3] !== '#' ? split[3] : '';
    record.opaque = true;
    return record;
  }
  rest = rest.replace(/\\/g, '/');
  const pieces = /^(\/\/[^/?#]*)?([^?#]*)(\?[^#]*)?(#.*)?$/s.exec(rest);
  let [, authority, path, query, hash] = pieces;
  if (authority === undefined && !from) { authority = `//${path.replace(/^\/*/, '').split('/')[0]}`; path = path.replace(/^\/*[^/]*/, ''); }
  if (authority !== undefined) {
    const where = /^\/\/(?:([^@]*)@)?(\[[^\]]*\]|[^:]*)(?::(\d*))?$/.exec(authority);
    if (!where || !where[2]) throw new TypeError(`Invalid URL: ${text}`);
    if (where[1] !== undefined) { const colon = where[1].indexOf(':'); record.username = colon < 0 ? where[1] : where[1].slice(0, colon); record.password = colon < 0 ? '' : where[1].slice(colon + 1); }
    record.hostname = where[2].toLowerCase();
    if (/[\s<>^|%]/.test(record.hostname)) throw new TypeError(`Invalid URL: ${text}`);
    record.port = where[3] && where[3] !== ports[scheme] ? String(Number(where[3])) : '';
    record.pathname = normalizePath(path || '/');
  } else {
    record.username = from.username; record.password = from.password; record.hostname = from.hostname; record.port = from.port;
    if (!path) { record.pathname = from.pathname; if (query === undefined) query = from.search || undefined; }
    else if (path.startsWith('/')) record.pathname = normalizePath(path);
    else record.pathname = normalizePath(from.pathname.replace(/[^/]*$/, '') + path);
  }
  record.pathname = keepEscapes(record.pathname, pathUnsafe);
  record.search = query && query !== '?' ? `?${keepEscapes(query.slice(1), queryUnsafe)}` : '';
  record.hash = hash && hash !== '#' ? `#${keepEscapes(hash.slice(1), hashUnsafe)}` : '';
  return record;
}

export class URL {
  #record;
  #params;
  constructor(input, base) {
    this.#record = parse(input instanceof URL ? input.href : input, base);
    this.#params = new URLSearchParams(this.#record.search, text => { this.#record.search = text ? `?${text}` : ''; });
  }
  static canParse(input, base) { try { new URL(input, base); return true; } catch { return false; } }
  get protocol() { return this.#record.protocol; }
  get username() { return this.#record.username; }
  get password() { return this.#record.password; }
  get hostname() { return this.#record.hostname; }
  set hostname(value) { this.#record.hostname = String(value).toLowerCase(); }
  get port() { return this.#record.port; }
  set port(value) { this.#record.port = String(value) === ports[this.#record.protocol] ? '' : String(value); }
  get host() { return this.#record.port ? `${this.#record.hostname}:${this.#record.port}` : this.#record.hostname; }
  set host(value) { const [name, port = ''] = String(value).split(':'); this.hostname = name; this.port = port; }
  get origin() { return this.#record.opaque ? 'null' : `${this.#record.protocol}//${this.host}`; }
  get pathname() { return this.#record.pathname; }
  set pathname(value) { this.#record.pathname = this.#record.opaque ? String(value) : keepEscapes(normalizePath(String(value)), pathUnsafe); }
  get search() { return this.#record.search; }
  set search(value) {
    const text = String(value).replace(/^\?/, '');
    this.#record.search = text ? `?${keepEscapes(text, queryUnsafe)}` : '';
    this.#params._reset(this.#record.search);
  }
  get hash() { return this.#record.hash; }
  set hash(value) { const text = String(value).replace(/^#/, ''); this.#record.hash = text ? `#${keepEscapes(text, hashUnsafe)}` : ''; }
  get searchParams() { return this.#params; }
  get href() {
    const r = this.#record;
    if (r.opaque) return `${r.protocol}${r.pathname}${r.search}${r.hash}`;
    const user = r.username || r.password ? `${r.username}${r.password ? `:${r.password}` : ''}@` : '';
    return `${r.protocol}//${user}${this.host}${r.pathname}${r.search}${r.hash}`;
  }
  set href(value) { this.#record = parse(value); this.#params._reset(this.#record.search); }
  toString() { return this.href; }
  toJSON() { return this.href; }
}

if (typeof globalThis.URL === 'undefined') globalThis.URL = URL;
if (typeof globalThis.URLSearchParams === 'undefined') globalThis.URLSearchParams = URLSearchParams;
