// Original bounded service adapter. SPDX-License-Identifier: GPL-3.0-or-later
import { randomUUID } from 'node:crypto';

export class LibraryError extends Error {
  constructor(code, message, status = 400) { super(message); this.code = code; this.status = status; }
}
const text = (value, limit = 192) => String(value ?? '').replaceAll('\0', '').slice(0, limit);
const videoId = value => /^[A-Za-z0-9_-]{11}$/.test(value ?? '');
const channelId = value => /^UC[A-Za-z0-9_-]{22}$/.test(value ?? '');
const playlistId = value => /^(WL|LL|[A-Za-z0-9_-]{10,100})$/.test(value ?? '');
const requireId = (value, check) => {
  if (!check(value)) throw new LibraryError('invalid_identity', 'The selected item has an invalid identity.');
  return value;
};
export function imageUrl(value) {
  try {
    const url = new URL(value);
    const domains = ['ytimg.com', 'ggpht.com', 'googleusercontent.com'];
    if (url.protocol === 'https:' && !url.username && !url.password && !url.port &&
        domains.some(host => url.hostname === host || url.hostname.endsWith(`.${host}`))) return url.href;
  } catch { /* Missing or unsupported artwork becomes an empty image. */ }
  return '';
}
function cardImage(node) {
  return node.content_image?.primary_thumbnail ?? node.content_image;
}
function cardAuthor(node) {
  if (node.author) return node.author;
  const parts = node.metadata?.metadata?.metadata_rows?.[0]?.metadata_parts ?? [];
  for (const part of parts) {
    const value = part.text;
    const id = value?.endpoint?.payload?.browseId;
    if (channelId(id)) return { name: value, id };
  }
  return { name: parts[0]?.text, id: '' };
}
function cardDuration(node) {
  const direct = Number(node.duration?.seconds ?? node.duration ?? NaN);
  if (Number.isFinite(direct) && direct >= 0) return Math.floor(direct);
  for (const overlay of cardImage(node)?.overlays ?? []) {
    for (const badge of overlay.badges ?? []) {
      const value = String(badge.text ?? '');
      if (/^\d{1,3}:\d{2}(?::\d{2})?$/.test(value)) {
        const parts = value.split(':').map(Number);
        if (parts.slice(1).every(part => part < 60))
          return parts.reduce((total, part) => total * 60 + part, 0);
      }
    }
  }
  return 0;
}
export function normalizeVideo(node) {
  if (!node || node.is_ad || node.is_promoted) return null;
  const id = node.video_id ?? node.id ?? node.content_id;
  if (!videoId(id)) return null;
  if (node.content_type && !['VIDEO', 'MOVIE', 'SHORT', 'STATION'].includes(node.content_type)) return null;
  const author = cardAuthor(node);
  return { id, title: text(node.title ?? node.metadata?.title), subtitle: text(author.name),
    overview: text(node.description_snippet, 1024),
    art_url: imageUrl(node.thumbnails?.[0]?.url ?? cardImage(node)?.image?.[0]?.url),
    channel_id: channelId(author.id) ? author.id : '',
    duration_sec: cardDuration(node),
    is_folder: false, is_live: !!node.is_live, played: !!node.is_watched };
}
function normalizePlaylist(node) {
  if (!node || node.is_ad || node.is_promoted) return null;
  const id = node.id ?? node.playlist_id ?? node.content_id;
  if (!playlistId(id)) return null;
  return { id: `library:playlist:${id}`, title: text(node.title ?? node.metadata?.title), subtitle: text(cardAuthor(node).name),
    art_url: imageUrl(node.thumbnails?.[0]?.url ?? cardImage(node)?.image?.[0]?.url), is_folder: true };
}
function normalizeComment(thread) {
  const node = thread.comment ?? thread;
  if (!node.comment_id) return null;
  return { id: text(node.comment_id, 160), author: text(node.author?.name),
    content: text(node.content, 4096), published: text(node.published_time),
    likes: text(node.like_count, 32), replies: text(node.reply_count, 32),
    pinned: !!node.is_pinned, owner: !!node.author_is_channel_owner,
    art_url: imageUrl(node.author?.thumbnails?.[0]?.url) };
}

