// Optional decorations and third-party lookups: thumbnail frames, DeArrow,
// SponsorBlock. SPDX-License-Identifier: GPL-3.0-or-later
//
// Ports of companion/prosperotube/thumbnail_source.py, dearrow.py and the
// sponsor route of app.py. None of them may fail a catalog or playback.
import { createHash } from './node_crypto.js';
import { safeRemote } from './errors.js';
import { deadline, scoped, tokenUrlsafe } from './core.js';

const host = globalThis.__host;
const VIDEO = /^[A-Za-z0-9_-]{11}$/;
const prefixOf = id => createHash('sha256').update(id).digest('hex').slice(0, 4);
const finite = value => typeof value === 'number' && Number.isFinite(value);
const pythonList = values => `[${values.map(value => JSON.stringify(value)).join(', ')}]`;   // json.dumps spacing

// thumbnail_source.apply
const FRAMES = { start: 'hq1', middle: 'hq2', end: 'hq3' };
export function thumbnails(result, source) {
  if (!Object.hasOwn(FRAMES, source)) return result;
  const data = { ...result };
  if (data.kind !== undefined && data.kind !== null && data.kind !== 'catalog') return data;
  const single = !Array.isArray(data.items);
  const rows = single ? (data.item && typeof data.item === 'object' ? [data.item] : []) : data.items;
  const copies = rows.slice(0, 4096).map(row => {
    if (!row || typeof row !== 'object' || row.is_folder || row.is_live || row.is_upcoming || !VIDEO.test(String(row.id ?? ''))) return row;
    let name = '';
    try { name = new URL(String(row.art_url ?? '')).hostname; } catch { /* not an address */ }
    return name === 'ytimg.com' || name.endsWith('.ytimg.com') ? { ...row, art_url: `https://i.ytimg.com/vi/${row.id}/${FRAMES[source]}.jpg` } : row;
  });
  if (!single) data.items = copies; else if (copies.length) data.item = copies[0];
  return data;
}

// ---- DeArrow ----
function trusted(rows) {
  if (!Array.isArray(rows)) return null;
  const row = rows.slice(0, 64).find(entry => entry && typeof entry === 'object' && entry.original === false);
  if (!row) return null;
  return row.locked === true || (finite(row.votes) && row.votes >= 0) ? row : null;
}
function cut(value, bytes) {   // at most `bytes` of UTF-8, never half a character
  let out = '', used = 0;
  for (const character of value) {
    const size = host.utf8Encode(character).byteLength;
    if (used + size > bytes) break;
    out += character; used += size;
  }
  return out;
}
export function branding(data) {
  if (!data || typeof data !== 'object') return { title: '', stamp: null };
  let title = '', stamp = null, row = trusted(data.titles);
  if (row && typeof row.title === 'string') {
    title = Array.from(row.title).slice(0, 2048).join('').replaceAll('>', '').replaceAll('\0', '').split(/\s+/).filter(Boolean).join(' ');
    title = cut(Array.from(title).filter(c => c.codePointAt(0) >= 32 && c.codePointAt(0) !== 127).join(''), 191);
  }
  row = trusted(data.thumbnails);
  if (row && finite(row.timestamp) && row.timestamp >= 0 && row.timestamp < 604800) stamp = row.timestamp;
  return { title, stamp };
}

