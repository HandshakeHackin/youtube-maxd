// What the commands and the HTTP service share: scoped fetches, the library
// adapter with the companion's bounds and error mapping, and the playback
// client with its short-lived extraction cache. SPDX-License-Identifier: GPL-3.0-or-later
import { cacheDirectory } from './platform.js';
import { Innertube, Log, Player, UniversalCache } from 'youtubei.js';
import { Library, LibraryError } from '../../companion/innertube/library.mjs';
import { DeviceAuthorization, createProfileClient } from '../../companion/innertube/device_auth.mjs';
import { DeviceAuth, Guards, OAuthProfiles } from './accounts.js';
import { ServiceError, resolverError } from './errors.js';
import { PLAYBACK_ORDER, SCRIPTLESS, extract } from './resolve.js';

const host = globalThis.__host;
export const config = host.config ?? {};
Log.setLevel(host.getenv('TUBE_JS_VERBOSE') ? Log.Level.INFO : Log.Level.NONE);
export { LibraryError };

const within = (hostname, list) => list.some(name => hostname === name || hostname.endsWith(`.${name}`));
// A fetch limited to https on the listed domains. API calls treat a redirect
// as an error so no header ever follows one to another origin.
export function scoped(domains, { redirect = 'manual', timeout = 15000, limit = 8 * 1024 * 1024, code = 'unsupported_origin' } = {}) {
  return async (input, init = {}) => {
    const request = new Request(input, init);
    const url = new URL(request.url);
    if (url.protocol !== 'https:' || url.username || url.password || url.port || !within(url.hostname, domains))
      throw new LibraryError(code, 'The service returned an unsupported origin.', 502);
    const response = await fetch(request, { redirect, signal: AbortSignal.timeout(timeout), max_bytes: init.max_bytes ?? limit });
    if (redirect === 'manual' && response.status >= 300 && response.status < 400)
      throw new LibraryError('upstream_redirect', 'The service requested an unsupported redirect.', 502);
    return response;
  };
}
export const youtubeFetch = scoped(['youtube.com', 'google.com', 'googleapis.com', 'gstatic.com']);
// Media reads go to the video servers and may be redirected between them.
export const mediaFetch = scoped(['googlevideo.com', 'youtube.com'], { redirect: 'follow', timeout: 20000 });

export async function deadline(milliseconds, work, onTimeout) {
  let timer;
  try {
    return await Promise.race([work, new Promise((_resolve, reject) => { timer = setTimeout(() => reject(onTimeout()), milliseconds); })]);
  } finally { clearTimeout(timer); }
}

// ---- library (companion/prosperotube/library.py, LibraryService) ----
// Guest plus the accounts signed in on this console. The library object is
// rebuilt whenever an account is added or removed, which retires every client,
// cursor and retained feed it held, as restarting the worker process does.
let library = null, removed = () => {};
export const accounts = new OAuthProfiles(config.mode === 'serve' ? config.state_dir : '');
export const hasProfile = identity => identity === 'guest' || accounts.has(identity);
export const onProfileRemoved = listener => { removed = listener; };
const guards = new Guards(identity => accounts.has(identity));
function rebuild() { library = null; guards.blockWatches(); }
// A refreshed access token is kept; if it cannot be written, the one in
// memory still serves and the next start refreshes again.
function refreshed(identity, credentials) { try { accounts.update(identity, credentials); } catch { /* see above */ } }
async function profileClient(profile, locale) {
  try { return await createProfileClient(profile, Innertube, youtubeFetch, refreshed, locale); }
  catch (error) {
    // Google refused the stored sign-in (revoked, or removed in the account's settings).
    if (profile.oauth && /refresh access token: 40[01]/.test(String(error?.message)))
      throw new LibraryError('authentication_required', 'Sign in to the YouTube account again.', 403);
    throw error;
  }
}
export function catalog() {
  library ??= new Library(profileClient, accounts.profiles.map(profile => structuredClone(profile)));
  return library;
}
const authorization = new DeviceAuthorization(() => Innertube.create({ client_type: 'TVHTML5', retrieve_player: false,
  retrieve_innertube_config: false, generate_session_locally: true, fetch: youtubeFetch }), youtubeFetch);
