// The embedded companion: the HTTP API of companion/prosperotube/app.py,
// answered inside the app on 127.0.0.1. SPDX-License-Identifier: GPL-3.0-or-later
//
// Same routes, fields, error codes and bounds as the Python service, so the
// native provider only needs another address and token. What differs: media
// bytes are never relayed (manifests and playlists name YouTube's servers,
// a muxed file answers with a redirect), there is one profile (Guest), and
// sign-in, phone remote, voice, import and proxy are not offered.
import { Catalog, item, videoRecord } from './catalog.js';
import { config, deadline, extractVideo, libraryExecute, mediaFetch, scoped, tokenUrlsafe } from './core.js';
import { ServiceError } from './errors.js';
import { DeArrow, Sponsor, thumbnails } from './extras.js';
import { bytesOf, headerLines } from './fetch_shim.js';
import { createPlayback, vttToSrt } from './resolve.js';
import { Invalid, jobRequest, libraryRequest, queryOptions, resolveRequest, validLocale } from './schema.js';

const host = globalThis.__host;
const VERSION = typeof __TUBE_VERSION__ === 'string' ? __TUBE_VERSION__ : '0.0.0';
const VIDEO = /^[A-Za-z0-9_-]{11}$/;
// Exactly what this service implements; no relay, sign-in, voice, import, dislikes or proxy.
const FEATURES = ['search', 'channels', 'playlists', 'dash', 'captions', 'dearrow', 'locale', 'thumbnail_source', 'storyboards',
  'translated_captions', 'video_details', 'library', 'content_filters', 'channel_pages', 'kids', 'stream_reminder', 'playlist_order',
  'subscribe_notifications', 'comment_post', 'search_first', 'channel_new_content', 'embedded'];
const SESSION_TTL = 1800000, JOB_TTL = 120000;

const videoId = value => { if (!VIDEO.test(value ?? '')) throw new ServiceError('invalid_video', 'Enter an eleven-character YouTube video ID.', 400); return value; };
const guestOnly = profile => { if (profile !== 'guest') throw new ServiceError('unknown_profile', 'Select an available profile.', 404); };
const localeOptions = (hl = '', gl = '') => {
  if (!validLocale(hl, gl)) throw new ServiceError('invalid_locale', 'Select a supported content language and country.', 400);
  return { ...(hl ? { hl } : {}), ...(gl ? { gl } : {}) };
};
const imageFetch = scoped(['ytimg.com', 'ggpht.com', 'googleusercontent.com'], { timeout: 10000, limit: 2 * 1024 * 1024 });
const captionFetch = scoped(['youtube.com'], { timeout: 15000, limit: 2 * 1024 * 1024 });

export class Companion {
  constructor(settings = config) {
    this.origin = settings.origin; this.token = settings.token;
    this.pairingCode = settings.pairing_code ?? ''; this.pairingDeadline = host.now() + 600000; this.pairAttempts = [];
    this.ranged = !!settings.ranged_manifest;
    this.sponsor = new Sponsor(settings.sponsorblock, settings.sponsorblock_server);
    this.dearrow = new DeArrow();
    this.catalog = new Catalog(request => libraryExecute({ ...request, profile: 'guest' }, 'playback'), (id, hl) => extractVideo(id, { hl }));
    this.sessions = new Map(); this.jobs = new Map();
  }

