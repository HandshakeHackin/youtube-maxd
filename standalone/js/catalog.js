// Search, channel and playlist pages by page number over the library's
// cursors, and the single-video record. SPDX-License-Identifier: GPL-3.0-or-later
//
// Port of companion/prosperotube/catalog.py (item, page_for) and of the
// catalog paging in oauth_extractor.py (_catalog).
import { ServiceError, number, safeRemote, text } from './errors.js';
import { captionMaps, chapterList } from './resolve.js';

const host = globalThis.__host;
const VIDEO = /^[A-Za-z0-9_-]{11}$/, CHANNEL = /^UC[A-Za-z0-9_-]{22}$/, PLAYLIST = /^[A-Za-z0-9_-]{10,100}$/;
const GROUP = /^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/;
const PAGE_SIZE = 24;
const index = value => Number.isInteger(value) && value >= 0 && value <= 100000;

// catalog.item: one row of a Page.
export function item(info) {
  const id = String(info?.id ?? '');
  if (!VIDEO.test(id)) return null;
  let art = `https://i.ytimg.com/vi/${id}/hqdefault.jpg`;
  try { if (info.thumbnail) art = safeRemote(info.thumbnail); } catch { /* the default image stays */ }
  const percent = info.watch_percent;
  return { id, title: text(info.title) || 'Unavailable video', subtitle: text(info.channel, 160), overview: text(info.description, 320),
    art_url: art, channel_id: text(info.channel_id, 64), duration_sec: Math.trunc(number(info.duration)), is_folder: false,
    is_live: info.is_live === true, is_short: info.is_short === true, resume_pos_sec: 0, played: false,
    watch_percent: typeof percent === 'number' && percent >= 0 && percent <= 100 ? percent : -1, playback_context: null };
}

// The fields of a resolved video that the catalog and details answers use.
export function videoRecord(source) {
  const details = source.info.basic_info, duration = number(details.duration);
  return { id: details.id, title: details.title, channel: details.author, channel_id: details.channel_id,
    description: details.short_description, thumbnail: details.thumbnail?.[0]?.url ?? '', duration, is_live: details.is_live === true,
    chapters: chapterList(source.info, duration),
    caption_languages: Array.from(captionMaps(source.info, false).subtitles.keys()).sort().slice(0, 32) };
}

function context(row, playlist, channel) {
  const supplied = row.playback_context;
  if (!supplied || typeof supplied !== 'object') return null;
  if (playlist) return supplied.playlist === playlist && index(supplied.index) ? { playlist, index: supplied.index } : null;
  if (channel) return CHANNEL.test(channel) && supplied.channel === channel && !('playlist' in supplied) && index(supplied.index) ? { channel, index: supplied.index } : null;
  const keys = Object.keys(supplied).sort().join(',');
  return keys === 'group,index' && typeof supplied.group === 'string' && GROUP.test(supplied.group) && Number.isInteger(supplied.index) &&
    supplied.index >= 0 && supplied.index <= 4095 ? { group: supplied.group, index: supplied.index } : null;
}

export class Catalog {
  // execute(request) runs a library `playback` operation; video(id, hl) resolves one video.
  constructor(execute, video) { this.execute = execute; this.video = video; this.catalogs = new Map(); this.lock = Promise.resolve(); }

  // oauth_extractor._catalog: page N is reached by following cursors in order;
  // a query is never re-run to guess an offset.
  async pages(operation, page) {
    const key = JSON.stringify(operation, Object.keys(operation).sort());
    const run = this.lock.then(async () => {
      const now = host.now();
      for (const [name, entry] of this.catalogs) if (entry.deadline <= now) this.catalogs.delete(name);
      let entry = this.catalogs.get(key);
      if (entry && page < entry.next && !entry.pages.has(page)) entry = null;
      if (!entry) {
        if (this.catalogs.size >= 32) this.catalogs.delete(this.catalogs.keys().next().value);
        entry = { deadline: now + 110000, pages: new Map(), next: 0, cursor: '' };
        this.catalogs.set(key, entry);
      }
      while (entry.next <= page) {
        if (entry.next && !entry.cursor) return { entries: [], more: false };
        let result;
        try { result = await this.execute(entry.next ? { action: 'catalog', cursor: entry.cursor } : operation); }
        catch (error) { this.catalogs.delete(key); throw error; }
        const records = result.items, cursor = result.cursor ?? '';
        if (!Array.isArray(records) || records.length > 24 || typeof result.has_more !== 'boolean' ||
            (result.has_more && (typeof cursor !== 'string' || !/^[A-Za-z0-9_-]{1,64}$/.test(cursor)))) {
          this.catalogs.delete(key);
          throw new ServiceError('library_transport', 'The TV catalog response was invalid.', 502);
        }
        entry.pages.set(entry.next, result);
        entry.next += 1;
        entry.cursor = result.has_more ? cursor : '';
        if (entry.pages.size > 16) entry.pages.delete(entry.pages.keys().next().value);
      }
      const stored = entry.pages.get(page);
      return { entries: stored.items.filter(record => record && VIDEO.test(record.id ?? '')).map(record => ({ id: record.id, title: record.title ?? '',
        channel: record.subtitle ?? '', channel_id: record.channel_id ?? '', description: record.overview ?? '', thumbnail: record.art_url ?? '',
        duration: record.duration_sec ?? 0, is_live: record.is_live ?? false, watch_percent: record.watch_percent ?? -1,
        is_short: record.is_short === true, playback_context: record.playback_context })), more: stored.has_more };
    });
    this.lock = run.catch(() => {});
    return run;
  }

