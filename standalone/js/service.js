// Entry of the embedded service: the companion's library modules and the
// playback resolver behind one command function. SPDX-License-Identifier: GPL-3.0-or-later
//
// `tubeMain()` is what the host calls. On the console the same `handle()`
// would be reached through an in-process request queue instead of argv.
import { cacheDirectory } from './platform.js';
import { Constants, Innertube, Log, Player, UniversalCache } from 'youtubei.js';
import { Library, LibraryError } from '../../companion/innertube/library.mjs';
import { createProfileClient } from '../../companion/innertube/device_auth.mjs';
import { CLIENTS, PLAYBACK_ORDER, SCRIPTLESS, captionTracks, formatRow, playback, playerResponse, printablePlayback, rangeCheck,
  redact, selectAudio, selectVideo, vttToSrt } from './resolve.js';
import { selfTest } from './selftest.js';

const host = globalThis.__host;
Log.setLevel(host.getenv('TUBE_JS_VERBOSE') ? Log.Level.INFO : Log.Level.NONE);

// Same rule as the companion worker: HTTPS to known origins only, and API
// redirects are errors so no header ever follows one to another origin.
const origins = ['youtube.com', 'google.com', 'googleapis.com', 'gstatic.com'];
const within = (hostname, list) => list.some(name => hostname === name || hostname.endsWith(`.${name}`));
async function scopedFetch(input, init = {}) {
  const request = new Request(input, init);
  const url = new URL(request.url);
  if (url.protocol !== 'https:' || url.username || url.password || url.port || !within(url.hostname, origins))
    throw new LibraryError('unsupported_origin', 'The service returned an unsupported origin.', 502);
  const response = await fetch(request, { redirect: 'manual', signal: AbortSignal.timeout(15000), max_bytes: 8 * 1024 * 1024 });
  if (response.status >= 300 && response.status < 400)
    throw new LibraryError('upstream_redirect', 'The service requested an unsupported redirect.', 502);
  return response;
}
// Media reads go to the video servers and may be redirected between them.
async function mediaFetch(input, init = {}) {
  const url = new URL(typeof input === 'string' ? input : input.url);
  if (url.protocol !== 'https:' || !within(url.hostname, ['googlevideo.com', 'youtube.com']))
    throw new LibraryError('unsupported_origin', 'The resolver returned an unsupported media origin.', 502);
  return fetch(input, { ...init, signal: AbortSignal.timeout(20000) });
}

const timing = {};
async function timed(name, work) {
  const started = host.now();
  try { return await work(); } finally { timing[name] = Math.round(host.now() - started); }
}

let library = null;
function catalog() {
  library ??= new Library((profile, locale) => createProfileClient(profile, Innertube, scopedFetch, () => {}, locale), []);
  return library;
}
// The playback client takes its session (visitor identity) from YouTube: with
// a locally generated one the app clients answered "Sign in to confirm you're
// not a bot" (checked live on 2026-10-11). It starts without the player script. Clients whose
// addresses arrive ready (SCRIPTLESS) never need it; for the others it is
// fetched and analysed once per player version and cached on disk, because
// that analysis is by far the most expensive step in this engine.
let playbackSession = null;
const cache = () => new UniversalCache(true, cacheDirectory());
function playbackClient() {
  playbackSession ??= timed('playback_client_ms', () => Innertube.create({ cache: cache(), retrieve_player: false,
    retrieve_innertube_config: false, generate_session_locally: !!host.getenv('TUBE_JS_LOCAL_SESSION'),
    enable_session_cache: false, fetch: scopedFetch }));
  return playbackSession;
}
async function prepare(yt, client) {
  if (SCRIPTLESS.includes(client) || yt.session.player) return;
  yt.session.player = await timed('player_script_ms', () => Player.create(cache(), scopedFetch));
}

const videoId = value => { if (!/^[A-Za-z0-9_-]{11}$/.test(value ?? '')) throw new LibraryError('invalid_video', 'Enter an eleven-character video ID.'); return value; };
const clientList = value => {
  const list = !value || value === 'all' ? CLIENTS : value.toUpperCase().split(',');
  for (const name of list) if (!CLIENTS.includes(name)) throw new LibraryError('invalid_client', `Unknown client: ${name}.`);
  return list;
};

async function library_(requests) {
  const results = [];
  for (const text of requests) {
    const request = JSON.parse(text.replaceAll('$cursor', results.at(-1)?.cursor ?? ''));
    results.push(await timed(`library_${results.length}_ms`, () => catalog().execute({ profile: 'guest', ...request })));
  }
  return results.length === 1 ? results[0] : results;
}

