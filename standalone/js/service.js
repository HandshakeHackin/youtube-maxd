// Bundle entry. Defines what the host calls: `__tubeRequest` (the embedded
// companion, see companion.js) and `tubeMain` (one-shot commands for tests,
// probes and diagnosis). SPDX-License-Identifier: GPL-3.0-or-later
import './platform.js';
import { Constants } from 'youtubei.js';
import './companion.js';
import { LibraryError, catalog, config, extractVideo, libraryExecute, mediaFetch, playbackClient, prepare, rawPlayerFacts, youtubeFetch } from './core.js';
import { ServiceError } from './errors.js';
import { resolveRequest } from './schema.js';
import { fileIndex, rangedManifest } from './segments.js';
import { CLIENTS, PLAYBACK_ORDER, captionMaps, createPlayback, dashManifest, formatRow, playerResponse, rangeCheck, redact, selectAudio, selectVideo, vttToSrt } from './resolve.js';
import { selfTest } from './selftest.js';

const host = globalThis.__host;
const timing = {};
async function timed(name, work) {
  const started = host.now();
  try { return await work(); } finally { timing[name] = Math.round(host.now() - started); }
}
const videoId = value => { if (!/^[A-Za-z0-9_-]{11}$/.test(value ?? '')) throw new LibraryError('invalid_video', 'Enter an eleven-character video ID.'); return value; };
const clientList = value => {
  const list = !value || value === 'all' ? CLIENTS : value.toUpperCase().split(',');
  for (const name of list) if (!CLIENTS.includes(name)) throw new LibraryError('invalid_client', `Unknown client: ${name}.`);
  return list;
};

async function library(requests) {
  const results = [];
  for (const text of requests) {
    const request = JSON.parse(text.replaceAll('$cursor', results.at(-1)?.cursor ?? ''));
    results.push(await timed(`library_${results.length}_ms`, () => libraryExecute({ profile: 'guest', ...request })));
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
      suggestions: (data.watch_next_feed ?? []).length } : {}) };
}

// The `Playback` record as the service would answer it, with every address
// described instead of printed (TUBE_JS_SHOW_URLS=1 prints the manifests).
async function resolve(id, client) {
  const request = resolveRequest({ id, max_height: Number(host.getenv('TUBE_JS_MAX_HEIGHT')) || 1080, codec: host.getenv('TUBE_JS_CODEC') || 'auto' });
  const source = await timed('extract_ms', () => extractVideo(id, { order: client ? clientList(client) : PLAYBACK_ORDER }));
  const session = { tracks: new Map(), manifests: new Map(), hls_manifests: new Map() };
  const playback = await createPlayback(source, request, 'http://127.0.0.1:0/media/session', session, 1800);
  const shown = !!host.getenv('TUBE_JS_SHOW_URLS');
  return { ...playback, client: source.client, served: {
    manifests: Object.fromEntries(Array.from(session.manifests, ([name, text]) => [name, typeof text !== 'string' ? 'built on first use' :
      shown ? text : `${text.length} bytes, ${redact(/<BaseURL>([^<]+)/.exec(text)?.[1]?.replaceAll('&amp;', '&') ?? '')}`])),
    playlists: Object.fromEntries(Array.from(session.hls_manifests, ([name, text]) => [name, shown ? text : `${text.length} bytes`])),
    tracks: Object.fromEntries(Array.from(session.tracks, ([name, track]) => [name, `${track.kind} ${shown ? track.url : redact(track.url)}`]).slice(0, 12)) } };
}

// Evidence run: every client's answer, and for the first `ranged` playable
// ones direct ranged reads, the generated manifest and captions.
async function probe(id, clients, ranged = 2) {
  const yt = await timed('playback_client_ms', () => playbackClient());
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
    const started = host.now();
    let videoUrl, audioUrl;
    try { videoUrl = await video.decipher(yt.session.player); audioUrl = audio ? await audio.decipher(yt.session.player) : ''; }
    catch (error) { entry.transform = { ok: false, error: String(error.message).slice(0, 160) }; continue; }
    const before = new URL(video.url ?? `https://x.invalid/?${video.signature_cipher ?? video.cipher}`), after = new URL(videoUrl);
    entry.transform = { ok: true, ms: Math.round(host.now() - started), needed_signature: !video.url, n_present: after.searchParams.has('n'),
      n_changed: !!video.url && before.searchParams.get('n') !== after.searchParams.get('n'), address: redact(videoUrl) };
    const length = video.content_length || 0;
    entry.ranges = { chosen: { video: formatRow(video), audio: audio ? formatRow(audio) : '' },
      video_first_256k: await rangeCheck(mediaFetch, videoUrl, 0, 262143),
      video_middle_64k: length > 1048576 ? await rangeCheck(mediaFetch, videoUrl, length >> 1, (length >> 1) + 65535) : 'skipped (short file)',
      audio_first_256k: audioUrl ? await rangeCheck(mediaFetch, audioUrl, 0, 262143) : 'none' };
    try {
      const began = host.now(), manifest = await answer.info.toDash();
      entry.to_dash = { bytes: manifest.length, representations: manifest.split('<Representation').length - 1, segment_base: manifest.includes('<SegmentBase'), ms: Math.round(host.now() - began) };
    } catch (error) { entry.to_dash = { error: String(error.message).slice(0, 160) }; }
    const tracks = Array.from(captionMaps(answer.info, false).subtitles);
    entry.caption_tracks = tracks.map(([language]) => language);
    if (tracks.length && !captionsDone) {
      captionsDone = true;
      try {
        const response = await youtubeFetch(tracks[0][1].url), text = await response.text(), srt = response.ok ? vttToSrt(text) : '';
        entry.caption_download = { status: response.status, bytes: text.length, webvtt: text.startsWith('WEBVTT'), srt_cues: srt ? srt.split('\n\n').length : 0 };
      } catch (error) { entry.caption_download = { error: String(error.message).slice(0, 160) }; }
    }
  }
  if (yt.session.player) report.player = { id: yt.session.player.player_id, signature_timestamp: yt.session.player.signature_timestamp };
  return report;
}