  // catalog.page_for
  async pageFor({ query = '', parent = '', page = 0, options = {} }) {
    if (!Number.isInteger(page) || page < 0 || page > 1000) throw new ServiceError('invalid_page', 'The requested page is outside the supported range.', 400);
    const empty = { api_version: 1, items: [], has_more: false, page };
    let operation;
    if (query) {
      if (Array.from(query).length > 200 || !query.trim()) throw new ServiceError('invalid_query', 'Search text must contain at most 200 characters.', 400);
      query = query.trim();
      if (query.startsWith('channel:') || query.startsWith('playlist:')) return this.pageFor({ parent: query, page, options });
      let identifier = query.startsWith('video:') ? query.slice(6) : '';
      if (query.startsWith('https://') || query.startsWith('http://')) {
        let link;
        try { link = new URL(query); } catch { throw new ServiceError('invalid_query', 'The YouTube link is invalid.', 400); }
        if (['youtube.com', 'www.youtube.com', 'm.youtube.com', 'youtu.be'].includes(link.hostname)) {
          const playlist = link.searchParams.get('list') ?? '';
          if (playlist && PLAYLIST.test(playlist)) return this.pageFor({ parent: `playlist:${playlist}`, page, options });
          if (link.pathname.startsWith('/channel/')) return this.pageFor({ parent: `channel:${link.pathname.split('/')[2]}`, page, options });
          identifier = link.hostname === 'youtu.be' ? link.pathname.replace(/^\/+|\/+$/g, '') : link.searchParams.get('v') ?? '';
          if (!identifier && (link.pathname.startsWith('/shorts/') || link.pathname.startsWith('/live/'))) identifier = link.pathname.split('/')[2];
        }
      }
      if (identifier) {
        if (!VIDEO.test(identifier)) throw new ServiceError('invalid_video', 'The YouTube video ID is invalid.', 400);
        if (page) return empty;
        const row = item(videoRecord(await this.video(identifier, options.hl ?? '')));
        return { ...empty, items: row ? [row] : [], page: 0 };
      }
      operation = { action: 'catalog', kind: 'search', query };
    } else if (parent.startsWith('channel:') && CHANNEL.test(parent.slice(8))) operation = { action: 'catalog', kind: 'channel', id: parent.slice(8) };
    else if (parent.startsWith('playlist:') && PLAYLIST.test(parent.slice(9))) operation = { action: 'catalog', kind: 'playlist', id: parent.slice(9) };
    else throw new ServiceError('invalid_catalog', 'Choose search, a channel, or a YouTube playlist.', 400);
    for (const name of ['hl', 'gl']) if (options[name]) operation[name] = options[name];
    if (options.hide) operation.hide = options.hide.slice().sort();
    const { entries, more } = await this.pages(operation, page);
    const playlist = operation.kind === 'playlist' ? operation.id : '', channel = operation.kind === 'channel' ? operation.id : '';
    const rows = [], seen = new Set();
    for (const raw of entries.slice(0, PAGE_SIZE)) {
      const row = item(raw);
      if (!row) continue;
      row.playback_context = context(raw, playlist, channel);
      if (playlist || channel || row.playback_context || !seen.has(row.id)) { rows.push(row); seen.add(row.id); }
    }
    return { api_version: 1, items: rows, has_more: more === true, page };
  }
}
