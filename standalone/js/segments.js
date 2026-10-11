// A DASH manifest whose segments are byte ranges of YouTube's own files, read
// from each file's index (MP4 `sidx`, WebM Cues). SPDX-License-Identifier: GPL-3.0-or-later
//
// Why: FFmpeg's DASH demuxer cannot seek inside a segment. With the classic
// one-segment-per-file manifest a jump forward reads everything in between,
// and YouTube paces one long request at about twice real time (a jump of five
// minutes took 150 s on the host). With one SegmentURL per fragment the
// demuxer asks for exactly the bytes it needs.
import { number, text } from './errors.js';

const view = bytes => new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);

// ISO BMFF segment index. `indexStart` is the file offset of `bytes`.
export function sidxSegments(bytes, indexStart) {
  const data = view(bytes);
  if (bytes.length < 32 || String.fromCharCode(...bytes.slice(4, 8)) !== 'sidx') throw new Error('no segment index');
  const version = bytes[8], timescale = data.getUint32(16);
  let at = 20, first;
  if (version === 0) { first = data.getUint32(at + 4); at += 8; }
  else { first = data.getUint32(at + 8) * 4294967296 + data.getUint32(at + 12); at += 16; }
  const count = data.getUint16(at + 2);
  at += 4;
  let offset = indexStart + data.getUint32(0) + first;
  const segments = [];
  for (let i = 0; i < count && at + 12 <= bytes.length; i++, at += 12) {
    const size = data.getUint32(at) & 0x7fffffff, duration = data.getUint32(at + 4);
    if (data.getUint32(at) >>> 31) throw new Error('nested segment index');
    segments.push({ start: offset, end: offset + size - 1, duration });
    offset += size;
  }
  if (!timescale || !segments.length) throw new Error('empty segment index');
  return { timescale, segments };
}

// EBML: element id and data size at `at`.
function element(bytes, at) {
  const width = lead => { let n = 1; while (n <= 8 && !(lead & (0x80 >> (n - 1)))) n++; return n; };
  const idSize = width(bytes[at]), sizeSize = width(bytes[at + idSize]);
  if (idSize > 4 || sizeSize > 8 || at + idSize + sizeSize > bytes.length) return null;
  let id = 0, size = bytes[at + idSize] & (0xff >> sizeSize);
  for (let i = 0; i < idSize; i++) id = id * 256 + bytes[at + i];
  for (let i = 1; i < sizeSize; i++) size = size * 256 + bytes[at + idSize + i];
  return { id, size, data: at + idSize + sizeSize };
}
const unsigned = (bytes, at, size) => { let value = 0; for (let i = 0; i < size; i++) value = value * 256 + bytes[at + i]; return value; };

// Matroska: clusters from the Cues. `head` is the start of the file (EBML
// header, Segment, Info), `cues` the Cues element, `length` the file size.
export function cueSegments(head, cues, length, durationSeconds) {
  let at = 0, segmentStart = -1, scale = 1000000;
  for (let item; segmentStart < 0 && (item = element(head, at)); at = item.data + item.size)
    if (item.id === 0x18538067) segmentStart = item.data;
  if (segmentStart < 0) throw new Error('no segment');
  for (let item, inner = segmentStart; (item = element(head, inner)) && item.data + item.size <= head.length; inner = item.data + item.size) {
    if (item.id !== 0x1549A966) continue;      // Info: the tick length
    for (let field, pos = item.data; pos < item.data + item.size && (field = element(head, pos)); pos = field.data + field.size)
      if (field.id === 0x2AD7B1) scale = unsigned(head, field.data, field.size);
  }
  const top = element(cues, 0);
  if (!top || top.id !== 0x1C53BB6B) throw new Error('no cues');
  const points = [];
  for (let point, pos = top.data; pos < Math.min(cues.length, top.data + top.size) && (point = element(cues, pos)); pos = point.data + point.size) {
    if (point.id !== 0xBB) continue;
    let time = 0, cluster = -1;
    for (let field, inner = point.data; inner < point.data + point.size && (field = element(cues, inner)); inner = field.data + field.size) {
      if (field.id === 0xB3) time = unsigned(cues, field.data, field.size);
      if (field.id === 0xB7)
        for (let entry, deep = field.data; deep < field.data + field.size && (entry = element(cues, deep)); deep = entry.data + entry.size)
          if (entry.id === 0xF1) cluster = unsigned(cues, entry.data, entry.size);
    }
    if (cluster >= 0 && (!points.length || cluster > points.at(-1).cluster)) points.push({ time: time * scale / 1000000, cluster: segmentStart + cluster });
  }
  if (!points.length) throw new Error('empty cues');
  const total = Math.round(durationSeconds * 1000);
  return { timescale: 1000, segments: points.map((point, index) => ({ start: point.cluster,
    end: (index + 1 < points.length ? points[index + 1].cluster : length) - 1,
    duration: Math.max(1, Math.round((index + 1 < points.length ? points[index + 1].time : Math.max(total, point.time + 1)) - point.time)) })) };
}

