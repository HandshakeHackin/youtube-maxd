// Playback resolution without yt-dlp or a relay: one player answer from a
// client that returns direct links, then the companion's format policy,
// manifest, captions, chapters and storyboards.
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Port of companion/prosperotube/sessions.py (Sessions.create, dash_manifest)
// and captions.py. The `Playback` record keeps its fields; what changes is
// that manifests name YouTube's servers directly instead of relay addresses.
import { ServiceError, classify, number, resolverError, safeRemote, text } from './errors.js';
import { fileIndex, rangeReader, rangedManifest } from './segments.js';

const host = globalThis.__host;

// Clients youtubei.js can impersonate for the player request; see README for
// what each returned live on 2026-10-11. Only VISIONOS gave addresses that can
// be read anywhere in a long file without a proof token, so it is the only
// default: IOS and ANDROID_VR addresses answer 403 a few tens of megabytes in.
export const CLIENTS = ['VISIONOS', 'ANDROID_VR', 'IOS', 'ANDROID', 'TV', 'TV_SIMPLY', 'TV_EMBEDDED', 'MWEB', 'WEB_EMBEDDED', 'WEB'];
export const PLAYBACK_ORDER = ['VISIONOS'];
// Clients whose media addresses arrive ready to use: no signature, no `n`
// transform, so the player script is not needed for them.
export const SCRIPTLESS = ['VISIONOS', 'IOS', 'ANDROID', 'ANDROID_VR'];

const codecs = mime => /codecs="([^"]+)/.exec(mime ?? '')?.[1] ?? '';
// sessions.codec_name
export function codecName(raw) {
  raw = String(raw ?? '').split(',')[0].trim();
  if (raw.startsWith('avc1') || raw.startsWith('h264')) return 'h264';
  if (raw.startsWith('vp9') || raw.startsWith('vp09')) return 'vp9';
  if (raw === 'av1' || raw.startsWith('av01.')) return 'av1';
  if (raw === 'hevc' || raw === 'h265' || raw.startsWith('hvc1.') || raw.startsWith('hev1.')) return 'hevc';
  if (raw.startsWith('mp4a') || raw.startsWith('aac')) return 'aac';
  if (raw.startsWith('opus')) return 'opus';
  return '';
}
const formatCodec = format => codecName(codecs(format.mime_type));
const container = mime => /^[a-z]+\/([a-z0-9]+)/.exec(mime ?? '')?.[1] ?? '';
const videoRange = format => {
  if (format.video_range === 'HDR10' || format.video_range === 'HLG') return format.video_range;
  const transfer = format.color_info?.transfer_characteristics;
  if (transfer === 'ARIB_STD_B67') return 'HLG';
  if (transfer === 'SMPTEST2084' || transfer === 'SMPTE2084') return 'HDR10';
  if (/HLG/i.test(format.quality_label ?? '')) return 'HLG';
  if (/HDR/i.test(format.quality_label ?? '')) return 'HDR10';
  return 'SDR';
};
const isHdr = format => videoRange(format) !== 'SDR';
const linkKind = format => format.url ? 'direct' : format.signature_cipher || format.cipher ? 'ciphered' : 'none';
const rate = format => number(format.average_bitrate || format.bitrate);

export function formats(info) {
  const data = info.streaming_data;
  return [...(data?.formats ?? []), ...(data?.adaptive_formats ?? [])];
}

// One compact row per format for reports; never contains an address.
export function formatRow(format) {
  const codec = formatCodec(format) || codecs(format.mime_type).split('.')[0], kbps = `${Math.round(rate(format) / 1000)}k`, link = linkKind(format);
  if (format.has_video)
    return `${format.itag} ${codec}${format.has_audio ? '+audio' : ''} ${format.width}x${format.height}@${format.fps ?? 0}${isHdr(format) ? ' HDR' : ''} ${kbps} ${container(format.mime_type)} ${link}${format.drm_families ? ' drm' : ''}`;
  return `${format.itag} ${codec} ${format.audio_sample_rate ?? 0}Hz ${format.audio_channels ?? 0}ch ${kbps}${format.language ? ` ${format.language}` : ''}${format.is_drc ? ' drc' : ''} ${container(format.mime_type)} ${link}`;
}

