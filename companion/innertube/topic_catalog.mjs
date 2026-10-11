// TV browse shelves and bounded, profile-owned navigation capabilities.
// SPDX-License-Identifier: GPL-3.0-or-later
import { randomUUID } from 'node:crypto';

const sections = new Set(['music', 'gaming', 'news', 'sports', 'live', 'movies']);
// MediaServiceCore 4d9767733aa9 BrowseApiHelper.getLikedMusicContinuation.
const likedMusicContinuation = '4qmFsgIWEhRGRW11c2ljX2xpa2VkX3ZpZGVvcw%3D%3D';
const text = value => typeof value === 'string' ? value.slice(0, 192) :
  String(value?.simpleText ?? value?.runs?.map(run => run.text ?? '').join('') ?? '').replaceAll('\0', '').slice(0, 192);
function find(node, name, all = false) {
  const matches = [], stack = [[node, 0]];
  let remaining = 100000;
  while (stack.length) {
    const [current, depth] = stack.pop();
    if (!current || typeof current !== 'object') continue;
    if (--remaining < 0 || depth > 40) throw new Error('TV browse structure exceeds limits');
    if (Object.hasOwn(current, name)) {
      matches.push(current[name]);
      if (!all) return current[name];
      continue;
    }
    for (const value of Object.values(current).reverse()) stack.push([value, depth + 1]);
  }
  return all ? matches : null;
}
function continuation(node) {
  const value = node?.continuations?.[0]?.nextContinuationData?.continuation ??
    find(node, 'continuationItemRenderer')?.continuationEndpoint?.continuationCommand?.token;
  return typeof value === 'string' && value.length <= 16384 ? value : '';
}

