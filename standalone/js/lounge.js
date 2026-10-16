// Opt-in YouTube Lounge receiver ("Link with TV code"): the port of
// companion/prosperotube/remote_lounge.py, with its request and answer shapes,
// bounds and error codes. No account credentials are involved.
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Protocol reference: MediaServiceCore 4d976773, lounge/; BrowserChannel framing
// follows Closure's channelrequest.js (lengths in UTF-16 code units, not UTF-8
// bytes). The back channel is a long poll that stays open and is read as it
// arrives (fetch's `onChunk`, see fetch_shim.js). Native clients acknowledge
// commands only after applying them on their UI thread.
//
// Lounge tokens, SID and gsessionid stay inside this module: they are never
// part of an answer or an error text, and nothing here is written to disk. The
// receiver's identity (device and screen) is the provider's to keep; it passes
// both with `start`.
import { ServiceError } from './errors.js';
import { Invalid } from './schema.js';

const host = globalThis.__host;
const YOUTUBE = 'https://www.youtube.com/api/lounge';
export const MAX_FRAME = 262144, MAX_QUEUE = 128, MAX_RECEIVERS = 4, LEASE_SECONDS = 120;
const INT_MAX = 2147483647, IDLE_MS = 65000;
const VIDEO = /^[A-Za-z0-9_-]{11}$/;

// Never interpolate HTTP failures, addresses, server bodies or Lounge tokens.
export const failure = (code = 'remote_protocol', status = 502) =>
  new ServiceError(code, 'YouTube phone connection stopped. Enable it again to reconnect.', status);
const same = (a, b) => JSON.stringify(a) === JSON.stringify(b);
const own = (object, key, fallback) => Object.hasOwn(object, key) ? object[key] : fallback;
const record = value => value !== null && typeof value === 'object' && !Array.isArray(value);
const playlistChange = entry => entry.action === 'setPlaylist' || entry.action === 'updatePlaylist';
// First milestones only: never log phone names, IDs, codes, tokens or server bodies.
const PHONE_EVENTS = ['remoteConnected', 'loungeStatus', 'setPlaylist'];
const PHONE_STAGES = ['received', 'delivered', 'acknowledged'];
function traceEvent(receiver, stage, action) {
  const event = PHONE_EVENTS.indexOf(action), step = PHONE_STAGES.indexOf(stage);
  if (event < 0 || step < 0) return;
  const bit = 1 << (step * PHONE_EVENTS.length + event);
  if (receiver.observed & bit) return;
  receiver.observed |= bit;
  console.error(`YouTube phone state: ${PHONE_STAGES[step]} ${PHONE_EVENTS[event]}`);
}
// What Python calls false: nothing, zero, and anything empty.
const empty = value => !value || (Array.isArray(value) && !value.length) || (record(value) && !Object.keys(value).length);

