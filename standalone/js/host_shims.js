// Web-platform globals the service needs and QuickJS does not have, written
// over the native `__host` object. SPDX-License-Identifier: GPL-3.0-or-later
//
// Deliberately small: only what youtubei.js and the service modules use.
const host = globalThis.__host;
const define = (name, value) => { if (typeof globalThis[name] === 'undefined') globalThis[name] = value; };

// ---- console, timers, microtasks ----
const show = value => typeof value === 'string' ? value : value instanceof Error ? `${value.name}: ${value.message}` :
  (() => { try { return JSON.stringify(value); } catch { return String(value); } })();
const quiet = !host.getenv('TUBE_JS_VERBOSE');
const line = (...values) => host.log(values.map(show).join(' '));
globalThis.console = { log: line, info: quiet ? () => {} : line, debug: quiet ? () => {} : line, warn: quiet ? () => {} : line, error: line, trace: line };

// The native timer queue holds the deadlines; this table holds the functions.
const timers = new Map();
const arm = (callback, delay, repeat, values) => {
  const wait = Math.max(0, Number(delay) || 0);
  const id = host.timerAdd(wait, repeat ? Math.max(1, wait) : 0);
  timers.set(id, { callback, repeat, values });
  return id;
};
const disarm = id => { if (timers.delete(id)) host.timerCancel(id); };
define('setTimeout', (callback, delay, ...values) => arm(callback, delay, false, values));
define('setInterval', (callback, delay, ...values) => arm(callback, delay, true, values));
define('clearTimeout', disarm);
define('clearInterval', disarm);
define('queueMicrotask', callback => { Promise.resolve().then(callback); });
// Called by the host loop for each timer that is due.
globalThis.__tubeTimer = id => {
  const timer = timers.get(id);
  if (!timer) return;
  if (!timer.repeat) timers.delete(id);
  timer.callback(...timer.values);
};
define('performance', { now: () => host.now(), timeOrigin: Date.now() });

// ---- text, base64, randomness ----
define('TextEncoder', class TextEncoder {
  get encoding() { return 'utf-8'; }
  encode(text = '') { return new Uint8Array(host.utf8Encode(String(text))); }
});
define('TextDecoder', class TextDecoder {
  constructor(label = 'utf-8') {
    if (!/^utf-?8$/i.test(label)) throw new RangeError(`Unsupported encoding: ${label}`);
  }
  get encoding() { return 'utf-8'; }
  decode(data) { return data === undefined ? '' : host.utf8Decode(data); }
});
// The library modules measure text with Node's Buffer.byteLength; nothing else of Buffer is used.
define('Buffer', { byteLength: value => typeof value === 'string' ? host.utf8Encode(value).byteLength : value.byteLength });
const alphabet ='ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/';
define('btoa', text => {
  let out = '';
  for (let i = 0; i < text.length; i += 3) {
    const a = text.charCodeAt(i), b = text.charCodeAt(i + 1), c = text.charCodeAt(i + 2);
    if (a > 255 || b > 255 || c > 255) throw new RangeError('Invalid character');
    out += alphabet[a >> 2] + alphabet[(a & 3) << 4 | b >> 4] +
      (i + 1 < text.length ? alphabet[(b & 15) << 2 | c >> 6] : '=') + (i + 2 < text.length ? alphabet[c & 63] : '=');
  }
  return out;
});
define('atob', text => {
  const clean = String(text).replace(/[\t\n\f\r ]/g, '').replace(/=+$/, '');
  let out = '', bits = 0, value = 0;
  for (const letter of clean) {
    const index = alphabet.indexOf(letter);
    if (index < 0) throw new RangeError('Invalid character');
    value = value << 6 | index; bits += 6;
    if (bits >= 8) { bits -= 8; out += String.fromCharCode(value >> bits & 255); }
  }
  return out;
});
const hex = bytes => Array.from(bytes, byte => byte.toString(16).padStart(2, '0')).join('');
define('crypto', {
  getRandomValues(view) {
    const bytes = new Uint8Array(view.buffer, view.byteOffset, view.byteLength);
    for (let at = 0; at < bytes.length; at += 256)
      bytes.set(new Uint8Array(host.random(Math.min(256, bytes.length - at))), at);
    return view;
  },
  randomUUID() {
    const bytes = new Uint8Array(host.random(16));
    bytes[6] = bytes[6] & 15 | 64; bytes[8] = bytes[8] & 63 | 128;
    const text = hex(bytes);
    return `${text.slice(0, 8)}-${text.slice(8, 12)}-${text.slice(12, 16)}-${text.slice(16, 20)}-${text.slice(20)}`;
  }
});