  // ---- playback sessions (sessions.Sessions) ----
  session(token) {
    const session = this.sessions.get(token);
    if (!session || session.deadline <= host.now()) {
      this.sessions.delete(token);
      throw new ServiceError('expired_session', 'Playback information expired. Select the video again to refresh it.', 410);
    }
    return session;
  }
  touch(session) { if (this.sessions.get(session.token) === session) session.deadline = host.now() + SESSION_TTL; }
  async resolve(request) {
    videoId(request.id);
    guestOnly(request.profile);
    const source = await extractVideo(request.id, { hl: request.hl, fresh: request.fresh });
    const now = host.now();
    for (const [key, session] of this.sessions) if (session.deadline <= now) this.sessions.delete(key);
    if (this.sessions.size >= 32) throw new ServiceError('busy', 'The companion has too many active playback sessions. Retry shortly.', 503);
    const session = { token: tokenUrlsafe(24), video_id: request.id, deadline: now + SESSION_TTL, profile: request.profile, request,
      tracks: new Map(), manifests: new Map(), hls_manifests: new Map() };
    const playback = await this.fill(session, source);
    this.sessions.set(session.token, session);
    return playback;
  }
  // Builds what /media serves for a session. Signed addresses last about six
  // hours; `renew` is when the session's own are within ten minutes of that.
  async fill(session, source) {
    const playback = await createPlayback(source, session.request, `${this.origin}/media/${session.token}`, session, SESSION_TTL / 1000, this.ranged ? mediaFetch : null);
    session.renew = Date.now() + (source.lifetime - 600) * 1000;
    return playback;
  }
  // The same session with new signed addresses: same names, so a player that
  // reopens its manifest address continues with links that work again.
  async refresh(session) {
    session.refreshing ??= (async () => {
      const fresh = { ...session, tracks: new Map(), manifests: new Map(), hls_manifests: new Map() };
      await this.fill(fresh, await extractVideo(session.video_id, { hl: session.request.hl, fresh: true }));
      Object.assign(session, { tracks: fresh.tracks, manifests: fresh.manifests, hls_manifests: fresh.hls_manifests, renew: fresh.renew });
    })().finally(() => { session.refreshing = null; });
    return session.refreshing;
  }
  async media(token, key, method, headers, query) {
    const session = this.session(token);
    // A manifest or playlist asked for with `?refresh=1`, or after its links
    // aged out, is rebuilt from a new player answer first.
    if ((session.manifests.has(key) || session.hls_manifests.has(key)) && method === 'GET' && session.request &&
        (query.get('refresh') === '1' || Date.now() > session.renew)) await this.refresh(session);
    const text = (body, type) => ({ status: 200, type, headers: [['Cache-Control', 'no-store']], body: method === 'GET' ? body : '' });
    if (session.manifests.has(key) || session.hls_manifests.has(key)) {
      this.touch(session);
      if (session.hls_manifests.has(key)) return text(session.hls_manifests.get(key), 'application/vnd.apple.mpegurl');
      const manifest = session.manifests.get(key);   // text, or a function that builds it on first use
      return text(typeof manifest === 'function' ? await manifest() : manifest, 'application/dash+xml');
    }
    const remote = session.tracks.get(key);
    if (!remote) throw new ServiceError('missing_media', 'The playback resource does not exist.', 404);
    const range = headers.get('range');
    if (range && (range.length > 100 || !/^bytes=(?:[0-9]+-[0-9]*|-[0-9]+)$/.test(range))) throw new ServiceError('invalid_range', 'Only a single byte range is supported.', 416);
    if (remote.kind === 'redirect') {   // a muxed file: the player reads it from YouTube itself
      this.touch(session);
      return { status: 302, type: 'text/plain', headers: [['Location', remote.url], ['Cache-Control', 'no-store']], body: '' };
    }
    let reply;
    try { reply = await (remote.kind === 'caption' ? captionFetch : imageFetch)(remote.url, { method: remote.kind === 'caption' ? 'GET' : method }); }
    catch { throw new ServiceError('media_network', 'The upstream media connection failed. Retry playback.', 502); }
    if ([401, 403, 410].includes(reply.status)) {   // the signed address ran out: the client resolves again
      this.sessions.delete(token);
      throw new ServiceError('expired_session', 'Playback information expired. Select the video again to refresh it.', 410);
    }
    if (reply.status !== 200) throw new ServiceError('upstream_media', 'The media server could not provide this resource.', 502);
    this.touch(session);
    if (remote.kind === 'caption') return text(vttToSrt(await reply.text()), 'application/x-subrip');
    return { status: 200, type: reply.headers.get('content-type') ?? 'image/jpeg', headers: [['Cache-Control', 'no-store']], body: new Uint8Array(await reply.arrayBuffer()) };
  }