export class Library {
  constructor(createClient, profiles = [], clock = () => Date.now()) {
    if (!Array.isArray(profiles) || profiles.length > 8) throw new LibraryError('invalid_profiles', 'At most eight profiles are supported.');
    this.createClient = createClient; this.clock = clock;
    this.profiles = new Map([['guest', { id: 'guest', name: 'Guest', cookie: '' }]]);
    for (const profile of profiles) {
      if (!/^[A-Za-z0-9_-]{1,32}$/.test(profile.id ?? '') || this.profiles.has(profile.id) ||
          typeof profile.cookie !== 'string' || profile.cookie.length > 65536 || /[\r\n]/.test(profile.cookie) ||
          (profile.account_index !== undefined && (!Number.isInteger(profile.account_index) || profile.account_index < 0 || profile.account_index > 7)) ||
          (profile.channel !== undefined && !/^[A-Za-z0-9_-]{1,100}$/.test(profile.channel)))
        throw new LibraryError('invalid_profiles', 'The profile configuration is invalid.');
      this.profiles.set(profile.id, { ...profile, name: text(profile.name) });
    }
    this.clients = new Map(); this.cursors = new Map(); this.threads = new Map();
  }
  async client(id) {
    const profile = this.profiles.get(id);
    if (!profile) throw new LibraryError('unknown_profile', 'Select an available profile.', 404);
    if (!this.clients.has(id)) this.clients.set(id, await this.createClient(profile));
    return this.clients.get(id);
  }
  page(owner, kind, feed, pending = null) {
    const comments = kind === 'comments';
    const items = pending ?? (comments ? Array.from(feed.contents ?? feed.replies ?? []).filter(thread => normalizeComment(thread)) :
      [...Array.from(feed.videos ?? []).map(normalizeVideo), ...Array.from(feed.playlists ?? []).map(normalizePlaylist)].filter(Boolean));
    if (items.length > 4096) throw new LibraryError('upstream_limit', 'The returned page exceeds the item limit.', 502);
    for (const [key, value] of this.threads) if (value.deadline <= this.clock()) this.threads.delete(key);
    const output = items.slice(0, 24).map(thread => {
      if (!comments) return thread;
      const item = normalizeComment(thread);
      if (thread.has_replies && typeof thread.getReplies === 'function') {
        if (this.threads.size >= 256) this.threads.delete(this.threads.keys().next().value);
        item.reply_cursor = randomUUID();
        this.threads.set(item.reply_cursor, { owner, thread, deadline: this.clock() + 120000 });
      } else item.reply_cursor = '';
      return item;
    });
    const remaining = items.slice(24);
    let cursor = '';
    for (const [key, value] of this.cursors) if (value.deadline <= this.clock()) this.cursors.delete(key);
    if (remaining.length || feed.has_continuation) {
      if (this.cursors.size >= 64) this.cursors.delete(this.cursors.keys().next().value);
      cursor = randomUUID();
      this.cursors.set(cursor, { owner, kind, feed, remaining, deadline: this.clock() + 120000 });
    }
    return { api_version: 1, kind, items: output, cursor, has_more: !!cursor };
  }
  async execute(input) {
    if (!input || typeof input !== 'object') throw new LibraryError('invalid_request', 'Expected an operation.');
    const owner = input.profile ?? 'guest';
    if (input.action === 'profiles') return { api_version: 1, profiles: Array.from(this.profiles.values(), p =>
      ({ id: p.id, name: p.name, account_configured: !!p.cookie })) };
    if (input.action === 'replies') {
      const entry = this.threads.get(input.cursor);
      if (!entry || entry.owner !== owner || entry.deadline <= this.clock())
        throw new LibraryError('cursor_expired', 'Reload the comments to view replies.', 409);
      if (entry.result) return entry.result;
      const feed = await entry.thread.getReplies();
      if (!Array.isArray(feed?.replies))
        throw new LibraryError('upstream_failure', 'YouTube did not return a reply list.', 502);
      entry.result = this.page(owner, 'comments', feed);
      return entry.result;
    }
    if (input.action === 'continue') {
      const entry = this.cursors.get(input.cursor);
      if (!entry || entry.owner !== owner || entry.deadline <= this.clock())
        throw new LibraryError('cursor_expired', 'Reload this page to continue.', 409);
      if (entry.result) return entry.result;
      const feed = entry.remaining.length ? entry.feed : await entry.feed.getContinuation();
      const result = this.page(owner, entry.kind, feed, entry.remaining.length ? entry.remaining : null);
      entry.result = result;
      return result;
    }
    const profile = this.profiles.get(owner);
    if (!profile) throw new LibraryError('unknown_profile', 'Select an available profile.', 404);
    const authenticated = !!profile.cookie;
    const privateActions = new Set(['subscriptions', 'history', 'library', 'playlists', 'watch_later',
      'like', 'dislike', 'unrate', 'subscribe', 'unsubscribe', 'playlist_create', 'playlist_add',
      'playlist_remove', 'playlist_rename', 'playlist_delete', 'watch_later_add', 'watch_later_remove']);
    if (privateActions.has(input.action) && !authenticated)
      throw new LibraryError('account_required', 'Configure an account profile on the companion first.', 401);
    if (!privateActions.has(input.action) && !['home', 'playlist', 'comments'].includes(input.action))
      throw new LibraryError('unknown_operation', 'This library operation is unavailable.');
    if (['like', 'dislike', 'unrate', 'comments', 'playlist_add', 'playlist_remove', 'watch_later_add', 'watch_later_remove'].includes(input.action))
      requireId(input.id, videoId);
    if (['subscribe', 'unsubscribe'].includes(input.action)) requireId(input.id, channelId);
    if (input.action === 'playlist') requireId(input.id, playlistId);
    if (['playlist_add', 'playlist_remove', 'playlist_rename', 'playlist_delete'].includes(input.action))
      requireId(input.playlist, playlistId);
    const client = await this.client(owner);
    let feed;
    switch (input.action) {
      case 'home': feed = await client.getHomeFeed(); break;
      case 'subscriptions': feed = await client.getSubscriptionsFeed(); break;
      case 'history': feed = await client.getHistory(); break;
      case 'library': feed = await client.getLibrary(); break;
      case 'playlists': feed = await client.getPlaylists(); break;
      case 'watch_later': feed = await client.getPlaylist('WL'); break;
      case 'playlist': feed = await client.getPlaylist(requireId(input.id, playlistId)); break;
      case 'comments':
        if (!['top', 'newest', undefined].includes(input.sort)) throw new LibraryError('invalid_sort', 'Select top or newest comments.');
        feed = await client.getComments(requireId(input.id, videoId), input.sort === 'newest' ? 'NEWEST_FIRST' : 'TOP_COMMENTS');
        return this.page(owner, 'comments', feed);
      default: break;
    }
    if (feed) return this.page(owner, 'catalog', feed);
    let response;
    switch (input.action) {
      case 'like': response = await client.interact.like(requireId(input.id, videoId)); break;
      case 'dislike': response = await client.interact.dislike(requireId(input.id, videoId)); break;
      case 'unrate': response = await client.interact.removeRating(requireId(input.id, videoId)); break;
      case 'subscribe': response = await client.interact.subscribe(requireId(input.id, channelId)); break;
      case 'unsubscribe': response = await client.interact.unsubscribe(requireId(input.id, channelId)); break;
      case 'playlist_create': {
        const title = text(input.title, 150).trim();
        if (!title) throw new LibraryError('invalid_title', 'Enter a playlist title.');
        response = await client.playlist.create(title, input.id ? [requireId(input.id, videoId)] : []); break;
      }
      case 'playlist_rename': {
        const title = text(input.title, 150).trim();
        if (!title) throw new LibraryError('invalid_title', 'Enter a playlist title.');
        response = await client.playlist.setName(requireId(input.playlist, playlistId), title); break;
      }
      case 'playlist_delete': response = await client.playlist.delete(requireId(input.playlist, playlistId)); break;
      case 'playlist_add': response = await client.playlist.addVideos(requireId(input.playlist, playlistId), [requireId(input.id, videoId)]); break;
      case 'playlist_remove': response = await client.playlist.removeVideos(requireId(input.playlist, playlistId), [requireId(input.id, videoId)]); break;
      case 'watch_later_add': response = await client.playlist.addVideos('WL', [requireId(input.id, videoId)]); break;
      case 'watch_later_remove': response = await client.playlist.removeVideos('WL', [requireId(input.id, videoId)]); break;
      default: throw new LibraryError('unknown_operation', 'This library operation is unavailable.');
    }
    if (!response || response.success === false || response.status_code >= 400 ||
        ('action_result' in response && (!Array.isArray(response.action_result) || !response.action_result.length)))
      throw new LibraryError('operation_failed', 'YouTube did not confirm the requested change.', 502);
    return { api_version: 1, success: true, playlist_id: playlistId(response.playlist_id) ? response.playlist_id : '' };
  }
}
