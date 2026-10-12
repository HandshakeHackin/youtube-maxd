// What the commands and the HTTP service share: scoped fetches, the library
// adapter with the companion's bounds and error mapping, and the playback
// client with its short-lived extraction cache. SPDX-License-Identifier: GPL-3.0-or-later
import { cacheDirectory } from './platform.js';
import { Innertube, Log, Player, UniversalCache } from 'youtubei.js';
import { Library, LibraryError } from '../../companion/innertube/library.mjs';
import { createProfileClient } from '../../companion/innertube/device_auth.mjs';
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

// ---- library (companion/prosperotube/library.py, LibraryService.execute, for the Guest profile) ----
let library = null;
export function catalog() {
  library ??= new Library((profile, locale) => createProfileClient(profile, Innertube, youtubeFetch, () => {}, locale), []);
  return library;
}
const VIDEO = /^[A-Za-z0-9_-]{11}$/, SESSION = /^[A-Za-z0-9_-]{1,64}$/;
const filled = value => typeof value === 'string' && value !== '';
// Changes that belong to an account are refused before anything is sent, with
// the same validation order and codes as the Python service.
function accountGuard(r) {
  const owner = r.profile ?? 'guest';
  const need = (ok, code, message) => { if (!ok) throw new ServiceError(code, message, 400); };
  switch (r.action) {
    case 'profile_remove':
      throw owner === 'guest' ? new ServiceError('account_required', 'Select an account profile first.', 400) :
        new ServiceError('unknown_profile', 'Select an available profile.', 404);
    case 'chat_send':
      need(SESSION.test(r.session ?? '') && SESSION.test(r.cursor ?? '') && VIDEO.test(r.id ?? '') && typeof r.text === 'string' &&
        r.text.trim() && Array.from(r.text).length <= 200 && !/[\x00-\x1f\x7f]/.test(r.text), 'invalid_message', 'Enter a message of 1 to 200 characters.');
      break;
    case 'comment_rate':
      need(SESSION.test(r.cursor ?? '') && ['like', 'dislike', 'none'].includes(r.rating), 'invalid_rating', 'Select an available comment rating.');
      break;
    case 'watch_progress':
      need(SESSION.test(r.session ?? '') && VIDEO.test(r.id ?? ''), 'invalid_progress', 'The playback report is invalid.');
      break;
    case 'mark_watched':
      need(SESSION.test(r.session ?? '') && VIDEO.test(r.id ?? ''), 'invalid_progress', 'A video and confirmation session are required.');
      break;
    case 'feedback':
      need(SESSION.test(r.cursor ?? '') && VIDEO.test(r.id ?? '') && ['not_interested', 'dont_recommend_channel', 'hide'].includes(r.feedback),
        'invalid_feedback', 'Select an available feedback choice.');
      break;
    case 'playlist_save': case 'playlist_unsave': case 'history_pause': case 'history_resume': case 'history_clear':
      need(SESSION.test(r.session ?? '') && (!r.action.startsWith('playlist_') || (typeof r.playlist === 'string' && !['WL', 'LL'].includes(r.playlist) &&
        /^[A-Za-z0-9_-]{10,100}$/.test(r.playlist))), 'invalid_request', 'The account change needs a confirmation session and a valid target.');
      break;
    case 'stream_reminder': case 'playlist_order': case 'comment_post': case 'comment_reply': {
      if ((r.action === 'stream_reminder' && r.reminder === 'status') || (r.action === 'playlist_order' && !r.playlist_order)) return;
      const fields = { stream_reminder: ['id', 'reminder'], playlist_order: ['playlist', 'playlist_order'], comment_post: ['id', 'text'], comment_reply: ['id', 'cursor', 'text'] }[r.action];
      need(SESSION.test(r.session ?? '') && fields.every(field => filled(r[field])), 'invalid_request', 'The change needs a confirmation session and a valid target.');
      break;
    }
    default: return;
  }
  throw owner === 'guest' ? new ServiceError('account_required', 'Select an account profile first.', 401) :
    new ServiceError('unknown_profile', 'Select an available profile.', 404);
}

let pending = 0, queue = Promise.resolve();
export async function libraryExecute(request, operation = 'execute') {
  if (!request || typeof request !== 'object' || host.utf8Encode(JSON.stringify(request)).byteLength > 8192)
    throw new ServiceError('library_request_limit', 'The library operation exceeds its request limit.', 400);
  if (pending >= 16) throw new ServiceError('library_busy', 'The library is busy. Retry shortly.', 429);
  pending++;
  // One operation at a time, as the worker process does: the adapter keeps
  // cursors and retained feeds that must not interleave.
  const run = queue.then(async () => {
    if (operation === 'execute') accountGuard(request);
    let result;
    try { result = operation === 'playback' ? await catalog().playback(request) : await catalog().execute(request); }
    catch (error) {
      // Only the adapter's bounded public errors cross this boundary.
      if (!(error instanceof LibraryError)) {
        if (host.getenv('TUBE_JS_TRACE')) console.error(error?.stack ?? error);
        throw new ServiceError('upstream_failure', 'YouTube could not complete this operation.', 502);
      }
      const code = typeof error.code === 'string' && /^[a-z_]{1,64}$/.test(error.code) ? error.code : 'library_failure';
      const status = Number.isInteger(error.status) && error.status >= 400 && error.status <= 599 ? error.status : 502;
      throw new ServiceError(code, typeof error.message === 'string' && error.message.length <= 256 ? error.message : 'The library operation failed.', status);
    }
    if (!result || typeof result !== 'object' || Array.isArray(result))
      throw new ServiceError('library_transport', 'The library worker returned invalid operation data.');
    if (JSON.stringify(result).length > 1048576) throw new ServiceError('response_limit', 'The returned operation exceeds the response limit.', 502);
    return result;
  });
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