  // ---- operations shared by jobs and direct routes ----
  async library(operation) {
    const { thumbnails: source, ...request } = operation;
    return thumbnails(await libraryExecute(request), source);
  }
  async page(kind, value, options, profile) {
    guestOnly(profile);
    const result = await this.catalog.pageFor({ [kind]: value, page: options.page,
      options: { ...localeOptions(options.hl, options.gl), ...(options.hide !== null && options.hide !== undefined ? { hide: options.hide } : {}) } });
    return thumbnails(result, options.thumbnails);
  }
  async details(id, profile, hl) {
    videoId(id);
    guestOnly(profile);
    return videoRecord(await extractVideo(id, localeOptions(hl)));
  }

  // ---- jobs (app.create_job, poll_job, cancel_job) ----
  retire(job) {
    job.cancelled = job.state === 'pending';
    // An unread chat that already opened is closed, as a cancelled read would be.
    if (job.chat && job.state === 'done' && !job.delivered && !job.cleaned) {
      const cursor = job.value?.result?.cursor;
      if (job.value?.result?.kind === 'live_chat' && typeof cursor === 'string' && /^[A-Za-z0-9_-]{1,64}$/.test(cursor)) {
        job.cleaned = true;
        libraryExecute({ action: 'chat_close', profile: job.profile, cursor }).catch(() => {});
      }
    }
  }
  createJob(payload) {
    const now = host.now(), active = () => Array.from(this.jobs.values()).filter(job => job.state === 'pending').length;
    for (const [key, job] of this.jobs) if (job.deadline <= now) { this.retire(job); this.jobs.delete(key); }
    if (active() >= 16) throw new ServiceError('job_limit', 'The companion is busy. Wait for the current request or cancel it.', 503);
    if (payload.action === 'resolve' && !payload.playback) throw new ServiceError('invalid_job', 'A playback request is required.', 400);
    if (payload.action === 'library' && !payload.library) throw new ServiceError('invalid_job', 'An enabled library and operation are required.', 400);
    if (payload.action === 'auth') throw new ServiceError('invalid_job', 'An enabled sign-in service and operation are required.', 400);
    // Finished jobs stay for repeat polling; that cache is bounded separately.
    if (this.jobs.size >= 80) {
      const completed = Array.from(this.jobs).filter(([, job]) => job.state !== 'pending')
        .sort((a, b) => (a[1].delivered ? 0 : 1) - (b[1].delivered ? 0 : 1) || a[1].deadline - b[1].deadline);
      if (completed.length) { this.retire(completed[0][1]); this.jobs.delete(completed[0][0]); }
    }
    const work = async () => {
      let result;
      if (payload.action === 'search') result = await this.page('query', payload.query, payload, payload.profile);
      else if (payload.action === 'catalog') result = await this.page('parent', payload.parent, payload, payload.profile);
      else if (payload.action === 'resolve') result = await this.resolve(payload.playback);
      else if (payload.action === 'library') result = await this.library(payload.library);
      else {
        const record = await this.details(payload.id, payload.profile, payload.hl);
        result = thumbnails({ item: item(record), chapters: record.chapters }, payload.thumbnails);
      }
      if (payload.action !== 'resolve') result = await this.dearrow.decorate(result, payload.dearrow_titles, payload.dearrow_thumbnails, this.origin);
      return { state: 'done', result };
    };
    const id = tokenUrlsafe(18);
    const job = { deadline: now + JOB_TTL, state: 'pending', value: null, delivered: false, cancelled: false,
      profile: payload.action === 'resolve' ? payload.playback.profile : payload.action === 'library' ? payload.library.profile : payload.profile,
      chat: payload.action === 'library' && ['live_chat', 'chat_poll'].includes(payload.library.action) };
    this.jobs.set(id, job);
    work().catch(error => {
      if (!(error instanceof ServiceError) && host.getenv('TUBE_JS_TRACE')) console.error(error?.stack ?? error);
      return error instanceof ServiceError ? { state: 'failed', error: { code: error.code, message: error.message } } :
        { state: 'failed', error: { code: 'service_failure', message: 'The video service could not complete this request. Retry.' } };
    }).then(value => { job.state = 'done'; job.value = value; if (job.cancelled) this.retire(job); });
    return { api_version: 1, job: id, state: 'pending' };
  }
  pollJob(id) {
    const job = this.jobs.get(id);
    if (!job || job.deadline <= host.now()) {
      if (job) this.retire(job);
      throw new ServiceError('expired_job', 'This request expired. Retry from the current screen.', 410);
    }
    if (job.state === 'pending') return { api_version: 1, state: 'pending' };
    job.delivered = true;
    return { api_version: 1, ...job.value };
  }
  cancelJob(id) {
    const job = this.jobs.get(id);
    if (job) { this.jobs.delete(id); this.retire(job); }
    return { state: 'cancelled' };
  }