export class DeArrow {
  constructor(fetch = scoped(['sponsor.ajay.app', 'dearrow-thumb.ajay.app', 'ytimg.com', 'ggpht.com', 'googleusercontent.com'], { timeout: 3000, limit: 2 * 1024 * 1024 })) {
    this.fetch = fetch; this.cache = new Map(); this.images = new Map(); this.active = 0; this.waiting = [];
  }
  async limited(work) {   // four lookups at a time
    if (this.active >= 4) await new Promise(resolve => this.waiting.push(resolve));
    this.active++;
    try { return await work(); } finally { this.active--; this.waiting.shift()?.(); }
  }
  async lookup(id) {
    const prefix = prefixOf(id);
    return this.limited(async () => {
      const cached = this.cache.get(prefix);
      if (cached && cached.until > host.now()) { this.cache.delete(prefix); this.cache.set(prefix, cached); return branding(cached.data[id]); }
      let data = {};
      try {
        const response = await this.fetch(`https://sponsor.ajay.app/api/branding/${prefix}`, { max_bytes: 1024 * 1024 });
        const fetched = response.status === 200 ? await response.json() : null;
        if (fetched && typeof fetched === 'object' && !Array.isArray(fetched)) data = fetched;
      } catch { /* an optional service */ }
      this.cache.delete(prefix);
      this.cache.set(prefix, { until: host.now() + (Object.keys(data).length ? 3600000 : 60000), data });
      while (this.cache.size > 128) this.cache.delete(this.cache.keys().next().value);
      return branding(data[id]);
    });
  }
  imageUrl(id, stamp, fallback, base) {
    try { fallback = safeRemote(fallback); } catch { fallback = `https://i.ytimg.com/vi/${id}/hqdefault.jpg`; }
    const until = host.now() + 3600000;
    for (const [key, record] of this.images)
      if (record.id === id && record.stamp === stamp && record.fallback === fallback) {
        this.images.delete(key); this.images.set(key, { ...record, until });
        return `${base}/art/dearrow/${key}`;
      }
    const key = tokenUrlsafe(24);
    this.images.set(key, { until, id, stamp, fallback });
    while (this.images.size > 512) this.images.delete(this.images.keys().next().value);
    return `${base}/art/dearrow/${key}`;
  }
  async decorate(result, titles, pictures, base) {
    if (!titles && !pictures) return result;
    const data = { ...result };
    if (data.kind !== undefined && data.kind !== null && data.kind !== 'catalog') return data;
    const rows = Array.isArray(data.items) ? data.items : data.item && typeof data.item === 'object' ? [data.item] : [];
    const copies = rows.map(row => row && typeof row === 'object' ? { ...row } : row);
    if ('items' in data) data.items = copies; else if (copies.length) data.item = copies[0];
    const one = async row => {
      if (!row || typeof row !== 'object' || row.is_folder || !VIDEO.test(String(row.id ?? ''))) return;
      const { title, stamp } = await this.lookup(row.id);
      if (titles && title) row.dearrow_title = title;
      if (pictures && stamp !== null) row.dearrow_art_url = this.imageUrl(row.id, stamp, row.art_url ?? '', base);
    };
    // A delayed optional service cannot fail the catalog.
    try { await deadline(6000, Promise.all(copies.slice(0, 64).map(one)), () => new Error('late')); } catch { /* rows keep what arrived */ }
    return data;
  }
  async thumbnail(key) {
    const record = this.images.get(key);
    if (!record) return null;
    const generated = `https://dearrow-thumb.ajay.app/api/v1/getThumbnail?videoID=${record.id}&time=${record.stamp}`;
    return this.limited(async () => {
      for (const url of record.until > host.now() ? [generated, record.fallback] : [record.fallback]) {
        try {
          const response = await this.fetch(url);
          if (response.status !== 200) continue;
          const bytes = new Uint8Array(await response.arrayBuffer()), starts = (...values) => values.every((value, at) => bytes[at] === value);
          if (starts(0xff, 0xd8, 0xff)) return { bytes, type: 'image/jpeg' };
          if (starts(0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a)) return { bytes, type: 'image/png' };
          if (starts(0x52, 0x49, 0x46, 0x46) && bytes[8] === 0x57 && bytes[9] === 0x45 && bytes[10] === 0x42 && bytes[11] === 0x50) return { bytes, type: 'image/webp' };
        } catch { /* try the original image */ }
      }
      return null;
    });
  }
}

// ---- SponsorBlock: hashed-prefix query, so the service never learns the video ----
const CATEGORIES = ['sponsor', 'selfpromo', 'interaction', 'intro', 'outro', 'preview', 'music_offtopic', 'filler', 'poi_highlight'];
export function sponsorOrigin(value) {   // app.sponsorblock_origin
  let url;
  try { url = new URL(value); } catch { url = null; }
  if (!url || url.protocol !== 'https:' || !/^[a-z0-9](?:[a-z0-9.-]{0,251}[a-z0-9])?$/.test(url.hostname) || url.username || url.password ||
      url.port || !['', '/'].includes(url.pathname) || url.search || url.hash || String(value).length > 300) throw new Error('The SponsorBlock server must be an https:// origin.');
  return `https://${url.hostname}`;
}
export class Sponsor {
  constructor(enabled, server) { this.enabled = !!enabled; this.server = server ? sponsorOrigin(server) : null; this.cache = new Map(); }
  static chosen(categories) {
    const chosen = Array.from(new Set(String(categories).split(',')));
    return chosen.every(name => CATEGORIES.includes(name)) ? chosen.sort() : null;
  }
  async segments(id, chosen, alt) {
    if (!this.enabled) return { segments: [], enabled: false };
    const server = alt && this.server ? this.server : 'https://sponsor.ajay.app';
    const key = `${id}|${chosen.join(',')}|${server}`, cached = this.cache.get(key);
    if (cached && cached.until > host.now()) return { segments: cached.segments, enabled: true };
    const segments = [];
    try {
      const actions = chosen.includes('poi_highlight') ? ['skip', 'poi'] : ['skip'];
      const fetch = scoped([new URL(server).hostname], { timeout: 5000, limit: 1024 * 1024 });
      const response = await fetch(`${server}/api/skipSegments/${prefixOf(id)}?categories=${encodeURIComponent(pythonList(chosen))}&actionTypes=${encodeURIComponent(pythonList(actions))}`);
      const payload = response.status === 200 ? await response.json() : null;
      for (const result of Array.isArray(payload) ? payload : []) {
        if (!result || typeof result !== 'object' || result.videoID !== id || !Array.isArray(result.segments)) continue;
        for (const entry of result.segments) {
          if (!entry || typeof entry !== 'object' || typeof entry.category !== 'string') continue;
          const pair = entry.segment, point = entry.category === 'poi_highlight';
          if (chosen.includes(entry.category) && (entry.actionType ?? 'skip') === (point ? 'poi' : 'skip') && Array.isArray(pair) && pair.length === 2 &&
              pair.every(finite) && pair[0] >= 0 && pair[0] <= pair[1] && pair[1] < 604800 && (point ? pair[0] === pair[1] : pair[0] < pair[1]))
            segments.push({ start: pair[0], end: pair[1], category: entry.category });
        }
      }
    } catch { /* optional service failures never become playback failures */ }
    if (this.cache.size >= 128) this.cache.delete(this.cache.keys().next().value);
    this.cache.set(key, { until: host.now() + 3600000, segments: segments.slice(0, 128) });
    return { segments: segments.slice(0, 128), enabled: true };
  }
}
