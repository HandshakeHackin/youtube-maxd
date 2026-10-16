// OAuth TV browse cards use the same bounded raw tile adapter as TV topics.
// SPDX-License-Identifier: GPL-3.0-or-later
import { YT } from 'youtubei.js';
import { TopicCatalog, find } from './topic_catalog.mjs';

export class TvAccountCatalog extends TopicCatalog {
  constructor(ErrorType, video, image, references, clock) {
    super(ErrorType, video, image, clock);
    this.references = references;
  }
  folder(owner, section, client, title, image, source) {
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
    const shelves = (find(raw, 'sectionListRenderer') ?? find(raw, 'sectionListContinuation'))?.contents?.filter(row => row.shelfRenderer) ?? [];
    root = root && shelves.length > 0;
    const result = super.feed(owner, section, client, raw, root, continued);
    if (root) {
      // Home needs video cards; each shelf also keeps its own continuation through a retained folder.
      const cards = [], seen = new Set();
      for (const { shelfRenderer: shelf } of shelves) {
        if (!shelf.content?.horizontalListRenderer) continue;
        const row = super.feed(owner, section, client, shelf.content, false);
        for (const item of row.catalog_items) if (!seen.has(item.id)) {
          seen.add(item.id); cards.push(item);
          if (cards.length > 4096) throw new this.Error('upstream_limit', 'The account feed exceeds the item limit.', 502);
        }
      }
      result.catalog_items = [...cards, ...result.catalog_items];
    }
    return result;
  }
  async browse(owner, action, client, playlist = '') {
    // MediaServiceCore TV Home uses default; the remaining IDs match the pinned account operations.
    const ids = { home: 'default', history: 'FEhistory', library: 'FElibrary', playlists: 'FEplaylist_aggregation',
      subscriptions: 'FEsubscriptions', watch_later: 'VLWL', playlist: 'VL' + playlist };
    const raw = await this.request(client, { browseId: ids[action] });
    const tiles = find(raw, 'tileRenderer', true).length;
    console.error(`YouTube browse shape: action=${action} tiles=${tiles} mode=${tiles ? 'tv' : 'legacy'}`);
    if (!tiles) {
      const Feed = action === 'home' ? YT.HomeFeed : action === 'library' ? YT.Library :
        ['playlist', 'watch_later'].includes(action) ? YT.Playlist : YT.History;
      return new Feed(client.actions, { data: raw });
    }
    const section = action === 'watch_later' ? 'playlist:WL' : action === 'playlist' ? 'playlist:' + playlist : action;
    const feed = this.feed(owner, section, client, raw);
    if (!feed.catalog_items.length)
      throw new this.Error('upstream_format', 'YouTube returned account cards that could not be read.', 502);
    return feed;
  }
}
