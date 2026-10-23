// OAuth TV browse cards use the same bounded raw tile adapter as TV topics.
// SPDX-License-Identifier: GPL-3.0-or-later
import { YT } from 'youtubei.js';
import { TopicCatalog, find } from './topic_catalog.mjs';
import { hides, isWatched } from './content_filters.mjs';

const channelPattern = /^UC[A-Za-z0-9_-]{22}$/;
const alphabet = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/';
const label = value => (typeof value === 'string' ? value :
  String(value?.simpleText ?? value?.runs?.map(run => run?.text ?? '').join('') ?? '')).replaceAll('\0', '').slice(0, 192);
// The bytes of a (URL-encoded) base64 or base64url string as Latin-1 text; '' for anything else.
function decoded(value) {
  let source;
  try { source = decodeURIComponent(value); } catch { return ''; }
  source = source.replaceAll('-', '+').replaceAll('_', '/').replace(/=+$/, '');
  if (!/^[A-Za-z0-9+/]*$/.test(source)) return '';
  let out = '', bits = 0, value24 = 0;
  for (const character of source) {
    value24 = ((value24 << 6) | alphabet.indexOf(character)) & 0xffffff; bits += 6;
    if (bits >= 8) { bits -= 8; out += String.fromCharCode((value24 >> bits) & 255); }
  }
  return out;
}
// A TV Subscriptions tab names its channel inside the browse parameters (a
// message whose first string is the channel ID, MediaServiceCore 7f36935
// browse/tv/*_subscriptions.json) or, in other layouts, as the browse ID itself.
export function tabChannel(tab) {
  const endpoint = tab?.endpoint?.browseEndpoint;
  if (channelPattern.test(endpoint?.browseId ?? '')) return endpoint.browseId;
  if (typeof endpoint?.params !== 'string' || endpoint.params.length > 1024) return '';
  return /UC[A-Za-z0-9_-]{22}/.exec(decoded(endpoint.params))?.[0] ?? '';
}

export class TvAccountCatalog extends TopicCatalog {
  constructor(ErrorType, video, image, references, clock) {
    super(ErrorType, video, image, clock);
    this.references = references;
  }
  folder(owner, section, client, title, image, source) {
    // A shelf's cards are already on the page (feed below). Only a named Home shelf
    // keeps a folder of its own; Subscriptions, History and untitled shelves would
    // only add "More videos" tiles repeating cards, page after page.
    if (source.row && (section !== 'home' || !String(title ?? '').trim())) return null;
    const browse = source.endpoint?.browseId;
    if (/^VL(?:WL|LL|[A-Za-z0-9_-]{10,100})$/.test(browse ?? ''))
      return { id: 'library:playlist:' + browse.slice(2), title: String(title ?? '').slice(0,192),
        art_url: this.imageUrl(image), is_folder: true };
    return this.references.register(owner, String(title ?? '').slice(0,192) || 'More videos', 'YouTube', async () =>
      this.feed(owner, section, client, source.row ?? await this.request(client, source.endpoint), source.root === true),
      section === 'home' ? 'home' : '');
  }
  tile(owner, section, client, tile) {
    const item = super.tile(owner, section, client, tile);
    const endpoint = tile?.onSelectCommand?.watchEndpoint;
    const playlist = section.startsWith('playlist:') ? section.slice(9) : '';
    const index = endpoint?.index ?? endpoint?.playlistIndex;
    if (item && playlist && endpoint?.playlistId === playlist && endpoint.videoId === item.id &&
        (endpoint.index === undefined || endpoint.playlistIndex === undefined || endpoint.index === endpoint.playlistIndex) &&
        Number.isInteger(index) && index >= 0 && index <= 100000)
      item.playback_context = { playlist, index };
    return item;
  }
  feed(owner, section, client, raw, root = true, continued = false) {
    const rows = (find(raw, 'sectionListRenderer') ?? find(raw, 'sectionListContinuation'))?.contents ?? [];
    root = root && rows.some(row => row?.shelfRenderer);
    const result = super.feed(owner, section, client, raw, root, continued);
    if (root) {
      // Video cards of every grid and shelf, in page order (the TV Subscriptions page
      // opens with a grid before its shelves); a named Home shelf also keeps a folder.
      const cards = [], seen = new Set();
      for (const value of rows) {
        const content = value?.gridRenderer ? { gridRenderer: value.gridRenderer } :
          value?.shelfRenderer?.content?.horizontalListRenderer || value?.shelfRenderer?.content?.gridRenderer ? value.shelfRenderer.content : null;
        if (!content) continue;
        const row = super.feed(owner, section, client, content, false);
        for (const item of row.catalog_items) if (!seen.has(item.id)) {
          seen.add(item.id); cards.push(item);
          if (cards.length > 4096) throw new this.Error('upstream_limit', 'The account feed exceeds the item limit.', 502);
        }
      }
      result.catalog_items = [...cards, ...result.catalog_items];
    }
    return result;
  }