async function info(id, mode) {
  const client = await catalog().client('guest');
  const data = await timed('info_ms', () => mode === 'full' ? client.getInfo(id) : client.getBasicInfo(id));
  const basic = data.basic_info;
  return { api_version: 1, id: basic.id, title: basic.title, author: basic.author, channel_id: basic.channel_id,
    duration_sec: basic.duration, view_count: basic.view_count, is_live: !!basic.is_live, is_private: !!basic.is_private,
    category: basic.category ?? '', keywords: (basic.keywords ?? []).slice(0, 8), description: String(basic.short_description ?? '').slice(0, 240),
    thumbnails: (basic.thumbnail ?? []).length, playability: data.playability_status?.status ?? '',
    ...(mode === 'full' ? { like_count: basic.like_count ?? null, published: String(data.primary_info?.published ?? ''),
      suggestions: (data.watch_next_feed ?? []).length, has_comments_section: !!data.comments_entry_point_header || undefined } : {}) };
}

async function resolve(id, client) {
  const yt = await playbackClient();
  const record = await timed('resolve_ms', () => playback(yt, { id, max_height: Number(host.getenv('TUBE_JS_MAX_HEIGHT')) || 1080,
    codec: host.getenv('TUBE_JS_CODEC') || 'auto' }, client ? clientList(client) : PLAYBACK_ORDER, name => prepare(yt, name)));
  return printablePlayback(record);
}

// Evidence run: every client's answer, and for the first `ranged` playable
// ones the transforms, direct ranged reads, the generated manifests, captions.
async function probe(id, clients, ranged = 2) {
  const yt = await playbackClient();
  const report = { id, clients: [] };
  let captionsDone = false;
  for (const name of clientList(clients)) {
    let answer;
    try { await prepare(yt, name); answer = await playerResponse(yt, id, name); }
    catch (error) { report.clients.push({ client: name, status: 'REQUEST_FAILED', reason: String(error.message).slice(0, 160) }); continue; }
    const entry = { ...answer.summary, title: String(answer.info.basic_info?.title ?? '').slice(0, 60) };
    report.clients.push(entry);
    if (!answer.list.length) continue;
    entry.video = answer.list.filter(format => format.has_video).map(formatRow);
    entry.audio = answer.list.filter(format => !format.has_video).map(formatRow);
    const video = selectVideo(answer.list, { max_height: 2160 }).find(format => !format.has_audio) ?? selectVideo(answer.list, { max_height: 2160 })[0];
    const audio = selectAudio(answer.list)[0];
    if (entry.status !== 'OK' || !video || ranged <= 0) continue;
    ranged--;
    const chosen = { video: formatRow(video), audio: audio ? formatRow(audio) : '' };
    const started = host.now();
    let videoUrl, audioUrl;
    try { videoUrl = await video.decipher(yt.session.player); audioUrl = audio ? await audio.decipher(yt.session.player) : ''; }
    catch (error) { entry.transform = { ok: false, error: String(error.message).slice(0, 160) }; continue; }
    const before = new URL(video.url ?? `https://x.invalid/?${video.signature_cipher ?? video.cipher}`), after = new URL(videoUrl);
    entry.transform = { ok: true, ms: Math.round(host.now() - started), needed_signature: !video.url,
      n_present: after.searchParams.has('n'), n_changed: !!video.url && before.searchParams.get('n') !== after.searchParams.get('n'),
      address: redact(videoUrl) };
    const length = video.content_length || 0;
    entry.ranges = { chosen,
      video_first_256k: await rangeCheck(mediaFetch, videoUrl, 0, 262143),
      video_middle_64k: length > 1048576 ? await rangeCheck(mediaFetch, videoUrl, length >> 1, (length >> 1) + 65535) : 'skipped (short file)',
      audio_first_256k: audioUrl ? await rangeCheck(mediaFetch, audioUrl, 0, 262143) : 'none' };
    // Does the untransformed address work too? One small read answers it.
    if (entry.transform.n_changed) entry.ranges.video_untransformed_64k = await rangeCheck(mediaFetch, video.url, 0, 65535);
    try {
      const began = host.now();
      const manifest = await answer.info.toDash();
      entry.to_dash = { bytes: manifest.length, representations: manifest.split('<Representation').length - 1,
        segment_base: manifest.includes('<SegmentBase'), ms: Math.round(host.now() - began) };
    } catch (error) { entry.to_dash = { error: String(error.message).slice(0, 160) }; }
    const tracks = captionTracks(answer.info);
    entry.caption_tracks = tracks.map(track => `${track.language}${track.automatic ? ' (automatic)' : ''}`);
    if (tracks.length && !captionsDone) {
      captionsDone = true;
      try {
        const response = await scopedFetch(tracks[0].url);
        const text = await response.text();
        const srt = response.ok ? vttToSrt(text) : '';
        entry.caption_download = { status: response.status, bytes: text.length, webvtt: text.startsWith('WEBVTT'),
          srt_cues: srt ? srt.split('\n\n').length : 0, first_cue: srt.split('\n\n')[0]?.split('\n').slice(1).join(' | ').slice(0, 80) ?? '' };
      } catch (error) { entry.caption_download = { error: String(error.message).slice(0, 160) }; }
    }
  }
  if (yt.session.player) report.player = { id: yt.session.player.player_id, signature_timestamp: yt.session.player.signature_timestamp };
  return report;
}