// How a player may ask for bytes deeper in a file. Each step: a fraction of
// the file (0..1), optional suffix `a` (send the client's User-Agent) or `q`
// (use the `range` query parameter instead of the header).
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

// A manifest for files on disk, for the seek laboratory (test/seek_lab.py):
// {duration, shape: "classic" | "ranged", segment_seconds, tracks: [{kind, url, head_file, index_file, mime_type,
// init_range, index_range, content_length, width, height, bitrate, audio_sample_rate}]}.
async function manifest(text) {
  const request = JSON.parse(text), [video, audio] = request.tracks;
  if (request.shape !== 'ranged') return { manifest: dashManifest(request.duration, video, audio, video.url, audio.url) };
  const tracks = [];
  for (const track of request.tracks) {
    // head_file: the file's first bytes; index_file: the bytes of index_range.
    const head = new Uint8Array(host.readFile(track.head_file)), index = new Uint8Array(host.readFile(track.index_file));
    tracks.push({ kind: track.kind, format: track, url: track.url, index: await fileIndex(async first => first === 0 ? head : index, track, request.duration) });
  }
  return { manifest: rangedManifest(request.duration, tracks, request.segment_seconds), segments: tracks.map(track => track.index.segments.length) };
}

const usage ='usage: tube_js serve --state-dir DIR [--host 127.0.0.1] [--port N] [--token T] [--sponsorblock] | search <query> [more] | ' +
  'info <id> [full] | comments <id> | channel <UC…> | resolve <id> [client] | probe <id> [client,…|all] [ranged] | ' +
  'ranges <id> [client] [itag] [steps] | library <json>… | version | selftest';

export async function handle(argv) {
  const [command, first, second, third] = argv;
  switch (command) {
    case 'version': return { engine: `quickjs-ng ${host.engine}`, library: 'youtubei.js 18.1.0', clients: CLIENTS };
    case 'selftest': return selfTest();
    case 'search': return library([JSON.stringify({ action: 'search', query: first }), ...(second === 'more' ? ['{"action":"continue","cursor":"$cursor"}'] : [])]);
    case 'comments': return library([JSON.stringify({ action: 'comments', id: videoId(first) })]);
    case 'channel': return library([JSON.stringify({ action: 'channel', channel: first, ...(second ? { tab: second } : {}) })]);
    case 'library': return library(argv.slice(1));
    case 'manifest': return manifest(first);
    case 'explore': {   // which facts a client's answers carry for one video (diagnosis)
      const yt = await playbackClient(), data = await yt.getInfo(videoId(first), { client: second ?? 'VISIONOS' });
      const markers = data.player_overlays?.decorated_player_bar?.player_bar?.markers_map ?? [];
      return { facts: rawPlayerFacts(first), duration: data.basic_info?.duration, width: data.streaming_data?.adaptive_formats?.[0]?.width,
        height: data.streaming_data?.adaptive_formats?.[0]?.height, overlays: data.player_overlays?.type ?? null,
        markers: Array.from(markers, entry => ({ key: entry.marker_key, chapters: (entry.value?.chapters ?? []).slice(0, 3).map(chapter => [String(chapter.title), chapter.time_range_start_millis]) })),
        panels: (data.page?.[1]?.engagement_panels ?? []).map(panel => panel.panel_identifier ?? panel.target_id ?? panel.type).slice(0, 8) };
    }
    case 'info': return info(videoId(first), second);
    case 'resolve': return resolve(videoId(first), second);
    case 'ranges': return ranges(videoId(first), clientList(second ?? 'VISIONOS')[0], third, argv[4]);
    case 'probe': return probe(videoId(first), second, third === undefined ? 2 : Number(third));
    default: throw new LibraryError('usage', usage);
  }
}

globalThis.tubeMain = async () => {
  const started = host.now();
  let output;
  try { output = await handle(config.argv ?? []); }
  catch (error) {
    if (host.getenv('TUBE_JS_TRACE')) console.error(error?.stack ?? error);
    const known = error instanceof LibraryError || error instanceof ServiceError;
    output = { error: { code: known ? error.code : 'upstream_failure',
      message: known || host.getenv('TUBE_JS_TRACE') ? String(error.message) : 'YouTube could not complete this operation.' } };
  }
  if (host.getenv('TUBE_JS_STATS')) {
    const stats = host.stats();
    output = { result: output, timing: { ...timing, total_ms: Math.round(host.now() - started), bundle_ms: Math.round(stats.bundle_ms) },
      requests: stats.requests, bytes_in: stats.bytes_in, peak_rss_kb: stats.peak_rss_kb, js_heap_kb: Math.round(stats.js_heap_kb) };
  }
  return JSON.stringify(output);
};