// The offline tests run the sign-in clock faster; outside them it is the real one.
const pace = host.getenv('TUBE_JS_FIXTURES') ? Number(host.getenv('TUBE_JS_AUTH_CLOCK')) || 1 : 1;
export const deviceAuth = new DeviceAuth(request => libraryExecute(request, 'device'), accounts, rebuild, () => host.now() / 1000 * pace);

// library._remove_profile: the private file first; a failed write keeps the
// account, its clients and its sessions. Nothing is changed on YouTube.
function removeProfile(request) {
  const identity = request.profile ?? 'guest';
  if (identity === 'guest') throw new ServiceError('account_required', 'Select an account profile first.', 400);
  if (!accounts.has(identity)) throw new ServiceError('unknown_profile', 'Select an available profile.', 404);
  accounts.remove(identity);
  guards.forget(identity);
  rebuild();
  removed(identity);
  return { api_version: 1, success: true, profile: identity };
}

// One operation of the library or of sign-in, with only the adapter's bounded
// public errors crossing this boundary.
async function send(request, operation) {
  let result;
  try {
    result = operation === 'device' ? await authorization.execute(request) : operation === 'playback' ? await catalog().playback(request) :
      operation === 'client' ? await catalog().client(request.profile) : await catalog().execute(request);
  } catch (error) {
    if (!(error instanceof LibraryError)) {
      if (host.getenv('TUBE_JS_TRACE')) console.error(error?.stack ?? error);
      throw new ServiceError('upstream_failure', 'YouTube could not complete this operation.', 502);
    }
    const code = typeof error.code === 'string' && /^[a-z_]{1,64}$/.test(error.code) ? error.code : 'library_failure';
    const status = Number.isInteger(error.status) && error.status >= 400 && error.status <= 599 ? error.status : 502;
    throw new ServiceError(code, typeof error.message === 'string' && error.message.length <= 256 ? error.message : 'The library operation failed.', status);
  }
  if (operation === 'client') return result;
  if (!result || typeof result !== 'object' || Array.isArray(result))
    throw new ServiceError('library_transport', 'The library worker returned invalid operation data.');
  if (JSON.stringify(result).length > 1048576) throw new ServiceError('response_limit', 'The returned operation exceeds the response limit.', 502);
  return result;
}

let pending = 0, queue = Promise.resolve();
// `operation`: execute (a library action, behind the one-attempt guards),
// playback (catalog pages), device (one sign-in step) or client (the profile's
// session itself, for the signed-in playback path).
export async function libraryExecute(request, operation = 'execute') {
  if (!request || typeof request !== 'object' || host.utf8Encode(JSON.stringify(request)).byteLength > (operation === 'device' ? 16384 : 8192))
    throw new ServiceError('library_request_limit', 'The library operation exceeds its request limit.', 400);
  if (pending >= 16) throw new ServiceError('library_busy', 'The library is busy. Retry shortly.', 429);
  pending++;
  // One operation at a time, as the worker process does: the adapter keeps
  // cursors and retained feeds that must not interleave.
  const run = queue.then(() => operation !== 'execute' ? send(request, operation) :
    request.action === 'profile_remove' ? removeProfile(request) : guards.run(request, () => send(request, operation)));
  queue = run.catch(() => {});
  try {
    return await deadline(30000, run, () => new ServiceError('library_timeout', 'The operation timed out. Check the account before repeating a change.', 504));
  } finally { pending--; }
}