  // Home's first usable group is the primary preview. Later groups stay behind
  // their own navigation cards instead of becoming unrelated rows in that grid.
  // Keep row continuations even when today's filtered page has no visible cards.
  homeFeed(owner, client, raw, flags = null, continued = false) {
    const hidden = hides(flags, 'home');
    const hideWatched = !flags || flags.has('watched_home');
    const visible = item => !hidden?.(item);
    const primaryVisible = item => visible(item) && (!hideWatched || !isWatched(item));
    const node = find(raw, 'sectionListRenderer') ?? find(raw, 'sectionListContinuation');
    if (!node) {
      const filter = feed => ({ ...feed,
        catalog_items: feed.catalog_items.filter(primaryVisible),
        ...(feed.has_continuation ? { getContinuation: async () => filter(await feed.getContinuation()) } : {}) });
      return filter(super.feed(owner, 'home', client, raw, false, continued));
    }
    const rows = node.contents;
    if (!Array.isArray(rows) || rows.length > 128)
      throw new this.Error('upstream_limit', 'Too many Home groups.', 502);
    const previews = [], folders = [];
    let count = 0;
    let primary = continued; // later pages must not promote category shelves into the primary grid
    for (const value of rows) {
      const shelf = value?.shelfRenderer;
      const content = value?.gridRenderer ? { gridRenderer: value.gridRenderer } :
        shelf?.content?.horizontalListRenderer || shelf?.content?.gridRenderer ? shelf.content : null;
      if (!content) continue; // ad slots and unsupported non-card layouts are never expanded
      const row = super.feed(owner, 'home', client, content, false);
      count += row.catalog_items.length;
      if (count > 4096)
        throw new this.Error('upstream_limit', 'The account feed exceeds the item limit.', 502);
      const items = row.catalog_items.filter(visible);
      const first = !primary && row.catalog_items.length > 0;
      if (first) {
        primary = true;
        previews.push(...row.catalog_items.filter(primaryVisible));
      }
      if (!items.length && !row.has_continuation) continue;
      const title = label(find(shelf?.headerRenderer, 'avatarLockupRenderer')?.title ?? shelf?.title) || 'More videos';
      // Retain the primary shelf too: its horizontal continuation may be
      // independent of the Home page's continuation. Empty final shelves vanish.
      folders.push(this.references.register(owner, title, 'YouTube', async () => row, 'home'));
    }
    const directContinuation = rows.find(row => row?.continuationItemRenderer)?.continuationItemRenderer;
    const candidate = node.continuations?.[0]?.nextContinuationData?.continuation ??
      directContinuation?.continuationEndpoint?.continuationCommand?.token;
    const token = typeof candidate === 'string' && candidate.length <= 16384 ? candidate : '';
    return { catalog_items: [...previews, ...folders], has_continuation: !!token,
      ...(token ? { getContinuation: async () =>
        this.homeFeed(owner, client, await this.request(client, { continuation: token }), flags, true) } : {}) };
  }

  // Subscribed channels of a TV sign-in: the channel tabs beside "All" on the
  // TV Subscriptions page. The web channel list does not answer a TV sign-in.
  async channels(client) {
    const raw = await this.request(client, { browseId: 'FEsubscriptions' });
    const tabs = (find(raw, 'tvSecondaryNavRenderer')?.sections ?? [])
      .flatMap(section => section?.tvSecondaryNavSectionRenderer?.tabs ?? []);
    if (tabs.length > 8192) throw new this.Error('upstream_limit', 'The account list exceeds the item limit.', 502);
    const items = [], seen = new Set();
    for (const value of tabs) {
      const tab = value?.tabRenderer, id = tabChannel(tab);
      if (!id || seen.has(id)) continue;
      seen.add(id);
      const thumbnails = tab.thumbnail?.thumbnails ?? [];
      let art = String(thumbnails[thumbnails.length - 1]?.url ?? '');
      if (art.startsWith('//')) art = 'https:' + art;
      items.push({ kind: 'channel', id: `channel:${id}`, channel_id: id, title: label(tab.title) || id, subtitle: '',
        art_url: this.imageUrl(art), is_folder: true });
    }
    return { tabs: tabs.length, feed: { catalog_items: items, has_continuation: false } };
  }
  async browse(owner, action, client, playlist = '', flags = null) {
    // MediaServiceCore TV Home uses default; the remaining IDs match the pinned account operations.
    const ids = { home: 'default', history: 'FEhistory', library: 'FElibrary', playlists: 'FEplaylist_aggregation',
      subscriptions: 'FEsubscriptions', watch_later: 'VLWL', playlist: 'VL' + playlist };
    const raw = await this.request(client, { browseId: ids[action] });
    const tiles = find(raw, 'tileRenderer', true).length;
    if (!tiles) {
      console.error(`YouTube browse shape: action=${action} tiles=0 mode=legacy`);
      const Feed = action === 'home' ? YT.HomeFeed : action === 'library' ? YT.Library :
        ['playlist', 'watch_later'].includes(action) ? YT.Playlist : YT.History;
      return new Feed(client.actions, { data: raw });
    }
    const section = action === 'watch_later' ? 'playlist:WL' : action === 'playlist' ? 'playlist:' + playlist : action;
    const feed = action === 'home' ? this.homeFeed(owner, client, raw, flags) : this.feed(owner, section, client, raw);
    console.error(`YouTube browse shape: action=${action} tiles=${tiles} mode=tv items=${feed.catalog_items.length}`);
    if (!feed.catalog_items.length && action !== 'home')
      throw new this.Error('upstream_format', 'YouTube returned account cards that could not be read.', 502);
    return feed;
  }
}