// FFmpeg finds the segment for a seek by dividing the time by one fixed
// `duration` (it reads no SegmentTimeline inside a SegmentList), but fragments
// differ in length. So fragments are grouped on a grid: with F the longest
// fragment, segment k holds the fragments that end in (kF, (k+1)F], which
// makes its first fragment the one that contains the time kF. A seek to t then
// opens a segment that starts at or before t and at most 2F earlier; the
// timestamps inside the fragments are the real ones.
export function evenSegments({ segments: fragments }) {
  const step = Math.max(...fragments.map(fragment => fragment.duration));
  const segments = [];
  let time = 0;
  for (const fragment of fragments) {
    time += fragment.duration;
    const slot = Math.ceil(time / step) - 1;
    // Every window of length F holds a fragment end, so slots fill in order.
    if (slot >= segments.length) segments.push({ start: fragment.start, end: fragment.end }); else segments.at(-1).end = fragment.end;
  }
  return { step, segments };
}

const xmlText = value => String(value).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
const codecs = mime => /codecs="([^"]+)/.exec(mime ?? '')?.[1] ?? '';

// tracks: [{ kind: 'video' | 'audio', format, url, index: { timescale, segments } }]
export function rangedManifest(duration, tracks) {
  const seconds = Math.max(number(duration), 0.001).toFixed(3);
  const whole = Math.max(1, Math.trunc(Math.max(number(duration), 0.001) * 1000));
  const one = ({ kind, format, url, index: fragments }) => {
    const { step, segments } = fragments ? evenSegments(fragments) : {}, index = { timescale: fragments?.timescale, segments };
    const webm =/^[a-z]+\/webm/.test(format.mime_type ?? '');
    const size = kind === 'video' ? ` width="${Math.trunc(number(format.width))}" height="${Math.trunc(number(format.height))}"` :
      ` audioSamplingRate="${Math.trunc(number(format.audio_sample_rate, 48000))}"`;
    const language = kind === 'audio' && format.language ? ` lang="${xmlText(text(format.language, 16)).replace(/"/g, '&quot;')}"` : '';
    // Every segment repeats the address in an attribute, as the classic
    // manifest does: FFmpeg re-parses the BaseURL text and loses an `&` there.
    const address = xmlText(url).replace(/"/g, '&quot;');
    return `<AdaptationSet contentType="${kind}" mimeType="${kind}/${webm ? 'webm' : 'mp4'}" codecs="${xmlText(codecs(format.mime_type)).replace(/"/g, '&quot;')}"${language}>` +
      `<Representation id="${kind}" bandwidth="${Math.max(1, Math.trunc(number(format.average_bitrate || format.bitrate) || 128000))}"${size}>` +
      // A track without a usable index keeps the one-segment shape.
      (!fragments ? `<BaseURL>${xmlText(url)}</BaseURL><SegmentList duration="${whole}" timescale="1000"><SegmentURL media="${address}" />` +
        `</SegmentList></Representation></AdaptationSet>` :
      `<BaseURL>${xmlText(url)}</BaseURL><SegmentList timescale="${index.timescale}" duration="${step}">` +
      `<Initialization sourceURL="${address}" range="${format.init_range.start}-${format.init_range.end}" />` +
      index.segments.map(segment => `<SegmentURL media="${address}" mediaRange="${segment.start}-${segment.end}" />`).join('') +
      `</SegmentList></Representation></AdaptationSet>`);
  };
  return `<?xml version='1.0' encoding='utf-8'?>\n<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" type="static" ` +
    `mediaPresentationDuration="PT${seconds}S" minBufferTime="PT1S" profiles="urn:mpeg:dash:profile:isoff-on-demand:2011">` +
    `<Period duration="PT${seconds}S">${tracks.map(one).join('')}</Period></MPD>`;
}

// Reads the index of one file (one small ranged request).
export async function fileIndex(fetch, format, url) {
  const range = async (first, last) => {
    const response = await fetch(url, { headers: { Range: `bytes=${first}-${last}` }, max_bytes: 4 * 1024 * 1024 });
    if (response.status !== 206) throw new Error(`index read answered ${response.status}`);
    return new Uint8Array(await response.arrayBuffer());
  };
  const init = format.init_range, index = format.index_range;
  if (!init || !index || !(index.end > index.start) || index.end - index.start > 4 * 1024 * 1024) throw new Error('no index range');
  // WebM is left out for now: with cluster ranges (cueSegments) host FFmpeg
  // played from the start but a seek did not return within 40 s (2026-10-11).
  if (/^[a-z]+\/webm/.test(format.mime_type ?? '')) throw new Error('WebM tracks keep the one-segment shape');
  return sidxSegments(await range(index.start, index.end), index.start);
}