export class TopicCatalog {
  constructor(ErrorType, normalizeVideo, imageUrl, clock = Date.now) {
    this.Error = ErrorType; this.normalizeVideo = normalizeVideo; this.imageUrl = imageUrl;
    this.clock = clock; this.entries = new Map();
  }
  prune() {
    for (const [key, value] of this.entries) if (value.deadline <= this.clock()) this.entries.delete(key);
  }
  folder(owner, section, client, title, image, source) {
    this.prune();
    const bytes = Buffer.byteLength(JSON.stringify(source));
    if (bytes > 2 * 1024 * 1024) throw new this.Error('upstream_limit', 'The section row exceeds the size limit.', 502);
    let retained = Array.from(this.entries.values()).reduce((n, value) => n + value.bytes, 0);
    while (this.entries.size && (this.entries.size >= 128 || retained + bytes > 8 * 1024 * 1024)) {
      const key = this.entries.keys().next().value;
      retained -= this.entries.get(key).bytes; this.entries.delete(key);
    }
    const key = randomUUID();
    this.entries.set(key, { owner, section, client, source, bytes, deadline: this.clock() + 600000 });
    return { id: `section:${section}:${key}`, title: text(title) || 'More videos',
      subtitle: 'YouTube', art_url: this.imageUrl(image), is_folder: true };
  }
  async request(client, payload, clientType = 'TV') {
    const response = await client.actions.execute('/browse', { ...payload, client: clientType });
    if (!response?.success || response.status_code >= 400 || !response.data)
      throw new this.Error('upstream_failure', 'YouTube did not return this section.', 502);
    return response.data;
  }
  async root(owner, section, client, authenticated = false) {
    if (!sections.has(section)) throw new this.Error('invalid_catalog', 'Choose an available section.');
    const feed = this.feed(owner, section, client, await this.request(client, { browseId: `FEtopics_${section}` }), true);
    if (section === 'music' && authenticated) {
      const liked = await this.likedMusic(owner, client);
      if (liked) feed.catalog_items.unshift(liked);
    }
    return feed;
  }
  async likedMusic(owner, client) {
    // Match SmartTube's TV row first, then the WEB VLLM fallback. A missing
    // personalized row must not prevent the public Music shelves from loading.
    for (const [clientType, payload] of [['TV', { continuation: likedMusicContinuation }],
      ['WEB', { browseId: 'VLLM' }]]) {
      try {
        const raw = await this.request(client, payload, clientType);
        const feed = this.feed(owner, 'music', client, raw, false, false, clientType);
        if (!feed.catalog_items.length && !feed.has_continuation) return null;
        return this.folder(owner, 'music', client, 'Liked music', feed.catalog_items[0]?.art_url,
          { row: raw, clientType });
      } catch { /* Optional account row; try the reference fallback. */ }
    }
    return null;
  }
  async open(owner, section, key) {
    this.prune();
    const entry = this.entries.get(key);
    if (!entry || entry.owner !== owner || entry.section !== section)
      throw new this.Error('cursor_expired', 'Reload this section to open the row.', 409);
    if (entry.feed) return entry.feed;
    const clientType = entry.source.clientType ?? 'TV';
    const raw = entry.source.row ?? await this.request(entry.client, entry.source.endpoint, clientType);
    entry.feed = this.feed(owner, section, entry.client, raw, entry.source.root === true, false, clientType);
    return entry.feed;
  }
  tile(owner, section, client, tile) {
    if (!tile || tile.isAd || tile.adSlotMetadata) return null;
    const metadata = tile.metadata?.tileMetadataRenderer ?? find(tile.onFocusCommand, 'entityMetadataRenderer');
    const title = text(metadata?.title);
    const header = tile.header?.tileHeaderRenderer;
    const image = header?.thumbnail?.thumbnails?.[0]?.url;
    const endpoint = tile.onSelectCommand?.browseEndpoint;
    if (endpoint && (/^(VL[A-Za-z0-9_-]{10,100}|UC[A-Za-z0-9_-]{22})$/.test(endpoint.browseId ?? '') ||
        (section === 'movies' && endpoint.browseId === 'FEstorefront')) &&
        (!endpoint.params || (typeof endpoint.params === 'string' && endpoint.params.length <= 4096)))
      return this.folder(owner, section, client, title, image,
        { endpoint: { browseId: endpoint.browseId, ...(endpoint.params ? { params: endpoint.params } : {}) },
          root: endpoint.browseId === 'FEstorefront' || endpoint.browseId.startsWith('UC') });
    if (tile.contentType !== 'TILE_CONTENT_TYPE_VIDEO') return null;
    const id = tile.onSelectCommand?.watchEndpoint?.videoId ?? tile.contentId;
    const lines = metadata?.lines ?? metadata?.bylines ?? [];
    const byline = lines[0]?.lineRenderer?.items ?? [];
    const author = byline.map(part => text(part.lineItemRenderer?.text)).filter(Boolean).join(' ');
    const channel = find(lines[0], 'browseEndpoint')?.browseId;
    const overlays = header?.thumbnailOverlays ?? [];
    const status = overlays.map(value => value.thumbnailOverlayTimeStatusRenderer).find(Boolean);
    const durationText = text(status?.text);
    const parts = /^\d{1,3}:\d{2}(?::\d{2})?$/.test(durationText) ? durationText.split(':').map(Number) : [];
    const duration = parts.length && parts.slice(1).every(value => value < 60) ? parts.reduce((n, v) => n * 60 + v, 0) : 0;
    return this.normalizeVideo({ id, title, author: { name: author, id: channel }, duration,
      thumbnails: [{ url: image }], ...this.videoState(tile,overlays) });
  }
  videoState(video, overlays) {
    const status=overlays.map(value=>value.thumbnailOverlayTimeStatusRenderer).find(Boolean);
    const rawStart=video.upcomingEventData?.startTime;
    const start=(typeof rawStart==='number' || (typeof rawStart==='string' && /^\d+$/.test(rawStart)))?Number(rawStart):NaN;
    const upcoming=new Date(start*1000);
    const validStart=Number.isFinite(upcoming.getTime()) && upcoming.getTime()>0;
    return {is_live:status?.style==='LIVE' || (video.badges ?? []).some(value=>value.metadataBadgeRenderer?.style==='BADGE_STYLE_TYPE_LIVE_NOW'),
      is_upcoming:validStart && upcoming.getTime()>this.clock(),
      upcoming:validStart?upcoming:undefined,
      is_watched:video.isWatched===true,
      thumbnail_overlays:overlays.filter(value=>value.thumbnailOverlayResumePlaybackRenderer).map(value=>({
        type:'ThumbnailOverlayResumePlayback',percent_duration_watched:value.thumbnailOverlayResumePlaybackRenderer.percentDurationWatched}))};
  }
  webVideo(video) {
    if (!video || video.isPlayable === false || video.isAd || video.adSlotMetadata) return null;
    const author = video.shortBylineText ?? video.longBylineText;
    const durationText = text(video.lengthText);
    const parts = /^\d{1,3}:\d{2}(?::\d{2})?$/.test(durationText) ? durationText.split(':').map(Number) : [];
    const duration = parts.length && parts.slice(1).every(value => value < 60) ? parts.reduce((n, v) => n * 60 + v, 0) : 0;
    return this.normalizeVideo({ id: video.videoId, title: text(video.title),
      author: { name: text(author), id: find(author, 'browseEndpoint')?.browseId }, duration,
      thumbnails: video.thumbnail?.thumbnails ?? [], ...this.videoState(video,video.thumbnailOverlays ?? []) });
  }
  feed(owner, section, client, raw, root, continuationResponse = false, clientType = 'TV') {
    let node = root ? find(raw, 'sectionListRenderer') ?? find(raw, 'sectionListContinuation') :
      find(raw, 'horizontalListRenderer') ?? find(raw, 'horizontalListContinuation') ??
      find(raw, 'playlistVideoListRenderer') ?? find(raw, 'playlistVideoListContinuation') ??
      find(raw, 'gridRenderer') ?? find(raw, 'gridContinuation');
    if (!node && clientType === 'WEB' && continuationResponse) {
      const update = find(raw, 'appendContinuationItemsAction') ?? find(raw, 'reloadContinuationItemsCommand');
      if (update) node = { items: update.continuationItems };
    }
    if (!node) {
      const promo = root && find(raw, 'genericPromoRenderer');
      if (promo?.bodyText)
        throw new this.Error('catalog_unavailable', text(promo.bodyText) || 'This section has no available videos.', 404);
      // TV sometimes returns a final tracking-only response for an exhausted token.
      if (continuationResponse && Object.keys(raw).every(key => ['responseContext', 'trackingParams'].includes(key)))
        return { catalog_items: [], has_continuation: false };
      throw new this.Error('upstream_format', 'YouTube returned an unsupported section layout.', 502);
    }
    let items;
    if (root) {
      const shelves = (node.contents ?? []).map(value => value.shelfRenderer).filter(Boolean);
      if (shelves.length > 128) throw new this.Error('upstream_limit', 'Too many section rows.', 502);
      items = shelves.map(shelf => {
        const row = shelf.content?.horizontalListRenderer;
        if (!row) return null;
        const title = find(shelf.headerRenderer, 'avatarLockupRenderer')?.title ?? shelf.title;
        const first = find(row, 'tileRenderer');
        return this.folder(owner, section, client, text(title), first?.header?.tileHeaderRenderer?.thumbnail?.thumbnails?.[0]?.url,
          { row: { horizontalListRenderer: row } });
      }).filter(Boolean);
    } else {
      const entries = node.items ?? node.contents;
      if (!Array.isArray(entries) || entries.length > 4096)
        throw new this.Error('upstream_limit', 'The section row exceeds the item limit.', 502);
      const seen = new Set();
      items = entries.map(value => clientType === 'WEB' ? this.webVideo(value.playlistVideoRenderer) :
        this.tile(owner, section, client, value.tileRenderer)).filter(item => {
        if (!item || seen.has(item.id)) return false;
        seen.add(item.id); return true;
      });
    }
    const token = continuation(node);
    return { catalog_items: items, has_continuation: !!token,
      getContinuation: async () => {
        if (!token) throw new this.Error('cursor_expired', 'This row has ended.', 409);
        return this.feed(owner, section, client, await this.request(client, { continuation: token }, clientType), root, true, clientType);
      } };
  }
}