// Address facts that are safe to print.
export function redact(address) {
  try {
    const url = new URL(address);
    const expire = Number(url.searchParams.get('expire'));
    const domain = url.hostname.split('.').slice(-2).join('.');
    return `[${domain}${url.pathname.split('/').slice(0, 2).join('/')}; c=${url.searchParams.get('c') ?? '-'}; ` +
      `expires in ${expire ? Math.round(expire - Date.now() / 1000) : '?'} s; ${Array.from(url.searchParams.keys()).length} parameters]`;
  } catch { return '[unreadable address]'; }
}

// Sessions.create: eligible video rows, best first.
const RANK = { h264: 3, vp9: 2, hevc: 1, av1: 0 };
const WANTED = { avc: 'h264', vp9: 'vp9', av1: 'av1', hevc: 'hevc' };
export function selectVideo(list, request = {}) {
  const wanted = WANTED[request.codec ?? 'auto'], maxHeight = request.max_height ?? 1080, maxFps = request.max_fps ?? 0;
  return list.filter(format => format.has_video && linkKind(format) !== 'none' && formatCodec(format) in RANK &&
      (!wanted || formatCodec(format) === wanted) && number(format.height) > 0 && number(format.height) <= maxHeight &&
      (!maxFps || (number(format.fps) > 0 && number(format.fps) <= maxFps)) && !format.drm_families && !format.is_type_otf)
    .sort((a, b) => number(b.height) - number(a.height) || RANK[formatCodec(b)] - RANK[formatCodec(a)] ||
      number(b.fps) - number(a.fps) || Number(isHdr(b)) - Number(isHdr(a)) || rate(b) - rate(a));
}
// Audio-only rows, best first: requested language, then the original mix
// without dynamic-range compression, then AAC, then bitrate.
export function selectAudio(list, request = {}) {
  const score = format => [request.audio_language && format.language === request.audio_language ? 1 : 0,
    format.is_original !== false && !format.is_drc ? 1 : 0, formatCodec(format) === 'aac' ? 1 : 0, rate(format)];
  return list.filter(format => format.has_audio && !format.has_video && linkKind(format) !== 'none' &&
      ['aac', 'opus'].includes(formatCodec(format)) && !format.drm_families)
    .sort((a, b) => { const x = score(a), y = score(b); for (let i = 0; i < x.length; i++) if (x[i] !== y[i]) return y[i] - x[i]; return 0; });
}

