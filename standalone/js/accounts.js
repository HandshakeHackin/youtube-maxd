// Accounts on the console: the private credential file, device-code sign-in
// and the one-attempt guards of account changes.
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Ports of companion/prosperotube/device_auth.py (OAuthProfiles, DeviceAuth)
// and of the guards in library.py (LibraryService). What differs from the
// Python service: one process and one thread, so the store needs no lock file
// and no snapshot comparison; and the guards do not have to outlive a worker
// process, only the library object, which is rebuilt whenever a profile is
// added or removed.
import { validCredentials } from '../../companion/innertube/device_auth.mjs';
import { ServiceError } from './errors.js';

const host = globalThis.__host;
const STORE_LIMIT = 196608, PROFILE_LIMIT = 8;
const hex = bytes => Array.from(new Uint8Array(host.random(bytes)), byte => byte.toString(16).padStart(2, '0')).join('');
const points = value => Array.from(value).length;   // code points, as Python counts
const scalar = (value, max) => typeof value === 'string' && value.length > 0 && value.length <= max && !/[^\x21-\x7e]/.test(value);
const keysAre = (value, names) => value && typeof value === 'object' && !Array.isArray(value) && Object.keys(value).sort().join(',') === names;
const validName = name => typeof name === 'string' && points(name) >= 1 && points(name) <= 150;

// ---- the credential file (device_auth.OAuthProfiles) ----
// `<state dir>/accounts/oauth-profiles.json`, mode 0600 in a 0700 directory:
// {"version": 1, "profiles": [{"id": "oauth_<16 hex>", "name", "cookie": "", "oauth": {access_token, refresh_token,
// expiry_date, client: {client_id, client_secret}}}]}. Written through a temporary file, flushed, then renamed over
// the old one. A file that cannot be understood is left exactly as it is: accounts are then unavailable until it is
// removed by hand, and nothing is ever written over it.
export class OAuthProfiles {
  constructor(directory) {
    this.path = directory ? `${directory}/accounts/oauth-profiles.json` : '';
    this.profiles = []; this.broken = false;
    const stored = this.path ? host.readFile(this.path) : undefined;
    if (stored === undefined) return;
    try {
      if (stored.byteLength > STORE_LIMIT) throw new Error('size');
      const data = JSON.parse(host.utf8Decode(stored));
      if (!keysAre(data, 'profiles,version') || data.version !== 1 || !Array.isArray(data.profiles) || data.profiles.length > PROFILE_LIMIT) throw new Error('format');
      const seen = new Set();
      for (const profile of data.profiles) {
        if (!keysAre(profile, 'cookie,id,name,oauth') || typeof profile.id !== 'string' || !/^oauth_[0-9a-f]{16}$/.test(profile.id) || seen.has(profile.id) ||
            profile.cookie !== '' || !validName(profile.name) || !validCredentials(profile.oauth)) throw new Error('profile');
        seen.add(profile.id);
      }
      this.profiles = data.profiles;
    } catch { this.broken = true; }
  }
  get available() { return !!this.path && !this.broken; }
  has(identity) { return this.profiles.some(profile => profile.id === identity); }
  full() { return this.profiles.length >= PROFILE_LIMIT; }
  #save(profiles) {
    if (!this.available) throw new ServiceError('auth_storage', 'The account store on this console could not be read and was left untouched.', 507);
    const text = JSON.stringify({ version: 1, profiles });
    if (host.utf8Encode(text).byteLength > STORE_LIMIT) throw new ServiceError('auth_storage', 'The account credential store is full.', 507);
    // Committed only when the file reads back as written.
    const written = host.writeFile(this.path, host.utf8Encode(text)) === true ? host.readFile(this.path) : undefined;
    if (written === undefined || host.utf8Decode(written) !== text)
      throw new ServiceError('auth_storage', 'The account could not be written to private storage.', 507);
    this.profiles = profiles;
  }
  append(credentials, name) {
    if (!validCredentials(credentials) || !validName(name)) throw new ServiceError('auth_response', 'The account credentials are invalid.', 502);
    if (this.full()) throw new ServiceError('auth_profiles_full', 'At most eight account profiles are supported.', 409);
    const profile = { id: `oauth_${hex(8)}`, name, cookie: '', oauth: credentials };
    this.#save([...this.profiles, profile]);
    return profile;
  }
  // A refreshed access token for a stored account; the client identity never changes.
  update(identity, credentials) {
    const existing = this.profiles.find(profile => profile.id === identity);
    if (!existing || !validCredentials(credentials) || credentials.client.client_id !== existing.oauth.client.client_id ||
        credentials.client.client_secret !== existing.oauth.client.client_secret)
      throw new ServiceError('auth_response', 'The refreshed account credentials are invalid.', 502);
    this.#save(this.profiles.map(profile => profile.id === identity ? { ...profile, oauth: credentials } : profile));
  }
  remove(identity) {
    if (!this.has(identity)) throw new ServiceError('unknown_profile', 'Select an available profile.', 404);
    this.#save(this.profiles.filter(profile => profile.id !== identity));
  }
}