// ---- structuredClone, events, abort ----
define('structuredClone', function clone(value, _options, seen = new Map()) {
  if (value === null || typeof value !== 'object') {
    if (typeof value === 'function' || typeof value === 'symbol') throw new TypeError('The value cannot be cloned.');
    return value;
  }
  if (seen.has(value)) return seen.get(value);
  let copy;
  if (Array.isArray(value)) { copy = []; seen.set(value, copy); for (const item of value) copy.push(clone(item, null, seen)); }
  else if (value instanceof Date) copy = new Date(value.getTime());
  else if (value instanceof ArrayBuffer) copy = value.slice(0);
  else if (ArrayBuffer.isView(value)) copy = new value.constructor(value);
  else if (value instanceof Map) { copy = new Map(); seen.set(value, copy); for (const [k, v] of value) copy.set(clone(k, null, seen), clone(v, null, seen)); }
  else if (value instanceof Set) { copy = new Set(); seen.set(value, copy); for (const v of value) copy.add(clone(v, null, seen)); }
  else { copy = {}; seen.set(value, copy); for (const key of Object.keys(value)) copy[key] = clone(value[key], null, seen); }
  return copy;
});
define('Event', class Event {
  constructor(type, init = {}) { this.type = String(type); this.cancelable = !!init.cancelable; this.defaultPrevented = false; this.target = null; }
  preventDefault() { if (this.cancelable) this.defaultPrevented = true; }
});
define('CustomEvent', class CustomEvent extends globalThis.Event {
  constructor(type, init = {}) { super(type, init); this.detail = init.detail ?? null; }
});
define('EventTarget', class EventTarget {
  #listeners = new Map();
  addEventListener(type, listener, options) {
    if (!listener) return;
    if (!this.#listeners.has(type)) this.#listeners.set(type, new Map());
    this.#listeners.get(type).set(listener, !!(options && options.once));
  }
  removeEventListener(type, listener) { this.#listeners.get(type)?.delete(listener); }
  dispatchEvent(event) {
    event.target = this;
    for (const [listener, once] of Array.from(this.#listeners.get(event.type) ?? [])) {
      if (once) this.#listeners.get(event.type).delete(listener);
      typeof listener === 'function' ? listener.call(this, event) : listener.handleEvent(event);
    }
    return !event.defaultPrevented;
  }
});
define('DOMException', class DOMException extends Error {
  constructor(message = '', name = 'Error') { super(message); this.name = name; }
});
define('AbortSignal', class AbortSignal extends globalThis.EventTarget {
  aborted = false; reason = undefined; onabort = null;
  throwIfAborted() { if (this.aborted) throw this.reason; }
  static abort(reason) { const signal = new AbortSignal(); cancel(signal, reason); return signal; }
  static timeout(delay) {
    const signal = new AbortSignal();
    signal.deadline = host.now() + delay;  // read by fetch as the transfer limit
    return signal;
  }
  static any(signals) {
    const signal = new AbortSignal();
    for (const source of signals) {
      if (source.aborted) { cancel(signal, source.reason); break; }
      if (source.deadline !== undefined) signal.deadline = Math.min(signal.deadline ?? Infinity, source.deadline);
      source.addEventListener('abort', () => cancel(signal, source.reason), { once: true });
    }
    return signal;
  }
});
function cancel(signal, reason) {
  if (signal.aborted) return;
  signal.aborted = true;
  signal.reason = reason ?? new globalThis.DOMException('This operation was aborted', 'AbortError');
  const event = new globalThis.Event('abort');
  signal.onabort?.(event);
  signal.dispatchEvent(event);
}
define('AbortController', class AbortController {
  signal = new globalThis.AbortSignal();
  abort(reason) { cancel(this.signal, reason); }
});

// The service sends a fixed English interface; only the time zone name is read.
define('Intl', {
  DateTimeFormat: function DateTimeFormat() {
    const zone = host.getenv('TZ') ?? '';
    return { resolvedOptions: () => ({ timeZone: /^[A-Za-z_]+(\/[A-Za-z0-9_+-]+)+$/.test(zone) ? zone : 'UTC', locale: 'en-US' }) };
  }
});