// ElementTree's escaping, so the text equals the Python generator's.
const xmlText = value => String(value).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
const xmlAttribute = value => xmlText(value).replace(/"/g, '&quot;').replace(/\r/g, '&#13;').replace(/\n/g, '&#10;').replace(/\t/g, '&#09;');
// sessions.dash_manifest: the static manifest already proven with the PS5's
// FFmpeg demuxer. Each representation is one complete, seekable file.
export function dashManifest(duration, video, audio, videoUrl, audioUrl) {
  duration = Math.max(number(duration), 0.001);
  const seconds = duration.toFixed(3), segment = Math.max(1, Math.trunc(duration * 1000));
  const one = (kind, format, url) => {
    const webm = container(format.mime_type) === 'webm';
    const size = kind === 'video' ? ` width="${Math.trunc(number(format.width))}" height="${Math.trunc(number(format.height))}"` :
      ` audioSamplingRate="${Math.trunc(number(format.audio_sample_rate, 48000))}"`;
    const language = kind === 'audio' && format.language ? ` lang="${xmlAttribute(text(format.language, 16))}"` : '';
    return `<AdaptationSet contentType="${kind}" mimeType="${kind}/${webm ? 'webm' : 'mp4'}" codecs="${xmlAttribute(codecs(format.mime_type))}"${language}>` +
      `<Representation id="${kind}" bandwidth="${Math.max(1, Math.trunc(rate(format) || 128000))}"${size}><BaseURL>${xmlText(url)}</BaseURL>` +
      `<SegmentList duration="${segment}" timescale="1000"><SegmentURL media="${xmlAttribute(url)}" /></SegmentList></Representation></AdaptationSet>`;
  };
  return `<?xml version='1.0' encoding='utf-8'?>\n<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" type="static" ` +
    `mediaPresentationDuration="PT${seconds}S" minBufferTime="PT1S" profiles="urn:mpeg:dash:profile:isoff-on-demand:2011">` +
    `<Period duration="PT${seconds}S">${one('video', video, videoUrl)}${one('audio', audio, audioUrl)}</Period></MPD>`;
}

// captions.vtt_to_srt
export function vttToSrt(source) {
  if (source.length > 2 * 1024 * 1024) throw new ServiceError('captions_too_large', 'The caption file exceeded its size limit.');
  const stamp = '(?:[0-9]{2,}:)?[0-9]{2}:[0-9]{2}\\.[0-9]{3}';
  const timing = new RegExp(`^(${stamp})\\s+-->\\s+(${stamp})(?:\\s+.*)?$`);
  const entities = { amp: '&', lt: '<', gt: '>', quot: '"', apos: "'", nbsp: ' ' };
  const plain = line => line.replace(/<[^>]*>/g, '').replace(/&(#x[0-9a-f]+|#[0-9]+|[a-z]+);/gi, (whole, name) => {
    if (name[0] !== '#') return entities[name.toLowerCase()] ?? whole;
    const point = name[1].toLowerCase() === 'x' ? parseInt(name.slice(2), 16) : parseInt(name.slice(1), 10);
    return point > 0 && point <= 0x10ffff ? String.fromCodePoint(point) : whole;
  }).trim();
  const fix = value => (value.split(':').length === 2 ? `00:${value}` : value).replace('.', ',');
  const lines = source.replace(/^﻿/, '').replace(/\r\n?/g, '\n').split('\n');
  const cues = [];
  for (let i = 0; i < lines.length;) {
    const match = timing.exec(lines[i++].trim());
    if (!match) continue;
    const body = [];
    while (i < lines.length && lines[i].trim()) body.push(plain(lines[i++]));
    if (!body.some(Boolean)) continue;
    cues.push(`${cues.length + 1}\n${fix(match[1])} --> ${fix(match[2])}\n${body.join('\n')}`);
    if (cues.length > 50000) throw new ServiceError('captions_limit', 'The caption file contains too many cues.');
  }
  return cues.join('\n\n') + (cues.length ? '\n' : '');
}

// Caption addresses by language as yt-dlp lists them: human-made tracks,
// automatic ones with their machine translations, and (when asked) machine
// translations of the human-made tracks, named `<target>-<source>`.
export function captionMaps(info, translated) {
  const subtitles = new Map(), automatic = new Map();
  const tracks = (info.captions?.caption_tracks ?? []).filter(track => track.base_url && track.language_code);
  const languages = info.captions?.translation_languages ?? [];
  const address = (track, target) => {
    const url = new URL(track.base_url, 'https://www.youtube.com');
    url.searchParams.set('fmt', 'vtt');
    if (target) url.searchParams.set('tlang', target);
    return url.href;
  };
  for (const track of tracks) {
    const spoken = track.kind === 'asr', name = track.name?.toString?.() || track.language_code;
    (spoken ? automatic : subtitles).set(track.language_code, { name, url: address(track), translated: false });
    if (!track.is_translatable || (!spoken && !translated)) continue;
    for (const language of languages) {
      const target = language.language_code;
      if (!target || target === track.language_code) continue;
      const key = spoken ? target : `${target}-${track.language_code}`;
      if (!automatic.has(key)) automatic.set(key, { name: language.language_name?.toString?.() || target, url: address(track, target), translated: true });
    }
  }
  return { subtitles, automatic };
}

// yt-dlp's chapters from a description: lines that begin with a timestamp.
export function chaptersOf(description, duration) {
  const found = [];
  for (const line of String(description ?? '').split('\n').slice(0, 4096)) {
    const match = /^\s*[([]?((?:\d{1,2}:)?\d{1,2}:\d{2})[)\]]?\s*[-–—:|]?\s+(.+?)\s*$/.exec(line);
    if (!match) continue;
    const start = match[1].split(':').reduce((total, part) => total * 60 + Number(part), 0);
    if (start > duration || (found.length && start <= found.at(-1).start_time)) continue;
    found.push({ start_time: start, title: match[2] });
  }
  if (found.length < 2 || found[0].start_time !== 0) return [];
  return found.map((chapter, index) => ({ ...chapter, end_time: index + 1 < found.length ? found[index + 1].start_time : duration }));
}

// Chapters as yt-dlp orders its sources: the structured list of the watch
// page (the uploader's, else YouTube's automatic one), else timestamps in the
// description. Records are {start_time, title, end_time} in seconds.
export function chapterList(info, duration) {
  const maps = Array.from(info.player_overlays?.decorated_player_bar?.player_bar?.markers_map ?? []);
  for (const key of ['DESCRIPTION_CHAPTERS', 'AUTO_CHAPTERS']) {
    const rows = Array.from(maps.find(entry => entry?.marker_key === key)?.value?.chapters ?? []).slice(0, 512)
      .map(chapter => ({ start_time: Number(chapter?.time_range_start_millis) / 1000, title: String(chapter?.title?.toString?.() ?? '') }))
      .filter(chapter => Number.isFinite(chapter.start_time) && chapter.start_time >= 0 && chapter.start_time <= duration && chapter.title);
    const ordered = rows.every((chapter, index) => !index || chapter.start_time > rows[index - 1].start_time);
    if (rows.length && ordered) return rows.map((chapter, index) => ({ ...chapter, end_time: index + 1 < rows.length ? rows[index + 1].start_time : duration }));
  }
  return chaptersOf(info.basic_info?.short_description, duration);
}

// A Short: said by the player answer where it has a microformat; the client
// used here sends none, so the shape decides (portrait, at most three minutes).
export function isShort(info, list, facts = {}) {
  if (typeof facts.shorts === 'boolean') return facts.shorts;
  const pictures = list.filter(format => format.has_video && number(format.width) > 0), duration = number(info.basic_info?.duration);
  return info.basic_info?.is_live !== true && duration > 0 && duration <= 180 && pictures.length > 0 && pictures.every(format => number(format.height) > number(format.width));
}

// paid_content.disclosure
export function disclosure(value) {
  if (typeof value !== 'string') return '';
  const clean = Array.from(Array.from(value).slice(0, 2048).join('').replaceAll('\0', '').split(/\s+/).filter(Boolean).join(' '))
    .filter(character => character.codePointAt(0) >= 32 && character.codePointAt(0) !== 127);
  let out = '', used = 0;
  for (const character of clean) {
    const size = host.utf8Encode(character).byteLength;
    if (used + size > 240) break;
    out += character; used += size;
  }
  return out;
}

// HLS master playlist: variants with what the chooser needs, and the lines to
// rebuild a master for one of them.
export function hlsVariants(master, base) {
  const lines = String(master).replace(/\r/g, '').split('\n'), media = [], variants = [];
  const attribute = (line, name) => { const match = new RegExp(`[:,]${name}=(?:"([^"]*)"|([^,]*))`).exec(line); return match ? match[1] ?? match[2] ?? '' : ''; };
  const absolute = uri => new URL(uri, base).href;
  for (let i = 0; i < lines.length; i++) {
    const line = lines[i];
    if (line.startsWith('#EXT-X-MEDIA:')) media.push({ group: attribute(line, 'GROUP-ID'), type: attribute(line, 'TYPE'),
      line: line.replace(/URI="([^"]*)"/, (_whole, uri) => `URI="${absolute(uri)}"`) });
    if (!line.startsWith('#EXT-X-STREAM-INF:')) continue;
    const address = lines.slice(i + 1).find(next => next && !next.startsWith('#'));
    if (!address) continue;
    const [width, height] = attribute(line, 'RESOLUTION').split('x').map(Number), list = attribute(line, 'CODECS').split(',').map(codecName);
    const range = attribute(line, 'VIDEO-RANGE');
    variants.push({ line, url: safeRemote(absolute(address)), width: width || 0, height: height || 0, fps: Number(attribute(line, 'FRAME-RATE')) || 0,
      bitrate: Number(attribute(line, 'BANDWIDTH')) || 0, video_codec: list.find(name => name in RANK) ?? '',
      video_range: range === 'PQ' ? 'HDR10' : range === 'HLG' ? 'HLG' : 'SDR',
      audio_codec: list.find(name => ['aac', 'opus'].includes(name)) ?? '', audio_group: attribute(line, 'AUDIO') });
  }
  const text = variant => ['#EXTM3U', '#EXT-X-INDEPENDENT-SEGMENTS',
    ...media.filter(entry => entry.type === 'AUDIO' && entry.group === variant.audio_group).map(entry => entry.line), variant.line, variant.url, ''].join('\n');
  return variants.map(variant => ({ ...variant, audio_codec: variant.audio_codec || (variant.audio_group ? 'aac' : ''), master: text(variant) }));
}

// Ask one client for the player answer and say what came back.
// `full` also asks for the watch-next answer (structured chapters); if that
// request fails the player answer alone is used.
export async function playerResponse(yt, id, client, full = false) {
  const started = host.now();
  const info = (full ? await yt.getInfo(id, { client }).catch(() => null) : null) ?? await yt.getBasicInfo(id, { client });
  const list = formats(info);
  return { info, list, summary: {
    client, status: info.playability_status?.status ?? 'MISSING', reason: String(info.playability_status?.reason ?? '').slice(0, 120),
    request_ms: Math.round(host.now() - started), formats: list.length,
    direct: list.filter(format => linkKind(format) === 'direct').length,
    ciphered: list.filter(format => linkKind(format) === 'ciphered').length,
    sabr_only: !!info.streaming_data?.server_abr_streaming_url && !list.some(format => linkKind(format) !== 'none'),
    hls_manifest: !!info.streaming_data?.hls_manifest_url, captions: info.captions?.caption_tracks?.length ?? 0 } };
}

// The video as the resolver sees it: the first client in `order` that answers
// with something playable. Failures carry the companion's resolver codes.
export async function extract(yt, id, { order = PLAYBACK_ORDER, prepare = async () => {}, fetch, facts = () => ({}), playback = null, ranged = false } = {}) {
  let failure = 'extraction_failed';
  // A client is asked again (twice at most) only when YouTube refuses the
  // links it just handed out: seen live on 2026-10-11, where FFmpeg met 403 on
  // a new address that was served normally seconds later.
  const turns = order.flatMap(name => [0, 1, 2].map(attempt => ({ client: name, attempt })));
  let refusedBy = '';
  for (const { client, attempt } of turns) {
    if (attempt && refusedBy !== client) continue;
    if (attempt) await new Promise(resolve => setTimeout(resolve, 700 * attempt));
    refusedBy = '';
    let answer;
    try { await prepare(client); answer = await playerResponse(yt, id, client, true); }
    catch (error) { if (error instanceof ServiceError) throw error; failure = classify(`${error?.message} ${error?.info?.reason ?? ''}`); continue; }
    const { info, list, summary } = answer;
    if (info.basic_info?.id !== id) { failure = 'extraction_failed'; continue; }
    if (summary.status !== 'OK') { failure = classify(`${summary.status} ${summary.reason}`); continue; }
    const live = info.basic_info.is_live === true, address = info.streaming_data?.hls_manifest_url;
    let hls = [];
    // A live stream plays from YouTube's own HLS playlist; for a recording it
    // is the fallback when no separate tracks have an address.
    if (address && fetch && (live || !list.some(format => linkKind(format) !== 'none'))) {
      try {
        const response = await fetch(safeRemote(address));
        if (response.ok) hls = hlsVariants(await response.text(), address);
      } catch (error) { if (!(error instanceof ServiceError)) failure = 'resolver_network'; }
    }
    if (!list.some(format => linkKind(format) !== 'none') && !hls.length) { failure = live ? failure : 'extraction_failed'; continue; }
    // One kilobyte of the selected video file proves its link is served before
    // a player is pointed at it. Non-playback callers retain the first sample.
    const indexed = ranged ? list.filter(format => format.init_range && format.index_range) : [];
    const usable = indexed.length && selectVideo(indexed, playback ?? {}).some(format => !format.has_audio) &&
      selectAudio(indexed, playback ?? {}).length ? indexed : list;
    const sample = live ? null : playback ? selectVideo(usable, playback)[0] : list.find(format => format.has_video && format.url);
    if (sample?.url && fetch) {
      const reply = await fetch(safeRemote(sample.url), { headers: { Range: 'bytes=0-1023' }, max_bytes: 65536 }).catch(() => null);
      if (reply && [401, 403, 410].includes(reply.status)) { refusedBy = client; failure = 'media_refused'; continue; }
      // A newly signed link can serve its header but refuse a seek into the
      // file. Check one bounded deep range before handing it to FFmpeg.
      const length = Number(sample.content_length);
      if (playback && Number.isSafeInteger(length) && length > 131072) {
        const first = Math.floor(length / 2), last = Math.min(length - 1, first + 65535);
        const deep = await fetch(safeRemote(sample.url), { headers: { Range: `bytes=${first}-${last}` }, max_bytes: 131072 }).catch(() => null);
        if (deep && [401, 403, 410].includes(deep.status)) { refusedBy = client; failure = 'media_refused'; continue; }
      }
    }
    // How long the signed addresses stay valid, as the answer says (about six hours).
    const lifetime = number(info.streaming_data?.expires?.getTime?.() ? (info.streaming_data.expires.getTime() - Date.now()) / 1000 : 21600, 21600);
    return { id, client, info, list, hls, live, player: yt.session.player, facts: facts(id), lifetime };
  }
  if (failure === 'media_refused') throw new ServiceError('upstream_media', 'The media server could not provide this resource.', 502);
  throw resolverError(failure);
}

// Sessions.create. `session` receives what /media will serve: manifests,
// playlists, redirects for muxed files, caption and storyboard sources.
// `ranged` (a fetch for the media servers) asks for manifests whose segments
// are byte ranges read from each file's index; see segments.js. They are built
// when first requested, and fall back to the classic shape if an index cannot
// be read.
export async function createPlayback(source, request, prefix, session, ttl, ranged = null) {
  const { info, list, live } = source, details = info.basic_info;
  const duration = number(details.duration);
  session.duration = duration; session.is_live = live;
  const choices = [], seen = new Set();
  const address = async format => safeRemote(await format.decipher(source.player));
  const label = (height, fps, video, audio, range) => `${height}p${fps ? ` ${fps} fps` : ''} ${video.toUpperCase()} / ${audio.toUpperCase()}${range === 'SDR' ? '' : ` ${range}`}`;
  const push = (fields, format, video, audio, fps) => choices.push({ url: fields.url, label: label(Math.trunc(number(format.height)), fps, video, audio, videoRange(format)),
    container: fields.container, video_codec: video, audio_codec: audio, width: Math.trunc(number(format.width)), height: Math.trunc(number(format.height)),
    fps, bitrate_bps: Math.trunc(rate(format) || number(format.bitrate)), is_live: live, video_range: videoRange(format), audio_channels: 2 });

  if (source.hls.length) {
    const wanted = WANTED[request.codec];
    const variants = source.hls.filter(variant => variant.video_codec && (!wanted || variant.video_codec === wanted) &&
        variant.height > 0 && variant.height <= request.max_height && (!request.max_fps || (variant.fps > 0 && variant.fps <= request.max_fps)))
      .sort((a, b) => b.height - a.height || RANK[b.video_codec] - RANK[a.video_codec] || b.fps - a.fps || Number(isHdr(b)) - Number(isHdr(a)) || b.bitrate - a.bitrate);
    for (const variant of variants) {
      const key = `${variant.height}/${variant.video_codec}/${videoRange(variant)}`;
      if (seen.has(key) || choices.length >= 8 || !variant.audio_codec) continue;
      seen.add(key);
      const name = `h${choices.length}.m3u8`;
      session.hls_manifests.set(name, variant.master);
      push({ url: `${prefix}/${name}`, container: 'hls' }, variant, variant.video_codec, variant.audio_codec, variant.fps);
    }
  }
  if (!live && !choices.length) {
    // Since 2026-10-14 YouTube answers 403 to an open-ended or whole-file range
    // (`rqh=1` in the address; bounded ranges of a few megabytes are served), so
    // the one-segment shape no longer plays. With ranged manifests only tracks
    // with an MP4 or WebM segment index are offered; the full list is the
    // fallback when none has one.
    const indexed = list.filter(format => format.init_range && format.index_range);
    const usable = ranged && selectVideo(indexed, request).some(format => !format.has_audio) && selectAudio(indexed, request).length ? indexed : list;
    const audios = selectAudio(usable, request);
    for (const video of selectVideo(usable, request)) {
      const vcodec = formatCodec(video), key = `${video.height}/${vcodec}/${video.has_audio}/${videoRange(video)}`;
      if (seen.has(key) || choices.length >= 8) continue;
      const fps = Math.max(0, number(video.fps));
      if (video.has_audio) {
        // A muxed file is not relayed: its address answers with a redirect.
        const name = `p${choices.length}`;
        session.tracks.set(name, { kind: 'redirect', url: await address(video) });
        seen.add(key);
        push({ url: `${prefix}/${name}`, container: text(container(video.mime_type), 16) }, video, vcodec, formatCodec(video) && 'aac', fps);
      } else if (audios.length && duration) {
        const audio = audios[0], name = `d${choices.length}.mpd`, videoUrl = await address(video), audioUrl = await address(audio);
        const classic = dashManifest(duration, video, audio, videoUrl, audioUrl);
        let built = null;
        session.manifests.set(name, !ranged ? classic : () => built ??= (async () => {
          const index = (format, url) => fileIndex(rangeReader(ranged, url), format, duration).catch(() => null);
          const tracks = [{ kind: 'video', format: video, url: videoUrl, index: await index(video, videoUrl) },
            { kind: 'audio', format: audio, url: audioUrl, index: await index(audio, audioUrl) }];
          return tracks.some(track => track.index) ? rangedManifest(duration, tracks) : classic;
        })());
        seen.add(key);
        push({ url: `${prefix}/${name}`, container: 'dash' }, video, vcodec, formatCodec(audio), fps);
      }
    }
  }
  if (!choices.length) throw new ServiceError('unsupported_media', 'No compatible AVC/VP9/HEVC/AV1 stream was returned. This video may require a different runtime, token, transport, or quality setting.', 422);

  const { subtitles, automatic } = captionMaps(info, request.translated_captions);
  const human = new Set(subtitles.keys()), wanted = request.caption_language;
  const order = (a, b) => { const x = [a !== wanted, !(wanted && a.startsWith(`${wanted}-`)), a], y = [b !== wanted, !(wanted && b.startsWith(`${wanted}-`)), b];
    return x[0] - y[0] || x[1] - y[1] || (x[2] < y[2] ? -1 : x[2] > y[2] ? 1 : 0); };
  if (request.automatic_captions) { for (const [language, track] of automatic) if (!subtitles.has(language)) subtitles.set(language, track); }
  else if (request.translated_captions)
    for (const language of Array.from(automatic.keys()).filter(name => !subtitles.has(name) && automatic.get(name).translated).sort(order).slice(0, 128))
      subtitles.set(language, automatic.get(language));
  const captions = [];
  for (const language of Array.from(subtitles.keys()).sort(order).slice(0, 256)) {
    const track = subtitles.get(language), name = `c${captions.length}.srt`;
    try { session.tracks.set(name, { kind: 'caption', url: safeRemote(track.url) }); } catch { continue; }
    captions.push({ language: text(language, 32), name: text(track.name || language, 64), url: `${prefix}/${name}`,
      automatic: !human.has(language), translated: track.translated });
  }

  // Seek-preview sheets: `$M` in the returned template is the sheet number.
  const storyboards = [];
  const boards = live || duration <= 0 ? [] : Array.from(info.storyboards?.boards ?? []).filter(board => board?.type === 'vod')
    .sort((a, b) => number(a.thumbnail_width) * number(a.thumbnail_height) - number(b.thumbnail_width) * number(b.thumbnail_height));
  for (const board of boards.slice(0, 8)) {
    const width = Math.trunc(number(board.thumbnail_width)), height = Math.trunc(number(board.thumbnail_height));
    const columns = Math.trunc(number(board.columns)), rows = Math.trunc(number(board.rows)), count = Math.trunc(number(board.thumbnail_count));
    const sheets = Math.trunc(number(board.storyboard_count));
    if (storyboards.length >= 4 || sheets < 1 || sheets > 512 || width < 1 || width > 1024 || height < 1 || height > 1024 ||
        columns < 1 || columns > 32 || rows < 1 || rows > 32 || count < 1 || Math.ceil(count / (columns * rows)) !== sheets) continue;
    const level = storyboards.length;
    try { for (let index = 0; index < sheets; index++) session.tracks.set(`sb${level}-${index}.jpg`, { kind: 'image', url: safeRemote(String(board.template_url).replace('$M', String(index))) }); }
    catch { continue; }
    storyboards.push({ width, height, columns, rows, count, sheets, interval_ms: Math.max(1, Math.round(duration * 1000 / count)), url: `${prefix}/sb${level}-$M.jpg` });
  }
  const chapters = chapterList(info, duration).slice(0, 128)
    .map(chapter => ({ title: text(chapter.title, 96), start: number(chapter.start_time), end: number(chapter.end_time) }));
  // The player configuration names the loudness; some clients only put it on the audio formats.
  const loudness = info.player_config?.audio_config?.loudness_db ?? selectAudio(list, request)[0]?.loudness_db;
  const views = String(details.view_count ?? '');
  const channelId = String(details.channel_id ?? '');
  return { api_version: 1, id: request.id, title: text(details.title),
    channel: text(details.author), channel_id: /^UC[A-Za-z0-9_-]{22}$/.test(channelId) ? channelId : '',
    channel_art_url: '', views: /^\d{1,16}$/.test(views) ? views : '',
    published: text(info.primary_info?.published, 64),
    duration_sec: duration, expires_in: ttl, choices, captions, chapters,
    paid_content_text: disclosure(source.facts?.paid), loudness_db: typeof loudness === 'number' && Number.isFinite(loudness) && loudness >= -60 && loudness <= 60 ? loudness : null,
    is_short: isShort(info, list, source.facts), storyboards };
}

// One ranged read straight from the media server; reports facts, keeps no data.
export async function rangeCheck(fetch, address, first, last, headers = {}) {
  const started = host.now();
  try {
    const response = await fetch(address, { headers: { Range: `bytes=${first}-${last}`, ...headers }, redirect: 'follow', max_bytes: last - first + 65536 });
    const bytes = new Uint8Array(await response.arrayBuffer());
    const magic = bytes.length >= 8 ? String.fromCharCode(...bytes.slice(4, 8)).replace(/[^ -~]/g, '.') : '';
    return { status: response.status, content_range: response.headers.get('content-range') ?? '', bytes: bytes.length,
      content_type: response.headers.get('content-type') ?? '', redirected: response.redirected,
      starts_with: first === 0 ? (bytes[0] === 0x1a && bytes[1] === 0x45 ? 'EBML' : magic) : undefined, ms: Math.round(host.now() - started) };
  } catch (error) { return { status: 0, error: String(error.message).slice(0, 120), ms: Math.round(host.now() - started) }; }
}