// ---- device-code sign-in (device_auth.DeviceAuth) ----
// One active session, sixteen retained for an hour, no timer: each status
// request performs at most one token poll and only after the interval YouTube
// asked for (five seconds at least). `execute` runs one operation of
// companion/innertube/device_auth.mjs. What the caller sees never contains the
// device code, the client identity or a token.
const VERIFY_HOSTS = ['www.google.com', 'google.com', 'www.youtube.com', 'youtube.com'];
function verificationUrl(value) {
  if (typeof value !== 'string' || value.length > 256 || /[?#]/.test(value)) return false;
  try {
    const url = new URL(value);
    return url.protocol === 'https:' && !url.username && !url.password && !url.port && VERIFY_HOSTS.includes(url.hostname) && ['/device', '/activate'].includes(url.pathname);
  } catch { return false; }
}
const wholeIn = (value, low, high) => Number.isInteger(value) && value >= low && value <= high;

export class DeviceAuth {
  constructor(execute, store, register, clock = () => host.now() / 1000) {
    this.execute = execute; this.store = store; this.register = register; this.clock = clock;
    this.sessions = new Map(); this.active = null;
  }
  forgetProfile(identity) {
    for (const session of this.sessions.values()) if (session.profile?.id === identity) { delete session.profile; session.state = 'canceled'; }
  }
  #get(identity) {
    const session = this.sessions.get(identity);
    if (!session) throw new ServiceError('auth_session', 'Start a new sign-in session.', 404);
    if (session.state === 'pending' && this.clock() >= session.deadline) {
      session.state = 'expired';
      delete session.client; delete session.device_code;
      if (this.active === session) this.active = null;
    }
    return session;
  }
  #public(session) {
    const value = { session: session.id, state: session.state };
    if (session.state === 'pending') Object.assign(value, { user_code: session.user_code, verification_url: session.verification_url,
      expires_in: Math.max(0, Math.ceil(session.deadline - this.clock())), poll_after: Math.max(0, Math.ceil(session.next_poll - this.clock())) });
    if (session.state === 'authorized') value.profile = session.profile;
    if (session.state === 'failed') value.error = session.error || 'YouTube sign-in could not be completed. Start a new session.';
    return value;
  }
  // `owner` is the job that asked, so a cancelled job can withdraw what it started.
  async start(name = 'YouTube account', owner = null) {
    if (this.active && ['starting', 'pending'].includes(this.active.state)) {
      if (this.active.state === 'pending') this.#get(this.active.id);
      if (this.active) throw new ServiceError('auth_busy', 'A sign-in session is already pending.', 409);
    }
    if (!this.store.available) throw new ServiceError('auth_storage', 'The account store on this console could not be read and was left untouched.', 507);
    if (this.store.full()) throw new ServiceError('auth_profiles_full', 'At most eight account profiles are supported.', 409);
    const now = this.clock();
    for (const [key, session] of this.sessions) if (session.retained_until <= now) this.sessions.delete(key);
    if (this.sessions.size >= 16) throw new ServiceError('auth_busy', 'Too many sign-in sessions are retained.', 429);
    if (typeof name !== 'string' || points(name.trim()) < 1 || points(name.trim()) > 150 || /[\x00-\x1f\x7f]/.test(name))
      throw new ServiceError('auth_name', 'Enter a valid account name.', 400);
    const session = { id: hex(16), state: 'starting', polling: false, task: owner };
    this.active = session;
    try {
      const result = await this.execute({ action: 'start' });
      if (this.active !== session || session.state !== 'starting') throw new Error('withdrawn');
      const client = result?.client;
      if (!keysAre(client, 'client_id,client_secret') || !scalar(client.client_id, 512) || !scalar(client.client_secret, 512) ||
          !scalar(result.device_code, 8192) || typeof result.user_code !== 'string' || !/^[A-Za-z0-9-]{1,32}$/.test(result.user_code) ||
          !verificationUrl(result.verification_url) || !wholeIn(result.expires_in, 1, 1800) || !wholeIn(result.interval, 1, 60))
        throw new Error('response');
      const started = this.clock(), interval = Math.max(5, result.interval);
      Object.assign(session, { state: 'pending', name: name.trim(), client, device_code: result.device_code, user_code: result.user_code,
        verification_url: result.verification_url, deadline: started + result.expires_in, interval, next_poll: started + interval,
        retained_until: started + 3600 });
      this.sessions.set(session.id, session);
      return this.#public(session);
    } catch {
      session.state = 'failed';
      if (this.active === session) this.active = null;
      throw new ServiceError('auth_failed', 'YouTube sign-in could not be started. Start a new session.', 502);
    } finally { delete session.task; }
  }
  async status(identity, owner = null) {
    const session = this.#get(identity);
    if (session.state !== 'pending' || session.polling || this.clock() < session.next_poll) return this.#public(session);
    session.polling = true; session.task = owner;
    session.next_poll = this.clock() + session.interval;
    try {
      const result = await this.execute({ action: 'poll', client: session.client, device_code: session.device_code });
      // Credentials that arrive after a cancellation or the expiry are dropped.
      if (this.active !== session || this.#get(identity).state !== 'pending') return this.#public(session);
      if (result?.state === 'pending' && typeof result.slow_down === 'boolean') {
        if (result.slow_down) session.interval = Math.min(60, session.interval + 5);
        session.next_poll = this.clock() + session.interval;
      } else if (['denied', 'expired'].includes(result?.state)) { session.state = result.state; this.active = null; }
      else if (result?.state === 'authorized' && validCredentials(result.credentials)) {
        // Stored and registered without a pause in between: a cancellation cannot fall between the two.
        const profile = this.store.append(result.credentials, session.name);
        this.register(profile);
        session.state = 'authorized'; session.profile = { id: profile.id, name: profile.name };
        this.active = null;
      } else throw new Error('response');
      return this.#public(session);
    } catch (error) {
      // An uncertain redemption is never tried again: the session ends. A known
      // local storage failure is actionable and must survive subsequent status reads.
      if (session.state === 'pending') session.state = 'failed';
      if (this.active === session) this.active = null;
      const failure = error instanceof ServiceError && error.code === 'auth_storage' ? error :
        new ServiceError('auth_failed', 'YouTube sign-in could not be completed. Start a new session.', 502);
      session.error = failure.message;
      throw failure;
    } finally {
      session.polling = false; delete session.task;
      if (session.state !== 'pending') { delete session.client; delete session.device_code; }
    }
  }
  cancel(identity) {
    const session = this.#get(identity);
    if (session.state === 'pending') {
      session.state = 'canceled';
      delete session.client; delete session.device_code;
      if (this.active === session) this.active = null;
    }
    return this.#public(session);
  }
  // A job that was cancelled while its request was still out.
  abandon(owner) {
    const session = this.active;
    if (!session || !owner || session.task !== owner) return;
    if (session.state === 'pending') this.cancel(session.id);
    else { session.state = 'canceled'; this.active = null; }
  }
}

// ---- one attempt per intent (library.LibraryService) ----
// The library modules refuse to repeat a change of their own. These records
// sit one level above them, so they also hold when the library object was
// rebuilt in between (a profile added or removed) or an operation timed out:
// a change that was sent and not confirmed is never sent a second time under
// the same confirmation. They live in memory for thirty minutes and end with
// the app; see docs/DECISIONS.md for why that is enough.
const SESSION = /^[A-Za-z0-9_-]{1,64}$/, VIDEO = /^[A-Za-z0-9_-]{11}$/, OWNER = /^[A-Za-z0-9_-]{1,32}$/;
const TTL = 1800000;
const filled = value => typeof value === 'string' && value !== '';
const pick = (result, keys) => Object.fromEntries(keys.map(key => [key, result[key]]));
const ACCOUNT_RESULTS = { playlist_save: ['saved', true], playlist_unsave: ['saved', false], history_pause: ['history', 'paused'],
  history_resume: ['history', 'active'], history_clear: ['history', 'cleared'] };
const SESSION_INTENTS = {
  stream_reminder: [['id', 'reminder'], 'reminder_unconfirmed', ['api_version', 'success', 'id', 'reminder_set']],
  playlist_order: [['playlist', 'playlist_order'], 'playlist_unconfirmed', ['api_version', 'success', 'playlist_id', 'order']],
  comment_post: [['id', 'text'], 'comment_unconfirmed', ['api_version', 'success', 'id', 'session', 'reply']],
  comment_reply: [['id', 'cursor', 'text'], 'comment_unconfirmed', ['api_version', 'success', 'id', 'session', 'reply']]
};
// The library sent nothing for these, so the confirmation may be used again.
const NOTHING_SENT = ['history_control_unavailable', 'reminder_unavailable', 'playlist_order_unavailable'];
const refused = (code, message, status = 502) => new ServiceError(code, message, status);

// Per kind of change: its request check, the record's key and identity, the
// codes of a refused repeat and a full table, and what a confirmed answer is.
const KINDS = [
  { name: 'chat', applies: r => r.action === 'chat_send',
    valid: r => SESSION.test(r.session ?? '') && SESSION.test(r.cursor ?? '') && VIDEO.test(r.id ?? '') && typeof r.text === 'string' &&
      !!r.text.trim() && points(r.text) <= 200 && !/[\x00-\x1f\x7f]/.test(r.text),
    invalid: ['invalid_message', 'Enter a message of 1 to 200 characters.'],
    key: r => r.session, identity: r => `${r.cursor}\n${r.id}\n${host.sha256(r.text)}`,
    repeat: () => ['chat_unconfirmed', 'Check the chat; this message intent cannot be repeated.'], busy: ['chat_busy', 'Too many message intents are retained.'],
    confirm(r, result) {
      const message = result.message_id;
      if (result.api_version !== 1 || result.success !== true || result.id !== r.id || result.session !== r.session || typeof message !== 'string' ||
          points(message) < 1 || points(message) > 160 || /[\x00-\x1f\x7f]/.test(message)) throw refused('chat_unconfirmed', 'YouTube did not confirm this message.');
      return pick(result, ['api_version', 'success', 'id', 'session', 'message_id']);
    } },
  { name: 'comment', applies: r => r.action === 'comment_rate',
    valid: r => SESSION.test(r.cursor ?? '') && ['like', 'dislike', 'none'].includes(r.rating),
    invalid: ['invalid_rating', 'Select an available comment rating.'],
    key: r => r.cursor, identity: r => r.rating,
    repeat: () => ['comment_unconfirmed', 'Reload the comments; this rating intent cannot be repeated.'], busy: ['comment_busy', 'Too many comment rating intents are retained.'],
    confirm(r, result) {
      if (result.success !== true || result.rating !== r.rating) throw refused('comment_unconfirmed', 'YouTube did not accept the comment rating.');
      return result;
    } },
  { name: 'watched', applies: r => r.action === 'mark_watched',
    valid: r => SESSION.test(r.session ?? '') && VIDEO.test(r.id ?? ''),
    invalid: ['invalid_progress', 'A video and confirmation session are required.'],
    key: r => r.session, identity: r => r.id,
    repeat: () => ['history_unconfirmed', 'Check account history before repeating this change.'], busy: ['history_busy', 'Too many history changes are retained.'],
    confirm(r, result) {
      const position = result.position_sec;
      if (result.api_version !== 1 || result.success !== true || result.video_id !== r.id || typeof position !== 'number' || !(position > 0 && position <= 1000000))
        throw refused('history_unconfirmed', 'YouTube did not confirm this video as watched.');
      return result;
    } },
  { name: 'feedback', applies: r => r.action === 'feedback',
    valid: r => SESSION.test(r.cursor ?? '') && VIDEO.test(r.id ?? '') && ['not_interested', 'dont_recommend_channel', 'hide'].includes(r.feedback),
    invalid: ['invalid_feedback', 'Select an available feedback choice.'],
    key: r => `${r.cursor}\n${r.feedback}`, identity: r => r.id,
    repeat: () => ['feedback_unconfirmed', 'Check the list before repeating this feedback.'], busy: ['feedback_busy', 'Too many feedback intents are retained.'],
    confirm(r, result) {
      if (result.api_version !== 1 || result.success !== true || result.video_id !== r.id || result.feedback !== r.feedback)
        throw refused('feedback_unconfirmed', 'YouTube did not confirm this feedback.');
      return pick(result, ['api_version', 'success', 'video_id', 'feedback']);
    } },
  { name: 'account', applies: r => Object.hasOwn(ACCOUNT_RESULTS, r.action),
    valid: r => SESSION.test(r.session ?? '') && (!r.action.startsWith('playlist_') || (typeof r.playlist === 'string' && !['WL', 'LL'].includes(r.playlist) &&
      /^[A-Za-z0-9_-]{10,100}$/.test(r.playlist))),
    invalid: ['invalid_request', 'The account change needs a confirmation session and a valid target.'],
    key: r => r.session, identity: r => `${r.action}\n${r.action.startsWith('playlist_') ? r.playlist : ''}`,
    repeat: r => [r.action.startsWith('playlist_') ? 'playlist_unconfirmed' : 'history_unconfirmed', 'Check the account before repeating this change.'],
    busy: ['account_busy', 'Too many account changes are retained.'],
    confirm(r, result) {
      const [field, expected] = ACCOUNT_RESULTS[r.action], saving = field === 'saved';
      if (result.api_version !== 1 || result.success !== true || result[field] !== expected || (saving && result.playlist_id !== r.playlist))
        throw refused(saving ? 'playlist_unconfirmed' : 'history_unconfirmed', 'YouTube did not confirm the account change.');
      return pick(result, saving ? ['api_version', 'success', 'playlist_id', 'saved'] : ['api_version', 'success', 'history']);
    } },
  // Reads of the same actions (`status`, an empty order) are not changes.
  { name: 'session', applies: r => Object.hasOwn(SESSION_INTENTS, r.action) && !(r.action === 'stream_reminder' && r.reminder === 'status') &&
      !(r.action === 'playlist_order' && !r.playlist_order),
    valid: r => SESSION.test(r.session ?? '') && SESSION_INTENTS[r.action][0].every(field => filled(r[field])),
    invalid: ['invalid_request', 'The change needs a confirmation session and a valid target.'],
    key: r => r.session, identity: r => `${r.action}\n${host.sha256(SESSION_INTENTS[r.action][0].map(field => r[field]).join('\0'))}`,
    repeat: r => [SESSION_INTENTS[r.action][1], 'Check the account before repeating this change.'], busy: ['account_busy', 'Too many account changes are retained.'],
    confirm(r, result) {
      const expected = r.action === 'stream_reminder' ? { id: r.id, reminder_set: r.reminder === 'set' } :
        r.action === 'playlist_order' ? { playlist_id: r.playlist, order: r.playlist_order } : { id: r.id, session: r.session, reply: r.action === 'comment_reply' };
      if (result.api_version !== 1 || result.success !== true || Object.entries(expected).some(([key, value]) => result[key] !== value))
        throw refused(SESSION_INTENTS[r.action][1], 'YouTube did not confirm the change.');
      return pick(result, SESSION_INTENTS[r.action][2]);
    } }
];

export class Guards {
  // has(profile) says whether an account profile exists.
  constructor(has, clock = () => host.now()) {
    this.has = has; this.clock = clock;
    this.records = Object.fromEntries(KINDS.map(kind => [kind.name, new Map()]));
    this.watches = new Map();
  }
  #owner(owner) {
    if (owner === 'guest') throw new ServiceError('account_required', 'Select an account profile first.', 401);
    if (!this.has(owner)) throw new ServiceError('unknown_profile', 'Select an available profile.', 404);
  }
  // Records of one account end with it.
  forget(owner) {
    for (const map of [...Object.values(this.records), this.watches])
      for (const key of Array.from(map.keys())) if (key.startsWith(`${owner}\n`)) map.delete(key);
  }
  // The tracking objects live in the library; when it is rebuilt, a report
  // already attempted must not be sent again from a new one.
  blockWatches() { for (const state of this.watches.values()) state.blocked = true; }
  #watch(r, create) {
    const owner = r.profile ?? 'guest';
    if (!OWNER.test(owner) || !SESSION.test(r.session ?? '') || !VIDEO.test(r.id ?? '')) throw new ServiceError('invalid_progress', 'The playback report is invalid.', 400);
    this.#owner(owner);
    const now = this.clock(), key = `${owner}\n${r.session}`;
    for (const [name, state] of this.watches) if (state.deadline <= now) this.watches.delete(name);
    let state = this.watches.get(key);
    if (state && state.id !== r.id) throw new ServiceError('invalid_progress', 'The playback session belongs to another video.', 400);
    if (state?.blocked) throw new ServiceError('history_unconfirmed', 'The history change was not confirmed; this session will not repeat it.', 409);
    if (!state && create) {
      if (this.watches.size >= 64) throw new ServiceError('history_busy', 'Too many playback sessions are retained.', 429);
      state = { id: r.id, blocked: false, deadline: now + TTL };
      this.watches.set(key, state);
    }
    return state;
  }
  #record(kind, r, create) {
    const owner = r.profile ?? 'guest';
    if (!OWNER.test(owner) || !kind.valid(r)) throw new ServiceError(...kind.invalid, 400);
    this.#owner(owner);
    const map = this.records[kind.name], now = this.clock(), key = `${owner}\n${kind.key(r)}`, identity = kind.identity(r);
    for (const [name, state] of map) if (state.deadline <= now) map.delete(name);
    let state = map.get(key);
    if (state && (state.identity !== identity || state.result === null)) throw new ServiceError(...kind.repeat(r), 409);
    if (!state && create) {
      if (map.size >= 256) throw new ServiceError(...kind.busy, 429);
      state = { identity, result: null, deadline: now + TTL };
      map.set(key, state);
    }
    return state;
  }
  // Runs one library operation behind the guards. `send` performs it.
  async run(request, send) {
    const kind = KINDS.find(candidate => candidate.applies(request)), watching = request.action === 'watch_progress';
    // A confirmed change asked for again answers with what was confirmed.
    const known = kind ? this.#record(kind, request, false) : null;
    if (known) return { ...known.result };
    if (watching) this.#watch(request, false);
    const record = kind ? this.#record(kind, request, true) : null, watch = watching ? this.#watch(request, true) : null;
    if (watch) watch.blocked = true;
    let result;
    try { result = await send(); }
    catch (error) {
      if (record && error instanceof ServiceError && NOTHING_SENT.includes(error.code) && ['account', 'session'].includes(kind.name)) {
        const map = this.records[kind.name], key = `${request.profile ?? 'guest'}\n${kind.key(request)}`;
        if (map.get(key) === record) map.delete(key);
      }
      throw error;
    }
    if (record) { record.result = kind.confirm(request, result); result = { ...record.result }; }
    if (watch) {
      if (result.success !== true || typeof result.position_sec !== 'number' || result.position_sec !== request.position_sec)
        throw new ServiceError('history_unconfirmed', 'The history change was not confirmed; this session will not repeat it.', 502);
      watch.blocked = false; watch.deadline = this.clock() + TTL;
    }
    return result;
  }
}