// ---- playback client ----
// The session (visitor identity) comes from YouTube: with a locally generated
// one the app clients answered "Sign in to confirm you're not a bot" (checked
// live on 2026-10-11). The player script is not loaded: the default client's
// addresses arrive ready. Its analysis costs seconds and hundreds of megabytes
// in this engine, so it only ever runs when the host allowed it explicitly.
// The pinned parser drops a few facts of the player answer that the resolve
// record carries (paid promotion, Shorts eligibility), so the playback client's
// fetch keeps those from the raw JSON, by video.
const playerFacts = new Map();
export const rawPlayerFacts = id => playerFacts.get(id) ?? {};
async function playbackFetch(input, init) {
  const response = await youtubeFetch(input, init);
  const url = typeof input === 'string' ? input : input.url ?? String(input);
  if (!response.ok || !/\/youtubei\/v1\/player(\?|$)/.test(url)) return response;
  const body = await response.text();
  try {
    const raw = JSON.parse(body), id = raw?.videoDetails?.videoId, micro = raw?.microformat?.playerMicroformatRenderer;
    if (typeof id === 'string') {
      const overlay = raw.paidContentOverlay?.paidContentOverlayRenderer?.text;
      const paid = typeof overlay?.simpleText === 'string' ? overlay.simpleText : Array.isArray(overlay?.runs) ?
        overlay.runs.slice(0, 64).map(run => typeof run?.text === 'string' ? run.text.slice(0, 512) : '').join('') : '';
      if (playerFacts.size >= 32) playerFacts.delete(playerFacts.keys().next().value);
      playerFacts.set(id, { paid, shorts: micro?.isShortsEligible === true || /\/shorts\//.test(String(micro?.canonicalUrl ?? '')) ? true : micro ? false : undefined,
        keys: Object.keys(raw), micro: micro ? Object.keys(micro) : null });
    }
  } catch { /* the parser will report a malformed answer */ }
  return new Response(body, { status: response.status, headers: response.headers, url: response.url });
}

const LANGUAGE = { 'en-NZ': 'en-GB', he: 'iw', mo: 'ro', nb: 'no', 'pt-BR': 'pt', tjl: '' };
const cache = () => new UniversalCache(true, cacheDirectory());
const clients = new Map();
export function playbackClient(hl = '') {
  const lang = Object.hasOwn(LANGUAGE, hl) ? LANGUAGE[hl] : hl;
  if (!clients.has(lang)) {
    if (clients.size >= 4) clients.delete(clients.keys().next().value);
    const created = Innertube.create({ cache: cache(), retrieve_player: false, retrieve_innertube_config: false,
      generate_session_locally: !!host.getenv('TUBE_JS_LOCAL_SESSION'), enable_session_cache: false, fetch: playbackFetch, ...(lang ? { lang } : {}) });
    created.catch(() => { if (clients.get(lang) === created) clients.delete(lang); });
    clients.set(lang, created);
  }
  return clients.get(lang);
}
export async function prepare(yt, client) {
  if (SCRIPTLESS.includes(client) || yt.session.player) return;
  if (!config.allow_player_script)
    throw new ServiceError('extraction_failed', 'This client needs the player script, which is not enabled.', 502);
  yt.session.player = await Player.create(cache(), youtubeFetch);
}

// extractor.Extractor.extract for one video: at most two at a time, 45 s, and
// a 30 s cache so details, resolve and a retry share one player request.
const extractions = new Map();
let running = 0;
const waiters = [];
export async function extractVideo(id, { hl = '', fresh = false, order = PLAYBACK_ORDER } = {}) {
  const key = `${id}|${hl}|${order.join(',')}`, now = host.now();
  for (const [name, entry] of extractions) if (entry.until <= now) extractions.delete(name);
  if (!fresh && extractions.has(key)) return extractions.get(key).value;
  const work = (async () => {
    if (running >= 2) await new Promise(resolve => waiters.push(resolve));
    running++;
    try {
      const yt = await playbackClient(hl).catch(() => { throw resolverError('resolver_network'); });
      return await extract(yt, id, { order, prepare: client => prepare(yt, client), fetch: mediaFetch, facts: rawPlayerFacts });
    } finally { running--; waiters.shift()?.(); }
  })();
  const value = deadline(45000, work, () => new ServiceError('timeout', 'YouTube took too long to respond. Retry from the video screen.', 504));
  if (extractions.size >= 128) extractions.delete(extractions.keys().next().value);
  extractions.set(key, { until: now + 30000, value });
  value.catch(() => { if (extractions.get(key)?.value === value) extractions.delete(key); });
  return value;
}

export function tokenUrlsafe(bytes) {
  const data = new Uint8Array(host.random(bytes));
  return btoa(String.fromCharCode(...data)).replace(/\+/g, '-').replace(/\//g, '_').replace(/=+$/, '');
}
