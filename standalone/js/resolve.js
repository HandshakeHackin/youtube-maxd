// Playback resolution without the companion: player response from a client
// that returns direct links, format policy, DASH pairing, captions.
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The policy mirrors companion/prosperotube/sessions.py (Sessions.create) so
// the native side keeps receiving the same `Playback` record; the difference
// is that every address points at YouTube itself instead of a relay.
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

export function codecName(mime) {
  const raw = /codecs="([^",]+)/.exec(mime ?? '')?.[1] ?? '';
  if (raw.startsWith('avc1')) return 'h264';
  if (raw.startsWith('vp9') || raw.startsWith('vp09')) return 'vp9';
  if (raw.startsWith('av01')) return 'av1';
  if (raw.startsWith('hvc1') || raw.startsWith('hev1')) return 'hevc';
  if (raw.startsWith('mp4a')) return 'aac';
  if (raw.startsWith('opus')) return 'opus';
  if (raw.startsWith('ec-3') || raw.startsWith('ac-3')) return 'dolby';
  return raw.split('.')[0];
}
const container = mime => /^[a-z]+\/([a-z0-9]+)/.exec(mime ?? '')?.[1] ?? '';
const isHdr = format => /HDR/.test(format.quality_label ?? '') || /^(vp09\.02|av01\.\d\.\d\d[A-Z]\.1[02])/.test(/codecs="([^"]+)/.exec(format.mime_type ?? '')?.[1] ?? '') ||
  ['SMPTEST2084', 'ARIB_STD_B67'].includes(format.color_info?.transfer_characteristics);
const linkKind = format => format.url ? 'direct' : format.signature_cipher || format.cipher ? 'ciphered' : 'none';

export function formats(info) {
  const data = info.streaming_data;
  return [...(data?.formats ?? []), ...(data?.adaptive_formats ?? [])];
}