// How a player may ask for bytes deeper in a file: Range header with and
// without the client's User-Agent, and the `range` query parameter.
async function ranges(id, client, itag, steps) {
  const yt = await playbackClient();
  await prepare(yt, client);
  const answer = await playerResponse(yt, id, client);
  const format = answer.list.find(row => String(row.itag) === String(itag)) ?? selectVideo(answer.list, { max_height: 1080 }).find(row => !row.has_audio);
  if (!format) return answer.summary;
  const started = host.now();
  const address = await format.decipher(yt.session.player), length = format.content_length || 0;
  const agent = { 'User-Agent': Constants.CLIENTS[client]?.USER_AGENT ?? '' };
  const n = text => { try { return new URL(text).searchParams.get('n'); } catch { return null; } };
  const report = { ...answer.summary, format: formatRow(format), content_length: length,
    transform: { ms: Math.round(host.now() - started), needed_signature: !format.url, n_present: n(address) !== null,
      n_changed: !!format.url && n(format.url) !== n(address), n_failed: String(n(address)).startsWith('enhanced_except_') }, reads: [] };
  // Each step: a fraction of the file (0..1), optional suffix `a` (send the
  // client's User-Agent) or `q` (use the `range` query parameter instead).
  for (const step of (steps ?? '0.5,0.004,0.5a,0.5q,0.999').split(',')) {
    const first = Math.min(Math.max(0, length - 65536), Math.floor(length * parseFloat(step))), last = Math.min(length - 1, first + 65535);
    let read;
    if (step.endsWith('q')) {
      const response = await mediaFetch(`${address}&range=${first}-${last}`, { max_bytes: 131072 });
      read = { status: response.status, bytes: (await response.arrayBuffer()).byteLength };
    } else read = await rangeCheck(mediaFetch, address, first, last, step.endsWith('a') ? agent : {});
    report.reads.push({ step, first, status: read.status, bytes: read.bytes, content_range: read.content_range, ms: read.ms });
  }
  return report;
}

const usage ='usage: tube_js search <query> [more] | info <id> [full] | comments <id> | channel <UC…> | ' +
  'resolve <id> [client] | probe <id> [client,…|all] [ranged] | ranges <id> [client] [itag] | library <json>… | version | selftest';

export async function handle(argv) {
  const [command, first, second, third] = argv;
  switch (command) {
    case 'version': return { engine: `quickjs-ng ${host.engine}`, library: 'youtubei.js 18.1.0', clients: CLIENTS };
    case 'selftest': return selfTest({ catalog, handle });
    case 'search': return library_([JSON.stringify({ action: 'search', query: first }),
      ...(second === 'more' ? ['{"action":"continue","cursor":"$cursor"}'] : [])]);
    case 'comments': return library_([JSON.stringify({ action: 'comments', id: videoId(first) })]);
    case 'channel': return library_([JSON.stringify({ action: 'channel', channel: first, ...(second ? { tab: second } : {}) })]);
    case 'library': return library_(argv.slice(1));
    case 'info': return info(videoId(first), second);
    case 'resolve': return resolve(videoId(first), second);
    case 'ranges': return ranges(videoId(first), clientList(second ?? 'ANDROID_VR')[0], third, argv[4]);
    case 'probe': return probe(videoId(first), second, third === undefined ? 2 : Number(third));
    default: throw new LibraryError('usage', usage);
  }
}

globalThis.tubeMain = async () => {
  const started = host.now();
  let output;
  try { output = await handle(host.argv); }
  catch (error) {
    if (host.getenv('TUBE_JS_TRACE')) console.error(error?.stack ?? error);
    const known = error instanceof LibraryError;
    output = { error: { code: known ? error.code : error?.code ?? 'upstream_failure',
      message: known || error?.code || host.getenv('TUBE_JS_TRACE') ? String(error.message) : 'YouTube could not complete this operation.' } };
  }
  if (host.getenv('TUBE_JS_STATS')) {
    const stats = host.stats();
    output = { result: output, timing: { ...timing, total_ms: Math.round(host.now() - started), bundle_ms: Math.round(stats.bundle_ms),
      network_ms: Math.round(stats.network_ms) }, requests: stats.requests, bytes_in: stats.bytes_in,
      peak_rss_kb: stats.peak_rss_kb, js_heap_kb: Math.round(stats.js_heap_kb) };
  }
  return JSON.stringify(output);
};