  pair(body) {
    if (!body || typeof body !== 'object' || typeof body.code !== 'string' || !/^[0-9]{6}$/.test(body.code)) throw new Invalid('String should match pattern', 'code');
    const now = host.now();
    this.pairAttempts = this.pairAttempts.filter(at => at > now - 60000);
    if (this.pairAttempts.length >= 5) throw new ServiceError('pair_rate_limit', 'Too many pairing attempts. Wait one minute and retry.', 429);
    this.pairAttempts.push(now);
    // Pairing is not needed here: the app reads the token from the state directory.
    if (!this.pairingCode || now > this.pairingDeadline) throw new ServiceError('pair_expired', 'The pairing code expired. Restart the companion to get a new code.', 410);
    if (!sameText(body.code, this.pairingCode)) throw new ServiceError('pair_denied', 'The pairing code did not match.', 403);
    return { api_version: 1, token: this.token };
  }

  // ---- routing ----
  async route(method, target, headers, body) {
    const url = new URL(target, 'http://127.0.0.1'), path = url.pathname, query = url.searchParams;
    if (path.startsWith('/v1/') && path !== '/v1/health' && path !== '/v1/pair' && !sameText(headers.get('authorization') ?? '', `Bearer ${this.token}`))
      return json({ error: { code: 'unauthorized', message: 'Pair the PS5 with this companion first.' } }, 401);
    const posted = () => {
      try { return JSON.parse(host.utf8Decode(body)); } catch { throw new Invalid('JSON decode error'); }
    };
    const routes = [
      ['GET', /^\/v1\/health$/, () => json({ api_version: 1, version: VERSION, service: 'ProsperoTube', mode: 'youtube', features: FEATURES,
        sponsorblock: this.sponsor.enabled, sponsorblock_alt: this.sponsor.server !== null, proxy: false,
        // Media addresses in manifests and playlists are YouTube's own, not this service's.
        embedded: true })],
      ['POST', /^\/v1\/pair$/, () => json(this.pair(posted()))],
      ['POST', /^\/v1\/library$/, async () => json(await this.library(libraryRequest(posted())))],
      ['POST', /^\/v1\/jobs$/, () => json(this.createJob(jobRequest(posted())))],
      ['GET', /^\/v1\/jobs\/([^/]+)$/, id => json(this.pollJob(id))],
      ['DELETE', /^\/v1\/jobs\/([^/]+)$/, id => json(this.cancelJob(id))],
      ['POST', /^\/v1\/resolve$/, async () => json(await this.resolve(resolveRequest(posted())))],
      ['GET', /^\/v1\/search$/, async () => {
        const options = queryOptions(query, { q: [1, 200] });
        return json(await this.dearrow.decorate(await this.page('query', options.q, options, options.profile), options.dearrow_titles, options.dearrow_thumbnails, this.origin));
      }],
      ['GET', /^\/v1\/catalog$/, async () => {
        const options = queryOptions(query, { parent: [1, 160] });
        return json(await this.dearrow.decorate(await this.page('parent', options.parent, options, options.profile), options.dearrow_titles, options.dearrow_thumbnails, this.origin));
      }],
      ['GET', /^\/v1\/video\/([^/]+)$/, async id => {
        const options = queryOptions(query), record = await this.details(id, options.profile, options.hl);
        return json({ api_version: 1, item: item(record), chapters: record.chapters, caption_languages: record.caption_languages });
      }],
      ['GET', /^\/v1\/dislikes\/([^/]+)$/, id => { videoId(id); return json({ api_version: 1, enabled: false }); }],
      ['GET', /^\/v1\/sponsor\/([^/]+)$/, async id => {
        videoId(id);
        const categories = query.get('categories') ?? 'sponsor,selfpromo', alt = ['true', '1', 'on', 'yes'].includes((query.get('alt') ?? '').toLowerCase());
        if (categories.length > 160) throw new Invalid('String should have at most 160 characters', 'categories');
        const chosen = Sponsor.chosen(categories);
        if (!chosen) throw new ServiceError('invalid_categories', 'An unsupported SponsorBlock category was requested.', 400);
        return json(await this.sponsor.segments(id, chosen, alt));
      }],
      ['GET', /^\/art\/dearrow\/([^/]+)$/, async key => {
        const image = /^[A-Za-z0-9_-]{32}$/.test(key) ? await this.dearrow.thumbnail(key) : null;
        return image ? { status: 200, type: image.type, headers: [['Cache-Control', 'private, max-age=300']], body: image.bytes } : { status: 404, type: 'text/plain', headers: [], body: '' };
      }],
      ['GET HEAD', /^\/media\/([^/]+)\/([^/]+)$/, (token, key) => this.media(token, key, method, headers, query)],
      // Not offered by the embedded service: the existing "not available" answers.
      ['POST', /^\/v1\/auth\/(start|status|cancel)$/, () => { throw new ServiceError('auth_unavailable', 'Enable the library worker on the companion first.', 503); }],
      ['POST', /^\/v1\/voice$/, () => { throw new ServiceError('voice_unavailable', 'Enable local speech recognition on the companion first.', 503); }],
      ['POST', /^\/v1\/remote$/, () => { throw new ServiceError('remote_session', 'Enable phone connection first.', 404); }]
    ];
    let known = false;
    for (const [methods, pattern, handler] of routes) {
      const match = pattern.exec(path);
      if (!match) continue;
      known = true;
      if (methods.split(' ').includes(method)) return handler(...match.slice(1).map(part => decodeURIComponent(part)));
    }
    return json({ detail: known ? 'Method Not Allowed' : 'Not Found' }, known ? 405 : 404);
  }