// One compact row per format for reports; never contains an address.
export function formatRow(format) {
  const codec = codecName(format.mime_type);
  const rate = `${Math.round((format.average_bitrate || format.bitrate || 0) / 1000)}k`;
  const link = linkKind(format);
  if (format.has_video) {
    const audio = format.has_audio ? '+audio' : '';
    return `${format.itag} ${codec}${audio} ${format.width}x${format.height}@${format.fps ?? 0}${isHdr(format) ? ' HDR' : ''} ${rate} ${container(format.mime_type)} ${link}${format.drm_families ? ' drm' : ''}`;
  }
  return `${format.itag} ${codec} ${format.audio_sample_rate ?? 0}Hz ${format.audio_channels ?? 0}ch ${rate}${format.language ? ` ${format.language}` : ''}${format.is_drc ? ' drc' : ''} ${container(format.mime_type)} ${link}`;
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
const showAddresses = () => !!host.getenv('TUBE_JS_SHOW_URLS');
export const printable = address => showAddresses() ? address : redact(address);

export function mediaAddress(address) {
  const url = new URL(address);
  const domains = ['googlevideo.com', 'youtube.com'];
  if (url.protocol !== 'https:' || url.username || url.password || url.port ||
      !domains.some(domain => url.hostname === domain || url.hostname.endsWith(`.${domain}`)))
    throw new Error('The resolver returned an unsupported media address.');
  return url.href;
}

// Sessions.create: eligible video rows, best first.
export function selectVideo(list, request = {}) {
  const wanted = { avc: 'h264', vp9: 'vp9', av1: 'av1', hevc: 'hevc' }[request.codec ?? 'auto'];
  const rank = { h264: 3, vp9: 2, hevc: 1, av1: 0 };
  const maxHeight = request.max_height ?? 1080, maxFps = request.max_fps ?? 0;
  return list.filter(format => format.has_video && linkKind(format) !== 'none' && codecName(format.mime_type) in rank &&
      (!wanted || codecName(format.mime_type) === wanted) && format.height > 0 && format.height <= maxHeight &&
      (!maxFps || (format.fps > 0 && format.fps <= maxFps)) && !isHdr(format) && !format.drm_families && !format.is_type_otf)
    .sort((a, b) => b.height - a.height || rank[codecName(b.mime_type)] - rank[codecName(a.mime_type)] ||
      (b.fps ?? 0) - (a.fps ?? 0) || b.bitrate - a.bitrate);
}
export function selectAudio(list, request = {}) {
  const score = format => [request.audio_language && format.language === request.audio_language ? 1 : 0,
    format.is_original !== false && !format.is_drc ? 1 : 0, codecName(format.mime_type) === 'aac' ? 1 : 0, format.bitrate];
  return list.filter(format => format.has_audio && !format.has_video && linkKind(format) !== 'none' &&
      ['aac', 'opus'].includes(codecName(format.mime_type)) && !format.drm_families)
    .sort((a, b) => { const x = score(a), y = score(b); for (let i = 0; i < x.length; i++) if (x[i] !== y[i]) return y[i] - x[i]; return 0; });
}

const xml = text => String(text).replace(/[&<>"]/g, c => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' })[c]);
// The static manifest the player is already validated with (sessions.py,
// dash_manifest): each representation is one complete, seekable file.
export function dashManifest(duration, video, audio, videoUrl, audioUrl) {
  const seconds = Math.max(duration, 0.001).toFixed(3);
  const one = (kind, format, url) => {
    const codec = /codecs="([^"]+)/.exec(format.mime_type)?.[1] ?? '';
    const extra = kind === 'video' ? ` width="${format.width}" height="${format.height}"` : ` audioSamplingRate="${format.audio_sample_rate || 48000}"`;
    const language = kind === 'audio' && format.language ? ` lang="${xml(format.language.slice(0, 16))}"` : '';
    return `<AdaptationSet contentType="${kind}" mimeType="${kind}/${container(format.mime_type)}" codecs="${xml(codec)}"${language}>` +
      `<Representation id="${kind}" bandwidth="${Math.max(1, format.bitrate | 0)}"${extra}><BaseURL>${xml(url)}</BaseURL>` +
      `<SegmentList duration="${Math.max(1, Math.round(seconds * 1000))}" timescale="1000"><SegmentURL media="${xml(url)}"/></SegmentList>` +
      `</Representation></AdaptationSet>`;
  };
  return `<?xml version="1.0" encoding="UTF-8"?><MPD xmlns="urn:mpeg:dash:schema:mpd:2011" type="static" ` +
    `mediaPresentationDuration="PT${seconds}S" minBufferTime="PT1S" profiles="urn:mpeg:dash:profile:isoff-on-demand:2011">` +
    `<Period duration="PT${seconds}S">${one('video', video, videoUrl)}${one('audio', audio, audioUrl)}</Period></MPD>`;
}

// companion/prosperotube/captions.py, vtt_to_srt.
export function vttToSrt(source) {
  const stamp = '(?:[0-9]{2,}:)?[0-9]{2}:[0-9]{2}\\.[0-9]{3}';
  const timing = new RegExp(`^(${stamp})\\s+-->\\s+(${stamp})(?:\\s+.*)?$`);
  const entities = { amp: '&', lt: '<', gt: '>', quot: '"', apos: "'", nbsp: ' ' };
  const plain = text => text.replace(/<[^>]*>/g, '').replace(/&(#x[0-9a-f]+|#[0-9]+|[a-z]+);/gi, (whole, name) =>
    name[0] === '#' ? String.fromCodePoint(name[1].toLowerCase() === 'x' ? parseInt(name.slice(2), 16) : parseInt(name.slice(1), 10)) :
      entities[name.toLowerCase()] ?? whole).trim();
  const fix = text => (text.split(':').length === 2 ? `00:${text}` : text).replace('.', ',');
  const lines = source.replace(/^﻿/, '').replace(/\r\n?/g, '\n').split('\n');
  const cues = [];
  for (let i = 0; i < lines.length;) {
    const match = timing.exec(lines[i++].trim());
    if (!match) continue;
    const body = [];
    while (i < lines.length && lines[i].trim()) body.push(plain(lines[i++]));
    if (!body.some(Boolean)) continue;
    cues.push(`${cues.length + 1}\n${fix(match[1])} --> ${fix(match[2])}\n${body.join('\n')}`);
    if (cues.length > 50000) throw new Error('The caption file contains too many cues.');
  }
  return cues.join('\n\n') + (cues.length ? '\n' : '');
}

export function captionTracks(info) {
  return (info.captions?.caption_tracks ?? []).filter(track => track.base_url).map(track => {
    const url = new URL(track.base_url, 'https://www.youtube.com');
    url.searchParams.set('fmt', 'vtt');
    return { language: String(track.language_code ?? '').slice(0, 32), name: String(track.name?.toString() ?? track.language_code).slice(0, 64),
      url: url.href, automatic: track.kind === 'asr', translated: false };
  });
}

// Ask one client for the player response and say what came back.
export async function playerResponse(yt, id, client) {
  const started = host.now();
  const info = await yt.getBasicInfo(id, { client });
  const list = formats(info);
  return { info, list, summary: {
    client, status: info.playability_status?.status ?? 'MISSING', reason: String(info.playability_status?.reason ?? '').slice(0, 120),
    request_ms: Math.round(host.now() - started), formats: list.length,
    direct: list.filter(format => linkKind(format) === 'direct').length,
    ciphered: list.filter(format => linkKind(format) === 'ciphered').length,
    sabr_only: !!info.streaming_data?.server_abr_streaming_url && !list.some(format => linkKind(format) !== 'none'),
    hls_manifest: !!info.streaming_data?.hls_manifest_url, captions: info.captions?.caption_tracks?.length ?? 0 } };
}

// The `Playback` record of docs/PROTOCOL.md with direct addresses.
export async function playback(yt, request, order = PLAYBACK_ORDER, prepare = async () => {}) {
  let last = null;
  for (const client of order) {
    await prepare(client);
    const answer = await playerResponse(yt, request.id, client);
    last = answer.summary;
    const videos = selectVideo(answer.list, request), audios = selectAudio(answer.list, request);
    if (answer.summary.status !== 'OK' || !videos.length) continue;
    const { info } = answer, player = yt.session.player;
    const duration = Number(info.basic_info?.duration ?? 0);
    const choices = [], seen = new Set();
    let decipherMs = 0, expires = Infinity;
    const address = async format => {
      const started = host.now();
      const url = mediaAddress(await format.decipher(player));
      decipherMs += host.now() - started;
      const expire = Number(new URL(url).searchParams.get('expire'));
      if (expire) expires = Math.min(expires, expire - Math.floor(Date.now() / 1000));
      return url;
    };
    for (const video of videos) {
      const vcodec = codecName(video.mime_type), key = `${video.height}/${vcodec}/${video.has_audio}`;
      if (seen.has(key) || choices.length >= 8) continue;
      let choice;
      if (video.has_audio) choice = { url: await address(video), container: container(video.mime_type), audio_codec: 'aac' };
      else if (audios.length && duration) {
        const audio = audios[0], videoUrl = await address(video), audioUrl = await address(audio);
        choice = { url: '', container: 'dash', audio_codec: codecName(audio.mime_type), video_url: videoUrl, audio_url: audioUrl,
          manifest: dashManifest(duration, video, audio, videoUrl, audioUrl), video_itag: video.itag, audio_itag: audio.itag };
      } else continue;
      seen.add(key);
      choices.push({ label: `${video.height}p${video.fps ? ` ${video.fps} fps` : ''} ${vcodec.toUpperCase()} / ${choice.audio_codec.toUpperCase()}`,
        video_codec: vcodec, width: video.width, height: video.height, fps: video.fps ?? 0,
        bitrate_bps: video.bitrate | 0, is_live: false, video_range: 'SDR', audio_channels: 2, ...choice });
    }
    if (!choices.length) continue;
    return { api_version: 1, id: request.id, title: String(info.basic_info?.title ?? '').slice(0, 192), duration_sec: duration,
      expires_in: Number.isFinite(expires) ? expires : 0, client, decipher_ms: Math.round(decipherMs),
      choices, captions: captionTracks(info), chapters: [], is_short: false, storyboards: [] };
  }
  const error = new Error(last?.reason || 'No compatible stream was returned.');
  error.code = last && last.status !== 'OK' ? 'unplayable' : 'unsupported_media';
  throw error;
}

// Replace every address in a Playback record before it is printed.
export function printablePlayback(record) {
  if (showAddresses()) return record;
  const choices = record.choices.map(choice => {
    const copy = { ...choice };
    for (const key of ['url', 'video_url', 'audio_url']) if (copy[key]) copy[key] = redact(copy[key]);
    if (copy.manifest) { copy.manifest_bytes = copy.manifest.length; delete copy.manifest; }
    return copy;
  });
  return { ...record, choices, captions: record.captions.map(track => ({ ...track, url: redact(track.url) })) };
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