// Strict UTF-8 to text, piece by piece: a character split between two pieces
// waits for its rest; overlong forms, surrogates and stray bytes are refused.
class Utf8 {
  #rest = [];
  feed(bytes, final) {
    let data = bytes, text = '', i = 0;
    const units = [];
    if (this.#rest.length) {
      data = new Uint8Array(this.#rest.length + bytes.length);
      data.set(this.#rest);
      data.set(bytes, this.#rest.length);
    }
    this.#rest = [];
    while (i < data.length) {
      const lead = data[i];
      if (lead < 0x80) { units.push(lead); i++; }
      else {
        const more = lead >= 0xC2 && lead <= 0xDF ? 1 : lead >= 0xE0 && lead <= 0xEF ? 2 : lead >= 0xF0 && lead <= 0xF4 ? 3 : 0;
        if (!more) throw failure();
        const low = lead === 0xE0 ? 0xA0 : lead === 0xF0 ? 0x90 : 0x80, high = lead === 0xED ? 0x9F : lead === 0xF4 ? 0x8F : 0xBF;
        let point = lead & (0xFF >> (more + 2)), k = 1;
        for (; k <= more && i + k < data.length; k++) {
          const next = data[i + k];
          if (k === 1 ? !(next >= low && next <= high) : (next & 0xC0) !== 0x80) throw failure();
          point = (point << 6) | (next & 0x3F);
        }
        if (k <= more) { this.#rest = Array.from(data.subarray(i)); break; }   // the rest comes with the next piece
        if (point > 0xFFFF) units.push(0xD800 + ((point - 0x10000) >> 10), 0xDC00 + ((point - 0x10000) & 0x3FF));
        else units.push(point);
        i += more + 1;
      }
      if (units.length >= 8192) { text += String.fromCharCode(...units); units.length = 0; }
    }
    if (final && this.#rest.length) throw failure();
    return text + String.fromCharCode(...units);
  }
}

// Incremental application framing: a decimal length, a newline, that many
// UTF-16 code units of JSON; several frames may follow one another.
export class Frames {
  #decoder = new Utf8();
  #buffer = '';
  #length = null;
  feed(data, final = false) {
    this.#buffer += this.#decoder.feed(data, final);
    if (this.#buffer.length > MAX_FRAME + 16) throw failure();
    const result = [];
    for (;;) {
      if (this.#length === null) {
        const end = this.#buffer.indexOf('\n');
        if (end < 0) {
          if (this.#buffer.length > 8) throw failure();
          break;
        }
        const header = this.#buffer.slice(0, end);
        if (!/^[0-9]{1,6}$/.test(header)) throw failure();
        this.#length = Number(header);
        if (!(this.#length > 0 && this.#length <= MAX_FRAME)) throw failure();
        this.#buffer = this.#buffer.slice(end + 1);
      }
      if (this.#buffer.length < this.#length) break;
      let value;
      try { value = JSON.parse(this.#buffer.slice(0, this.#length)); } catch { throw failure(); }
      this.#buffer = this.#buffer.slice(this.#length);
      this.#length = null;
      if (!Array.isArray(value) || value.length > 512) throw failure();
      result.push(value);
    }
    if (final && (this.#buffer || this.#length !== null)) throw failure();
    return result;
  }
}

function boundedText(value, limit, pattern) {
  // eslint-disable-next-line no-control-regex
  if (typeof value !== 'string' || value.length > limit || /[\x00-\x1f]/.test(value)) throw failure();
  if (pattern && !pattern.test(value)) throw failure();
  return value;
}

// A number, or text that is one (the phone sends both), finite and in range.
function number(value, minimum = 0, maximum = 1000000) {
  let result;
  if (typeof value === 'number') result = value;
  else if (typeof value === 'string' && /^\s*[+-]?(?:[0-9]+\.?[0-9]*|\.[0-9]+)(?:[eE][+-]?[0-9]+)?\s*$/.test(value)) result = Number(value);
  else throw failure();
  if (!Number.isFinite(result) || !(result >= minimum && result <= maximum)) throw failure();
  return result;
}

// Typed, allowlisted commands; never arbitrary wire parameters. `queueState`
// receives the whole queue when it is longer than one native window.
export function command(name, args, queueState = null) {
  if (!record(args) || Object.keys(args).length > 32) throw failure();
  const result = { action: name };
  if (['play', 'pause', 'next', 'previous', 'stopVideo', 'getNowPlaying'].includes(name)) return result;
  if (name === 'setPlaylist' || name === 'updatePlaylist') {
    let video = boundedText(own(args, 'videoId', ''), 11);
    const ids = empty(args.videoIds) ? [] : boundedText(args.videoIds, 119999).split(',');
    if (ids.length > 10000 || ids.some(id => !VIDEO.test(id))) throw failure();
    const index = number(own(args, 'currentIndex', 0), 0, 100000);
    if (!Number.isInteger(index) || (video && !VIDEO.test(video))) throw failure();
    if (name === 'setPlaylist' && !video) {
      if (!ids.length || index >= ids.length) throw failure();
      video = ids[index];
    }
    Object.assign(result, { video_id: video, video_ids: ids, playlist: boundedText(own(args, 'listId', ''), 100, /^[A-Za-z0-9_-]*$/),
      index, position_sec: number(own(args, 'currentTime', 0)), ctt: boundedText(own(args, 'ctt', ''), 512) });
    if (ids.length > 128) {
      if (!video) {
        if (index >= ids.length) throw failure();
        video = ids[index];
        result.video_id = video;
      }
      const anchor = index < ids.length && ids[index] === video ? index : ids.indexOf(video);
      if (anchor < 0 || index < anchor || index - anchor + ids.length - 1 > 100000) throw failure();
      const base = index - anchor, start = Math.max(0, Math.min(anchor - 32, ids.length - 128));
      result.video_ids = ids.slice(start, start + 128);
      result.window_start = base + start;
      if (queueState) Object.assign(queueState, { ids, base, playlist: result.playlist, ctt: result.ctt,
        window_start: base + start, window_end: base + start + 127 });
    }
  } else if (name === 'seekTo') result.position_sec = number(args.newTime);
  else if (name === 'setVolume') {
    if (!Object.hasOwn(args, 'volume') && !Object.hasOwn(args, 'delta')) throw failure();
    if (Object.hasOwn(args, 'volume')) result.volume = number(args.volume, 0, 100);
    if (Object.hasOwn(args, 'delta')) result.delta = number(args.delta, -100, 100);
  } else if (name === 'remoteConnected' || name === 'remoteDisconnected') {
    Object.assign(result, { device_id: boundedText(own(args, 'id', ''), 128), name: boundedText(own(args, 'name', ''), 100) });
  } else if (name === 'loungeStatus') {
    let devices;
    try { devices = JSON.parse(boundedText(own(args, 'devices', '[]'), 32768)); } catch (error) { throw error instanceof ServiceError ? error : failure(); }
    if (!Array.isArray(devices) || devices.length > 32) throw failure();
    result.devices = [];
    for (const device of devices) {
      if (!record(device)) throw failure();
      if (device.type === 'LOUNGE_SCREEN') continue;
      result.devices.push({ device_id: boundedText(own(device, 'id', ''), 128), name: boundedText(own(device, 'name', ''), 100) });
    }
  } else if (name === 'dpadCommand') result.key = boundedText(own(args, 'key', ''), 8, /^(?:UP|DOWN|LEFT|RIGHT|ENTER|BACK)$/);
  else if (name === 'voiceCommand') result.status = boundedText(own(args, 'status', ''), 12, /^(?:INITIATED|CANCELED)$/);
  else if (name === 'setSubtitlesTrack') {
    Object.assign(result, { video_id: boundedText(own(args, 'videoId', ''), 11, /^(?:[A-Za-z0-9_-]{11})?$/),
      language: boundedText(own(args, 'languageCode', ''), 32, /^[A-Za-z0-9_-]*$/) });
  } else return null;
  return result;
}

// ---- request models (RemoteRequest, PlayerState): unknown fields refused, no coercion ----
const field = {
  // eslint-disable-next-line no-control-regex
  text: (fallback, pattern, max = Infinity) => (value, name) => {
    if (value === undefined) return fallback;
    if (typeof value !== 'string' || value.length > max || !pattern.test(value)) throw new Invalid('String should match the field rules', name);
    return value;
  },
  integer: (fallback, min, max) => (value, name) => {
    if (value === undefined) return fallback;
    if (!Number.isInteger(value) || value < min || value > max) throw new Invalid('Input should be an integer in range', name);
    return value;
  },
  number: (fallback, min, max) => (value, name) => {
    if (value === undefined) return fallback;
    if (typeof value !== 'number' || !Number.isFinite(value) || value < min || value > max) throw new Invalid('Input should be a number in range', name);
    return value;
  },
  flag: fallback => (value, name) => {
    if (value === undefined) return fallback;
    if (typeof value !== 'boolean') throw new Invalid('Input should be a boolean', name);
    return value;
  }
};
function model(fields, input, prefix = '') {
  if (!record(input)) throw new Invalid('Input should be an object', prefix);
  for (const name of Object.keys(input)) if (!Object.hasOwn(fields, name)) throw new Invalid('Extra inputs are not permitted', prefix + name);
  const output = {};
  for (const [name, read] of Object.entries(fields)) output[name] = read(input[name], prefix + name);
  return output;
}
const PLAYER = {
  video_id: field.text('', /^(?:|[A-Za-z0-9_-]{11})$/), state: field.integer(2, -1, 4),
  position_sec: field.number(0, 0, 1000000), duration_sec: field.number(0, 0, 1000000),
  playlist: field.text('', /^[A-Za-z0-9_-]*$/, 100), index: field.integer(0, 0, 100000),
  // eslint-disable-next-line no-control-regex
  ctt: field.text('', /^[^\x00-\x1f]*$/, 512), volume: field.integer(100, 0, 100), muted: field.flag(false),
  has_previous: field.flag(false), has_next: field.flag(false), language: field.text('', /^[A-Za-z0-9_-]*$/, 32),
  vss_id: field.text('', /^[A-Za-z0-9_.-]*$/, 64)
};
export const playerState = input => model(PLAYER, input, 'player.');
const REMOTE = {
  action: (value, name) => { if (!['start', 'pair', 'poll', 'update', 'stop'].includes(value)) throw new Invalid('String should match the field rules', name); return value; },
  device_id: field.text('', /^(?:|[0-9a-f]{8}(?:-[0-9a-f]{4}){3}-[0-9a-f]{12})$/),
  // eslint-disable-next-line no-control-regex
  name: field.text('ProsperoTube PS5', /^[^\x00-\x1f]+$/, 100), screen_id: field.text('', /^[A-Za-z0-9_-]*$/, 128),
  session: field.text('', /^(?:|[0-9a-f]{32})$/), ack: field.integer(0, 0, INT_MAX), update_id: field.integer(0, 0, INT_MAX),
  player: value => value === undefined || value === null ? null : playerState(value),
  voice_enabled: field.flag(false), profile: field.text('guest', /^[A-Za-z0-9_-]{1,32}$/)
};
export const remoteRequest = input => model(REMOTE, input);

// One operation at a time, in arrival order (asyncio.Lock).
class Lock {
  #tail = Promise.resolve();
  run(work) {
    const result = this.#tail.then(work);
    this.#tail = result.catch(() => {});
    return result;
  }
}
const hex = count => Array.from(new Uint8Array(host.random(count)), byte => byte.toString(16).padStart(2, '0')).join('');
function uuid4() {
  const bytes = new Uint8Array(host.random(16));
  bytes[6] = (bytes[6] & 15) | 64; bytes[8] = (bytes[8] & 63) | 128;
  const text = Array.from(bytes, byte => byte.toString(16).padStart(2, '0')).join('');
  return `${text.slice(0, 8)}-${text.slice(8, 12)}-${text.slice(12, 16)}-${text.slice(16, 20)}-${text.slice(20)}`;
}
// Python's str() of a float: a whole number keeps its ".0".
const float = value => Number.isInteger(value) ? value.toFixed(1) : String(value);
const pause = (milliseconds, signal) => new Promise((resolve, reject) => {
  const timer = setTimeout(() => { signal.removeEventListener('abort', stop); resolve(); }, milliseconds);
  const stop = () => { clearTimeout(timer); reject(signal.reason); };
  signal.addEventListener('abort', stop, { once: true });
});

function newReceiver(session, device, name, screen, voice, profile, now) {
  return { session, device_id: device, name, screen_id: screen, token: '', sid: '', gsid: '', code: '', aid: -1, rid: 1, ofs: 0,
    sequence: 0, delivered: 0, ack: 0, observed: 0, update_id: -1, last_player: null, error: '', queue: [], listening: null, abort: new AbortController(),
    lock: new Lock(), touched: now, voice_enabled: voice, full_playlist: null, profile, playlist_anchor: null, playlist_task: null, playlist_retry: 0 };
}

export class Lounge {
  // `base`: YouTube, or the tests' stand-in on this machine. `playlistLoader`:
  // the library's `remote_playlist` action, or null. `clock`: seconds, monotonic.
  constructor({ base = '', playlistLoader = null, clock = () => host.now() / 1000 } = {}) {
    this.base = base || YOUTUBE;
    this.direct = !!base;                 // the stand-in is asked for real, even while everything else is canned
    this.receivers = new Map();
    this.lock = new Lock();
    this.playlistLoader = playlistLoader;
    this.clock = clock;
  }

  status(receiver) {
    return { api_version: 1, session: receiver.session, device_id: receiver.device_id, screen_id: receiver.screen_id, name: receiver.name,
      state: receiver.error ? 'failed' : 'listening', error: receiver.error };
  }

  params(receiver, rpc = false) {
    // Native capture and search support and a speech recogniser must both exist for "mic".
    const params = { device: 'LOUNGE_SCREEN', theme: 'cl', capabilities: `dsp,dpa${receiver.voice_enabled ? ',mic' : ''}`,
      mdxVersion: '2', VER: '8', CVER: '1', v: '2', t: '1', app: 'lb-v4', name: receiver.name, id: receiver.device_id,
      loungeIdToken: receiver.token, zx: hex(8), RID: rpc ? 'rpc' : String(receiver.rid) };
    if (receiver.sid) Object.assign(params, { SID: receiver.sid, gsessionid: receiver.gsid, AID: String(receiver.aid) });
    if (rpc) Object.assign(params, { CI: '0', TYPE: 'xmlhttp' });
    else receiver.rid += 1;
    return params;
  }

  // One request to the Lounge service. Redirects are never followed and no
  // header of ours exists to leak; a failure carries nothing of the exchange.
  send(method, path, { params = null, data = null, ...more } = {}) {
    const query = params ? `?${new URLSearchParams(params).toString().replace(/\+/g, '%20')}` : '';
    return fetch(`${this.base}${path}${query}`, { method, redirect: 'manual', uncanned: this.direct,
      ...(data ? { body: new URLSearchParams(data).toString(), headers: { 'content-type': 'application/x-www-form-urlencoded' } } : {}), ...more });
  }
  // A whole, bounded answer.
  async response(method, path, options = {}) {
    const reply = await this.send(method, path, { ...options, max_bytes: MAX_FRAME + 1 });
    if (reply.status !== 200) throw failure('remote_upstream');
    const body = new Uint8Array(await reply.arrayBuffer());
    if (body.length > MAX_FRAME) throw failure();
    return body;
  }

  accept(receiver, frames) {
    for (const frame of frames) {
      for (const message of frame) {
        if (!Array.isArray(message) || message.length !== 2 || !Number.isInteger(message[0]) || message[0] < 0 || message[0] > INT_MAX ||
            !Array.isArray(message[1]) || message[1].length < 1 || message[1].length > 8) throw failure();
        const [index, payload] = message;
        if (index <= receiver.aid) continue;
        if (index !== receiver.aid + 1) throw failure('remote_gap');
        const name = boundedText(payload[0], 64);
        if (name === 'c' || name === 'S') {
          if (payload.length < 2) throw failure();
          const identity = boundedText(payload[1], 256, /^[A-Za-z0-9_-]+$/);
          if (name === 'c') receiver.sid = identity; else receiver.gsid = identity;
        } else if (name === 'stop' || name === 'close') throw failure('remote_disconnected');
        else if (name !== 'noop') {
          const queueState = {}, typed = command(name, payload.length > 1 ? payload[1] : {}, queueState);
          if (typed !== null) {
            if (receiver.queue.length >= MAX_QUEUE) throw failure('remote_overflow');
            receiver.sequence += 1;
            receiver.queue.push({ sequence: receiver.sequence, ...typed });
            traceEvent(receiver, 'received', typed.action);
            if (playlistChange(typed)) {
              this.retirePlaylist(receiver);
              const whole = Object.keys(queueState).length > 0;
              receiver.full_playlist = whole ? queueState : null;
              if (!whole) {
                const ids = typed.video_ids.length ? typed.video_ids : typed.video_id ? [typed.video_id] : [];
                const anchor = typed.index < ids.length && ids[typed.index] === typed.video_id ? typed.index : ids.indexOf(typed.video_id);
                if (anchor >= 0 && typed.index >= anchor)
                  receiver.playlist_anchor = { ...typed, sequence: receiver.sequence, video_ids: ids, window_start: typed.index - anchor };
              }
            } else if (name === 'stopVideo') {
              this.retirePlaylist(receiver);
              receiver.full_playlist = null;
            }
          }
        }
        receiver.aid = index;
      }
    }
  }

  async refreshToken(receiver) {
    const info = JSON.parse(new Utf8().feed(await this.response('POST', '/pairing/get_lounge_token_batch',
      { data: { screen_ids: receiver.screen_id }, signal: receiver.abort.signal }), true));
    const screens = record(info) ? info.screens : null;
    if (!Array.isArray(screens) || screens.length > 16) throw failure();
    const matching = screens.filter(entry => record(entry) && entry.screenId === receiver.screen_id);
    if (matching.length !== 1) throw failure();
    receiver.token = boundedText(matching[0].loungeToken, 4096, /^[A-Za-z0-9_.=-]+$/);
  }

  async rebind(receiver) {
    // Pairing changes the screen registration: SmartTube refreshes its token and channel here.
    receiver.abort.abort();
    if (receiver.listening) await receiver.listening;
    if (receiver.error) throw failure(receiver.error);
    receiver.abort = new AbortController();
    await this.refreshToken(receiver);
    if (receiver.error) throw failure(receiver.error);
    receiver.sid = receiver.gsid = '';
    receiver.aid = -1; receiver.rid = 1; receiver.ofs = 0;
    await this.bind(receiver);
    if (receiver.error) throw failure(receiver.error);
    // Native sequence/ack and pending commands survive; only upstream channel indices restart.
    receiver.listening = this.listen(receiver);
  }

  async bind(receiver) {
    const body = await this.response('POST', '/bc/bind', { params: this.params(receiver), data: { count: '0' }, signal: receiver.abort.signal });
    this.accept(receiver, new Frames().feed(body, true));
    if (!receiver.sid || !receiver.gsid) throw failure();
    console.error('YouTube phone state: channel ready');
  }

  fail(receiver, code) {
    this.retirePlaylist(receiver);
    receiver.error = code;
    receiver.full_playlist = null;
    receiver.queue.length = 0;        // never apply stale commands after a failed channel
    receiver.code = '';
    receiver.abort.abort();           // ends the back channel; a no-op when it failed by itself
  }

  // The back channel: one long poll after another, each read as it arrives.
  async listen(receiver) {
    const signal = receiver.abort.signal, expired = () => this.clock() - receiver.touched > LEASE_SECONDS;
    try {
      for (;;) {
        // Native must renew the lease by polling, even while the phone is idle.
        if (expired()) throw failure('remote_expired');
        const parser = new Frames();
        const reply = await this.send('GET', '/bc/bind', { params: this.params(receiver, true), signal, idle_ms: IDLE_MS, max_bytes: MAX_FRAME * 2,
          onChunk: (bytes, status) => {
            if (status !== 200) throw failure('remote_upstream');
            this.accept(receiver, parser.feed(bytes));
            if (expired()) throw failure('remote_expired');
          } });
        if (reply.status !== 200) throw failure('remote_upstream');
        this.accept(receiver, parser.feed(new Uint8Array(0), true));
        // A normal long-poll completion opens a fresh channel. Avoid spinning
        // on empty upstream answers; failures require an explicit restart.
        await pause(250, signal);
      }
    } catch (error) {
      if (signal.aborted) return;       // stopped or failed elsewhere: that reason stands
      this.fail(receiver, error instanceof ServiceError ? error.code : 'remote_upstream');
    }
  }

  async stop(receiver) {
    this.retirePlaylist(receiver);    // a queue read still out is ignored when it returns
    receiver.error = 'remote_stopped';
    receiver.abort.abort();
    if (receiver.listening) await receiver.listening;
    await receiver.lock.run(async () => {
      this.receivers.delete(receiver.session);
      receiver.queue.length = 0;
      try {
        if (receiver.token && receiver.sid) {
          // Closing is best effort, bounded, and never retries an ambiguous
          // request. Local resources close even offline.
          try { await this.response('GET', '/bc/bind', { params: { ...this.params(receiver), TYPE: 'terminate' }, signal: AbortSignal.timeout(2000) }); }
          catch { /* the phone notices when the channel is gone */ }
        }
      } finally {
        receiver.token = receiver.sid = receiver.gsid = receiver.code = '';
        receiver.full_playlist = null;
      }
    });
  }

  start(payload) {
    return this.lock.run(async () => {
      for (const existing of Array.from(this.receivers.values())) {
        if (this.clock() - existing.touched > LEASE_SECONDS || existing.error) await this.stop(existing);
        else if (payload.device_id && existing.device_id === payload.device_id) {
          if (existing.name !== payload.name || existing.profile !== payload.profile || existing.voice_enabled !== payload.voice_enabled ||
              (payload.screen_id && existing.screen_id !== payload.screen_id)) throw failure('remote_identity_conflict', 409);
          existing.touched = this.clock();
          return this.status(existing);
        }
      }
      if (this.receivers.size >= MAX_RECEIVERS) throw failure('remote_capacity', 429);
      const created = newReceiver(hex(16), payload.device_id || uuid4(), payload.name, payload.screen_id, payload.voice_enabled, payload.profile, this.clock());
      try {
        if (!created.screen_id) {
          const raw = await this.response('GET', '/pairing/generate_screen_id');
          created.screen_id = boundedText(new Utf8().feed(raw, true).trim(), 128, /^[A-Za-z0-9_-]+$/);
        }
        await this.refreshToken(created);
        await this.bind(created);
      } catch (error) {
        await this.stop(created);
        throw error instanceof ServiceError ? error : failure('remote_upstream');
      }
      created.touched = this.clock();
      this.receivers.set(created.session, created);
      created.listening = this.listen(created);
      return this.status(created);
    });
  }

  async execute(payload) {
    if (payload.action === 'start') return this.start(payload);
    const receiver = this.receivers.get(payload.session);
    if (!receiver) throw failure('remote_session', 404);
    if (payload.action === 'stop') {
      await this.stop(receiver);
      return { api_version: 1, state: 'stopped' };
    }
    return receiver.lock.run(async () => {
      if (this.receivers.get(payload.session) !== receiver) throw failure('remote_session', 404);
      if (receiver.error || this.clock() - receiver.touched > LEASE_SECONDS) throw failure(receiver.error || 'remote_expired', 409);
      receiver.touched = this.clock();
      if (payload.action === 'poll') {
        if (!(receiver.ack <= payload.ack && payload.ack <= receiver.delivered)) throw failure('remote_ack', 409);
        receiver.ack = payload.ack;
        while (receiver.queue.length && receiver.queue[0].sequence <= payload.ack)
          traceEvent(receiver, 'acknowledged', receiver.queue.shift().action);
        const commands = receiver.queue.slice(0, 32);
        for (const entry of commands) traceEvent(receiver, 'delivered', entry.action);
        if (commands.length) receiver.delivered = Math.max(receiver.delivered, commands.at(-1).sequence);
        return { ...this.status(receiver), commands, ack: receiver.ack };
      }
      try {
        if (payload.action === 'pair') {
          console.error('YouTube phone state: pairing code requested');
          const info = JSON.parse(new Utf8().feed(await this.response('POST', '/pairing/get_pairing_code', { params: { ctx: 'pair' },
            data: { lounge_token: receiver.token, screen_id: receiver.screen_id, screen_name: receiver.name, access_type: 'permanent',
              app: 'lb-v4', device_id: receiver.device_id, qr: '1' } }), true));
          if (!record(info)) throw failure();
          receiver.code = boundedText(info.code, 12, /^(?:[0-9]{9}|[0-9]{12})$/);
          if (receiver.error) throw failure(receiver.error);
          await this.rebind(receiver);
          console.error('YouTube phone state: pairing code ready');
          return { ...this.status(receiver), pairing_code: receiver.code };
        }
        if (payload.player === null || payload.update_id <= 0) throw failure('remote_state', 400);
        const player = payload.player;
        if (payload.update_id === receiver.update_id && same(player, receiver.last_player)) return this.status(receiver);
        if (payload.update_id <= receiver.update_id) throw failure('remote_state_order', 409);
        const packets = Lounge.statePackets(player), form = { count: String(packets.length), ofs: String(receiver.ofs) };
        packets.forEach(([name, args], index) => {
          form[`req${index}__sc`] = name;
          for (const [key, value] of Object.entries(args)) form[`req${index}_${key}`] = String(value);
        });
        await this.response('POST', '/bc/bind', { params: this.params(receiver), data: form });
        // If the back channel failed while this request was in flight, do not
        // publish success to the client or revive the failed receiver.
        if (receiver.error) throw failure(receiver.error);
        receiver.ofs += packets.length;
        receiver.update_id = payload.update_id; receiver.last_player = player;
        Lounge.refreshPlaylistWindow(receiver, player);
        this.schedulePlaylist(receiver, player);
        return this.status(receiver);
      } catch (error) {
        if (error instanceof ServiceError) {
          if (error.status >= 500) this.fail(receiver, error.code);
          throw error;
        }
        this.fail(receiver, 'remote_upstream');
        throw failure('remote_upstream');
      }
    });
  }

  retirePlaylist(receiver) {
    receiver.playlist_anchor = null;
    receiver.playlist_retry = 0;
    if (receiver.playlist_task) { receiver.playlist_task.cancelled = true; receiver.playlist_task = null; }
  }

  static playlistIndices(queue) {
    return queue.video_indices ?? Array.from(queue.video_ids, (_id, offset) => queue.window_start + offset);
  }

  schedulePlaylist(receiver, player) {
    const anchor = receiver.playlist_anchor;
    if (!this.playlistLoader || !anchor || receiver.full_playlist || receiver.playlist_task || this.clock() < receiver.playlist_retry ||
        receiver.ack < anchor.sequence || !/^(?:WL|LL|[A-Za-z0-9_-]{10,100})$/.test(anchor.playlist) || receiver.queue.some(playlistChange)) return;
    const offset = Lounge.playlistIndices(anchor).indexOf(player.index);
    if (player.playlist !== anchor.playlist || player.ctt !== anchor.ctt || offset < 0 || offset >= anchor.video_ids.length ||
        anchor.video_ids[offset] !== player.video_id) return;
    if (anchor.expanded) {
      const margin = Math.min(16, Math.max(1, Math.floor(anchor.video_ids.length / 4)));
      const nearLeft = anchor.window_start > 0 && offset < margin && !player.playlist.startsWith('RD');
      const checked = anchor.checked_index === player.index && (player.state !== 0 || anchor.checked_state === 0);
      if (checked || (!nearLeft && offset < anchor.video_ids.length - margin)) return;
    }
    const task = { cancelled: false };
    receiver.playlist_task = task;
    this.loadPlaylist(receiver, anchor, player, task);   // settles by itself; never rejects
  }

  async loadPlaylist(receiver, anchor, player, task) {
    try {
      const result = await this.playlistLoader({ action: 'remote_playlist', profile: receiver.profile, id: player.video_id,
        playlist: player.playlist, index: player.index });
      if (task.cancelled) return;
      if (!record(result)) throw failure('remote_playlist_unavailable');
      let ids = result.video_ids, start = result.window_start, indices = result.video_indices;
      const whole = value => typeof value === 'number' && Number.isInteger(value);
      if (!Object.hasOwn(result, 'video_indices') && Array.isArray(ids) && whole(start)) indices = Array.from(ids, (_id, offset) => start + offset);
      if (result.api_version !== 1 || result.playlist !== player.playlist || result.index !== player.index || result.video_id !== player.video_id ||
          !Array.isArray(ids) || ids.length < 1 || ids.length > 128 || ids.some(id => typeof id !== 'string' || !VIDEO.test(id)) ||
          !whole(start) || !Array.isArray(indices) || indices.length !== ids.length || indices.some(index => !whole(index) || index < 0 || index > 100000) ||
          indices.some((index, offset) => offset > 0 && indices[offset - 1] >= index) || start !== indices[0] ||
          !indices.includes(player.index) || ids[indices.indexOf(player.index)] !== player.video_id) throw failure('remote_playlist_unavailable');
      await receiver.lock.run(() => {
        const current = receiver.last_player;
        if (task.cancelled || this.receivers.get(receiver.session) !== receiver || receiver.error || receiver.playlist_anchor !== anchor || !current ||
            ['video_id', 'playlist', 'index', 'ctt'].some(key => current[key] !== player[key]) || receiver.queue.some(playlistChange)) return;
        if (same(ids, anchor.video_ids) && same(indices, Lounge.playlistIndices(anchor)) && player.state !== 0) {
          Object.assign(anchor, { expanded: true, checked_index: player.index, checked_state: player.state });
          return;
        }
        if (result.radio_rebased === true && player.playlist.startsWith('RD') && anchor.window_start <= start && start <= player.index) {
          const oldIndices = Lounge.playlistIndices(anchor);
          const before = oldIndices.map((position, offset) => [position, offset]).filter(([position]) => position < start).map(([, offset]) => offset).slice(-32);
          const combined = [...before.map(offset => anchor.video_ids[offset]), ...ids];
          indices = [...before.map(offset => oldIndices[offset]), ...indices];
          const crop = Math.max(0, Math.min(indices.indexOf(player.index) - 32, combined.length - 128));
          indices = indices.slice(crop, crop + 128);
          start = indices[0];
          ids = combined.slice(crop, crop + 128);
        }
        if (receiver.queue.length >= MAX_QUEUE || receiver.sequence >= INT_MAX) return;
        receiver.sequence += 1;
        const update = { sequence: receiver.sequence, action: 'updatePlaylist', video_id: current.video_id, playlist: current.playlist, ctt: current.ctt,
          index: current.index, position_sec: current.position_sec, video_ids: ids, window_start: start };
        if (!same(indices, Array.from(ids, (_id, offset) => start + offset))) update.video_indices = indices;
        receiver.queue.push(update);
        receiver.playlist_anchor = { ...update, expanded: true, checked_index: player.index, checked_state: player.state };
      });
    } catch {
      // Preserve the supplied queue when optional expansion is unavailable.
    } finally {
      if (receiver.playlist_task === task) {
        receiver.playlist_task = null;
        receiver.playlist_retry = this.clock() + 10;
      }
    }
  }

  static refreshPlaylistWindow(receiver, player) {
    const saved = receiver.full_playlist;
    if (!saved || receiver.queue.length >= MAX_QUEUE || receiver.sequence >= INT_MAX) return;
    if (receiver.queue.some(playlistChange)) return;
    const index = player.index, local = index - saved.base, ids = saved.ids;
    if (!player.video_id || !(player.has_previous || player.has_next || player.state === 0) || player.playlist !== saved.playlist ||
        player.ctt !== saved.ctt || local < 0 || local >= ids.length || ids[local] !== player.video_id) return;
    if ((saved.window_start === saved.base || index >= saved.window_start + 16) &&
        (saved.window_end === saved.base + ids.length - 1 || index <= saved.window_end - 16)) return;
    const start = Math.max(0, Math.min(local - 32, ids.length - 128));
    if (saved.window_start === saved.base + start) return;
    receiver.sequence += 1;
    receiver.queue.push({ sequence: receiver.sequence, action: 'updatePlaylist', video_id: player.video_id, video_ids: ids.slice(start, start + 128),
      playlist: saved.playlist, ctt: saved.ctt, index, window_start: saved.base + start, position_sec: player.position_sec });
    saved.window_start = saved.base + start; saved.window_end = saved.base + start + 127;
  }

  static statePackets(player) {
    const clock = { currentTime: float(player.position_sec), duration: float(player.duration_sec), loadedTime: '0', seekableStartTime: '0',
      seekableEndTime: float(player.duration_sec) };
    return [['nowPlaying', { ...clock, videoId: player.video_id, state: player.state, listId: player.playlist, currentIndex: player.index, ctt: player.ctt }],
      ['onStateChange', { ...clock, state: player.state }],
      ['onVolumeChanged', { volume: player.volume, muted: String(player.muted) }],
      ['onHasPreviousNextChanged', { hasPrevious: String(player.has_previous), hasNext: String(player.has_next) }],
      ['onSubtitlesTrackChanged', { videoId: player.video_id, vss_id: player.vss_id, languageCode: player.language, sourceLanguageCode: player.language }]];
  }
}