  // Never throws: every failure becomes the answer the Python service gives.
  async handle(method, target, headers, body) {
    try { return await deadline(80000, Promise.resolve().then(() => this.route(method, target, headers, body)),
      () => new ServiceError('timeout', 'YouTube took too long to respond. Retry from the video screen.', 504)); }
    catch (error) {
      if (error instanceof ServiceError) return json({ api_version: 1, error: { code: error.code, message: error.message } }, error.status);
      if (error instanceof Invalid) return json({ detail: [{ type: 'value_error', loc: error.field ? ['body', error.field] : ['body'], msg: error.message }] }, 422);
      if (error instanceof URIError) return json({ detail: 'Not Found' }, 404);
      if (host.getenv('TUBE_JS_TRACE')) console.error(error?.stack ?? error);
      return { status: 500, type: 'text/plain; charset=utf-8', headers: [], body: 'Internal Server Error' };
    }
  }
}

function json(value, status = 200) { return { status, type: 'application/json', headers: [], body: JSON.stringify(value) }; }
// Comparison that does not stop at the first difference.
function sameText(a, b) {
  let difference = a.length ^ b.length;
  for (let i = 0; i < b.length; i++) difference |= (a.charCodeAt(i % Math.max(1, a.length)) || 0) ^ b.charCodeAt(i);
  return difference === 0;
}

// The host calls this once per complete request (see tube_engine.c).
let service = null;
globalThis.__tubeRequest = (id, method, target, headerText, body) => {
  service ??= new Companion();
  service.handle(method, target, headerLines(headerText), body).then(answer => {
    host.respond(id, answer.status, [['Content-Type', answer.type], ...answer.headers], bytesOf(answer.body) ?? new Uint8Array(0));
  });
};
