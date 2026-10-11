// Original bounded service adapter. SPDX-License-Identifier: GPL-3.0-or-later
import { randomUUID } from 'node:crypto';
import { validCredentials } from './device_auth.mjs';
import { ChatReader } from './live_chat.mjs';
import { TopicCatalog } from './topic_catalog.mjs';
import { remotePlaylist } from './remote_playlist.mjs';
import { playbackNext } from './playback_next.mjs';
import { channelWindow, channelPage, channelHome, mappedFeed } from './channel_window.mjs';
import { PlaybackGroups } from './playback_groups.mjs';
import { HomeCatalog, watchPercent } from './home_feed.mjs';
import { HistoryActions } from './history_actions.mjs';
import { SuggestionFilters } from './suggestions.mjs';
import { remoteRecommendationGroup } from './remote_recommendations.mjs';
import { normalizeChannel, searchFeed, searchFilters, searchQuery, searchSuggestions } from './search_catalog.mjs';
import { videoDescription, videoDetails } from './video_description.mjs';
import { KidsCatalog } from './kids.mjs';
import { StreamReminders } from './stream_reminder.mjs';
import { PlaylistOrders } from './playlist_order.mjs';
import { CommentPosts, commentHeader, commentState } from './comment_posts.mjs';
import { threadReplies } from './comment_replies.mjs';
import { channelMarks } from './channel_marks.mjs';
import { accepted } from './intents.mjs';
import { channelsFeed, notificationsFeed } from './account_sections.mjs';
import { FeedbackActions } from './feedback_actions.mjs';
import { requestLocale } from './locale.mjs';
import { hideSet, hideSection, hides, isShort, shortsCard } from './content_filters.mjs';
import { AccountActions, historyPaused } from './account_actions.mjs';

export class LibraryError extends Error {
  constructor(code, message, status = 400) { super(message); this.code = code; this.status = status; }
}
const text = (value, limit = 192) => String(value ?? '').replaceAll('\0', '').slice(0, limit);
const playlistTitle = value => {
  const title = String(value ?? '').trim();
  if (!title || Array.from(title).length > 150 || /[\0-\x1f\x7f]/.test(title))
    throw new LibraryError('invalid_title', 'Enter a playlist title of 1 to 150 characters.');
  return title;
};
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
function cardLive(node) {
  if (node.is_live) return true;
  if((node.thumbnail_overlays ?? []).some(overlay=>overlay?.type==='ThumbnailOverlayTimeStatus' && overlay.style==='LIVE'))return true;
  return (cardImage(node)?.overlays ?? []).some(overlay =>
    ['ThumbnailBottomOverlayView', 'ThumbnailOverlayBadgeView'].includes(overlay.type) &&
    (overlay.badges ?? []).some(badge => badge.type === 'ThumbnailBadgeView' &&
      badge.badge_style === 'THUMBNAIL_OVERLAY_BADGE_STYLE_LIVE'));
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
  const durationText=String(node.duration ?? '');
  if(/^\d{1,3}:\d{2}(?::\d{2})?$/.test(durationText)) {
    const parts=durationText.split(':').map(Number);
    if(parts.slice(1).every(part=>part<60))return parts.reduce((total,part)=>total*60+part,0);
  }
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
function playbackContext(node, id, playlist) {
  if (typeof playlist !== 'string' || !playlistId(playlist)) return null;
  const endpoint = (node.endpoint ?? node.renderer_context?.command_context?.on_tap)?.payload;
  if (endpoint?.videoId !== id || endpoint?.playlistId !== playlist) return null;
  if (endpoint.index !== undefined && endpoint.playlistIndex !== undefined && endpoint.index !== endpoint.playlistIndex) return null;
  let index = endpoint.index !== undefined ? endpoint.index : endpoint.playlistIndex;
  if (index === undefined) {
    const displayed = String(node.index ?? '');
    if (!/^[1-9][0-9]{0,5}$/.test(displayed)) return null;
    index = Number(displayed) - 1;
  }
  return Number.isInteger(index) && index >= 0 && index <= 100000 ? {playlist, index} : null;
}
export function normalizeVideo(node, playlist = '') {
  if (!node || node.is_ad || node.is_promoted) return null;
  const id = node.video_id ?? node.id ?? node.content_id;
  if (!videoId(id)) return null;
  if (node.content_type && !['VIDEO', 'MOVIE', 'SHORT', 'STATION'].includes(node.content_type)) return null;
  const author = cardAuthor(node);
  const context = playbackContext(node, id, playlist);
  const scheduled=node.upcoming instanceof Date?node.upcoming.getTime():NaN;
  return { id, title: text(node.title ?? node.metadata?.title), subtitle: text(author.name),
    overview: text(node.description_snippet, 1024),
    art_url: imageUrl(node.thumbnails?.[0]?.url ?? cardImage(node)?.image?.[0]?.url),
    channel_id: channelId(author.id) ? author.id : '',
    duration_sec: cardDuration(node),
    is_folder: false, is_upcoming: !!node.is_upcoming, is_live: cardLive(node), played: !!node.is_watched, watch_percent: watchPercent(node),
    ...(Number.isFinite(scheduled) && scheduled>0?{upcoming_start_ms:scheduled}:{}),
    ...(isShort(node)?{is_short:true}:{}),
    ...(typeof node.is_reminder_set==='boolean'?{reminder_set:node.is_reminder_set}:{}),
    ...(context ? {playback_context: context} : {}) };
}
// A channel's own Videos and Live cards name no channel: their first metadata
// part is the view count (checked live on 2026-10-11). The page owner fills in.
const ownedBy = author => (_node, item) => item.channel_id || !channelId(author?.id) || !text(author.name) ? item :
  { ...item, subtitle: text(author.name), channel_id: author.id };
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
    pinned: !!node.is_pinned, owner: !!node.author_is_channel_owner, ...commentState(node),
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
          (profile.oauth !== undefined && (!validCredentials(profile.oauth) || profile.cookie || profile.account_index || profile.channel)) ||
          (profile.account_index !== undefined && (!Number.isInteger(profile.account_index) || profile.account_index < 0 || profile.account_index > 7)) ||
          (profile.channel !== undefined && !/^[A-Za-z0-9_-]{1,100}$/.test(profile.channel)))
        throw new LibraryError('invalid_profiles', 'The profile configuration is invalid.');
      this.profiles.set(profile.id, { ...profile, name: text(profile.name) });
    }
    this.clients = new Map(); this.cursors = new Map(); this.threads = new Map(); this.watches = new Map();
    this.commentRatings = new Map();
    this.groups = new PlaybackGroups(LibraryError, feed => feed.catalog_items ?? [...Array.from(feed.videos ?? []).map(node=>normalizeVideo(node)), ...Array.from(feed.playlists ?? []).map(normalizePlaylist)].filter(Boolean), clock);
    this.historyActions = new HistoryActions(LibraryError,clock);
    this.suggestionFilters = new SuggestionFilters(LibraryError,node=>normalizeVideo(node),clock);
    this.suggestionPages = new WeakMap();
    this.feedback = new FeedbackActions(LibraryError,clock);
    // Only configured account profiles retain recommendation feedback references.
    this.homeCatalog = new HomeCatalog(LibraryError,(node,owner)=>{
      const profile=this.profiles.get(owner),item=normalizeVideo(node);
      return profile?.cookie || profile?.oauth?this.feedback.decorate(owner,node,item):item;
    },normalizePlaylist,this.suggestionFilters);
    this.channelCatalog = new HomeCatalog(LibraryError,node=>normalizeVideo(node),normalizePlaylist,this.suggestionFilters,'Channel','channel');
    this.account = new AccountActions(LibraryError,clock);
    this.chats = new ChatReader(LibraryError, text, imageUrl, clock);
    this.topics = new TopicCatalog(LibraryError, normalizeVideo, imageUrl, clock);
    this.kids = new KidsCatalog(LibraryError, node => normalizeVideo(node), clock);
    this.reminders = new StreamReminders(LibraryError, clock);
    this.playlistOrders = new PlaylistOrders(LibraryError, clock);
    this.commentPosts = new CommentPosts(LibraryError, clock);
  }
  // One client per profile and content locale: a retained feed, cursor or
  // section keeps the session that opened it, so locales never mix.
  async client(id, where = null) {
    const profile = this.profiles.get(id);
    if (!profile) throw new LibraryError('unknown_profile', 'Select an available profile.', 404);
    const key = id + (where?.key ?? '');
    if (!this.clients.has(key)) {
      const localized = Array.from(this.clients.keys()).filter(name => name.includes('|'));
      if (where?.key && localized.length >= 16) this.clients.delete(localized[0]);
      this.clients.set(key, await this.createClient(profile, where?.key ? { lang: where.lang, location: where.location } : undefined));
    }
    return this.clients.get(key);
  }
  async playback(input) {
    const owner = input?.profile;
    if (!this.profiles.has(owner))throw new LibraryError('unknown_profile','Select an available profile.',404);
    if (input.action !== 'catalog' && !this.profiles.get(owner)?.oauth)
      throw new LibraryError('playback_account_context', 'Select an OAuth account profile.', 403);
    const where = input.action === 'catalog' ? requestLocale(input, LibraryError) : null;
    const client = await this.client(owner, where);
    if (input.action === 'credentials') {
      const oauth = client.session.oauth;
      if (Date.parse(oauth.oauth2_tokens?.expiry_date) <= this.clock() + 60000)
        await oauth.refreshAccessToken();
      const credentials = oauth.oauth2_tokens;
      if (!validCredentials(credentials) || Date.parse(credentials.expiry_date) <= this.clock())
        throw new LibraryError('authentication_required', 'Sign in to the YouTube account again.', 403);
      return { access_token: credentials.access_token, expiry_date: credentials.expiry_date };
    }
    if (input.action !== 'catalog') throw new LibraryError('invalid_request', 'The playback operation is invalid.');
    if (input.cursor) {
      if (this.cursors.get(input.cursor)?.kind !== 'catalog')
        throw new LibraryError('cursor_expired', 'Reload this catalog to continue.', 409);
      return this.execute({ action: 'continue', profile: owner, cursor: input.cursor });
    }
    let feed, video = null;
    if (input.kind === 'search' && typeof input.query === 'string' && input.query.length > 0 && input.query.length <= 200)
      feed = await client.search(input.query);
    else if (input.kind === 'channel' && channelId(input.id)) {
      const channel = await client.getChannel(input.id);
      feed = await channel.getVideos();
      video = ownedBy({ name: channel.metadata?.title, id: input.id });
    }
    else if (input.kind === 'playlist' && playlistId(input.id)) feed = await client.getPlaylist(input.id);
    else throw new LibraryError('invalid_request', 'The catalog operation is invalid.');
    return this.page(owner, 'catalog', feed, null, input.kind === 'playlist' ? input.id : '', input.kind === 'channel' ? input.id : '',
      0, false, '', { hide: hides(hideSet(input.hide, LibraryError), hideSection(input)), ...(video ? { video } : {}) });
  }
  // extra.video decorates a parsed card; extra.hide drops normalized rows. Both
  // stay with the cursor, so continuation pages apply the same selection.
  page(owner, kind, feed, pending = null, playlist = '', channel = '', channelIndex = 0, history = false, group = '', extra = null) {
    const comments = kind === 'comments';
    let items = pending ?? feed.catalog_items ?? (comments ? Array.from(feed.contents ?? feed.replies ?? []).filter(thread => normalizeComment(thread)) :
      [...Array.from(feed.videos ?? []).map((node, offset) => {
        const item=normalizeVideo(node,playlist);
        if(item && channel && channelIndex+offset<=100000)
          item.playback_context={channel,index:channelIndex+offset};
        return item && extra?.video?extra.video(node,item):item;
      }), ...Array.from(feed.playlists ?? []).map(normalizePlaylist)].filter(Boolean));
    if(!comments && !playlist && !channel && !pending) {
      const attached=this.groups.attach(owner,feed,items,group,true);group=attached.key;items=attached.items;
    }
    // Hidden rows keep their source and group positions; they are only not shown.
    if(!comments && !pending && extra?.hide)items=items.filter(item=>!extra.hide(item));
    const nextChannelIndex=channel && !pending ? channelIndex+Array.from(feed.videos ?? []).length : channelIndex;
    if (items.length > 4096) throw new LibraryError('upstream_limit', 'The returned page exceeds the item limit.', 502);
    for (const [key, value] of this.threads) if (value.deadline <= this.clock()) this.threads.delete(key);
    const output = items.slice(0, 24).map(thread => {
      if (!comments) {
        const cursor=history?this.historyActions.register(owner,feed,thread.id):'';
        return cursor?{...thread,history_remove_cursor:cursor}:thread;
      }
      const item = normalizeComment(thread);
      if (thread.has_replies && typeof thread.getReplies === 'function') {
        if (this.threads.size >= 256) this.threads.delete(this.threads.keys().next().value);
        item.reply_cursor = randomUUID();
        this.threads.set(item.reply_cursor, { owner, thread, deadline: this.clock() + 120000 });
      } else item.reply_cursor = '';
      item.rating_cursor = ''; item.rating_like = item.rating_dislike = item.rating_remove = false;
      const node = thread.comment ?? thread;
      const profile = this.profiles.get(owner);
      item.respond_cursor = profile?.cookie || profile?.oauth ? this.commentPosts.register(owner, node) : '';
      if (profile?.cookie || profile?.oauth) {
        const methods = {
          like: !node.is_liked && node.like_command && typeof node.like === 'function' ? 'like' : '',
          dislike: !node.is_disliked && node.dislike_command && typeof node.dislike === 'function' ? 'dislike' : '',
          none: node.is_liked && node.unlike_command && typeof node.unlike === 'function' ? 'unlike' :
            node.is_disliked && node.undislike_command && typeof node.undislike === 'function' ? 'undislike' : ''
        };
        for (const [key, entry] of this.commentRatings) if (entry.deadline <= this.clock()) this.commentRatings.delete(key);
        if (Object.values(methods).some(Boolean) && this.commentRatings.size < 256) {
          item.rating_cursor = randomUUID();
          item.rating_like = !!methods.like; item.rating_dislike = !!methods.dislike; item.rating_remove = !!methods.none;
          this.commentRatings.set(item.rating_cursor, { owner, node, methods, deadline: this.clock() + 120000 });
        }
      }
      return item;
    });
    const remaining = items.slice(24);
    let cursor = '';
    for (const [key, value] of this.cursors) if (value.deadline <= this.clock()) this.cursors.delete(key);
    if (remaining.length || feed.has_continuation) {
      if (this.cursors.size >= 64) this.cursors.delete(this.cursors.keys().next().value);
      cursor = randomUUID();
      this.cursors.set(cursor, { owner, kind, feed, remaining, playlist, channel, channelIndex:nextChannelIndex, history, group, extra, deadline: this.clock() + 120000 });
    }
    return { api_version: 1, kind, items: output, cursor, has_more: !!cursor };
  }
  async execute(input) {
    if (!input || typeof input !== 'object') throw new LibraryError('invalid_request', 'Expected an operation.');
    const owner = input.profile ?? 'guest';
    const where = requestLocale(input, LibraryError);
    if (input.action === 'suggestions_filter') {
      if(!this.profiles.has(owner))throw new LibraryError('unknown_profile','Select an available profile.',404);
      const flags=hideSet(input.hide,LibraryError),area=this.suggestionFilters.area(owner,input.id);
      const feed=await this.suggestionFilters.open(owner,input.id);
      const signature=flags?Array.from(flags).sort().join(','):'-';
      if(this.suggestionPages.get(feed)?.signature!==signature)
        this.suggestionPages.set(feed,{signature,page:this.page(owner,'catalog',feed,null,'','',0,false,'',{hide:hides(flags,area)})});
      return this.suggestionPages.get(feed).page;
    }
    if (input.action === 'group_window') return this.groups.window(owner,input,await this.client(owner, where));
    if (input.action === 'channel_window') return channelWindow(await this.client(owner, where),input,normalizeVideo,LibraryError);
    if (input.action === 'playback_next') return playbackNext(await this.client(owner, where),input,LibraryError,
      data=>remoteRecommendationGroup(data,input,owner,this.groups,node=>normalizeVideo(node),LibraryError));
    if (['remote_playlist', 'playback_queue', 'playback_at'].includes(input.action))
      return remotePlaylist(await this.client(owner, where), input, LibraryError);
    if (input.action === 'profiles') return { api_version: 1, profiles: Array.from(this.profiles.values(), p =>
      ({ id: p.id, name: p.name, account_configured: !!(p.cookie || p.oauth) })) };
    if (input.action === 'replies') {
      const entry = this.threads.get(input.cursor);
      if (!entry || entry.owner !== owner || entry.deadline <= this.clock())
        throw new LibraryError('cursor_expired', 'Reload the comments to view replies.', 409);
      if (entry.result) return entry.result;
      const feed = await threadReplies(entry.thread, await this.client(owner, where), LibraryError);
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
      if (!entry.pending) {
        entry.pending = (async()=>{
          const feed = entry.remaining.length ? entry.feed : await this.groups.continuation(entry.feed);
          if(this.cursors.get(input.cursor)!==entry || entry.deadline<=this.clock())
            throw new LibraryError('cursor_expired','Reload this page to continue.',409);
          const result = this.page(owner, entry.kind, feed, entry.remaining.length ? entry.remaining : null, entry.playlist, entry.channel, entry.channelIndex, entry.history, entry.group, entry.extra);
          entry.result = result;
          return result;
        })();
      }
      const pending=entry.pending;
      try {return await pending;} finally {if(entry.pending===pending)entry.pending=null;}
    }
    const profile = this.profiles.get(owner);
    if (!profile) throw new LibraryError('unknown_profile', 'Select an available profile.', 404);
    const authenticated = !!(profile.cookie || profile.oauth);
    if (['live_chat', 'chat_poll', 'chat_close', 'chat_send'].includes(input.action)) {
      if (input.action === 'live_chat') {
        requireId(input.id, videoId);
        if (!['top', 'live', undefined].includes(input.chat_mode)) throw new LibraryError('invalid_sort', 'Select top or live chat.');
      }
      if (input.action === 'chat_send') {
        if (!authenticated || !(await this.client(owner, where)).session?.logged_in)
          throw new LibraryError('account_required', 'Sign in to the selected account first.', 401);
        requireId(input.id, videoId);
      }
      return this.chats.execute(input, owner, input.action === 'live_chat' ? await this.client(owner, where) : null);
    }
    const privateActions = new Set(['subscriptions', 'history', 'library', 'playlists', 'watch_later',
      'like', 'dislike', 'unrate', 'subscribe', 'unsubscribe', 'playlist_create', 'playlist_add',
      'playlist_remove', 'playlist_rename', 'playlist_delete', 'watch_later_add', 'watch_later_remove', 'watch_progress', 'comment_rate', 'history_remove', 'mark_watched',
      'channels', 'notifications', 'feedback', 'playlist_save', 'playlist_unsave', 'history_pause', 'history_resume', 'history_clear',
      'subscribe_notifications', 'playlist_order', 'comment_post', 'comment_reply']);
    // Reading a reminder is public; changing it belongs to an account.
    if (input.action === 'stream_reminder' && input.reminder !== 'status') privateActions.add('stream_reminder');
    if (privateActions.has(input.action) && !authenticated)
      throw new LibraryError('account_required', 'Configure an account profile on the companion first.', 401);
    if (!privateActions.has(input.action) && !['home', 'playlist', 'comments', 'topics', 'topic_open', 'suggestions', 'search', 'search_suggestions', 'description', 'channel', 'channel_search', 'video_details', 'stream_reminder'].includes(input.action))
      throw new LibraryError('unknown_operation', 'This library operation is unavailable.');
    if (['channel', 'channel_search'].includes(input.action)) requireId(input.channel, channelId);
    if (['like', 'dislike', 'unrate', 'comments', 'playlist_add', 'playlist_remove', 'watch_later_add', 'watch_later_remove', 'watch_progress', 'history_remove', 'mark_watched', 'description', 'feedback',
      'video_details', 'stream_reminder', 'comment_post', 'comment_reply'].includes(input.action))
      requireId(input.id, videoId);
    if (['subscribe', 'unsubscribe', 'subscribe_notifications'].includes(input.action)) requireId(input.id, channelId);
    const level = input.notifications ?? '';
    if ((level !== '' && (!['all', 'personalized', 'none'].includes(level) || !['subscribe', 'subscribe_notifications'].includes(input.action))) ||
        (level === '' && input.action === 'subscribe_notifications'))
      throw new LibraryError('invalid_request', 'Select all, personalized or none for a subscription.');
    if (![undefined, false].includes(input.first_playable) && (input.first_playable !== true || input.action !== 'search'))
      throw new LibraryError('invalid_search', 'Only a search can return its first playable video.');
    if (input.action === 'playlist') requireId(input.id, playlistId);
    if (input.action === 'suggestions') requireId(input.id, videoId);
    if (['playlist_add', 'playlist_remove', 'playlist_rename', 'playlist_delete'].includes(input.action))
      requireId(input.playlist, playlistId);
    const query = ['search', 'channel_search'].includes(input.action) ? searchQuery(input.query, LibraryError) : '';
    const flags = hideSet(input.hide, LibraryError);
    const extra = { hide: hides(flags, hideSection(input)) };
    const filters = input.action === 'search' ? searchFilters(input, LibraryError) : null;
    if (input.action === 'search_suggestions') searchQuery(input.query ?? '', LibraryError, true);
    const client = await this.client(owner, where);
    if (input.action === 'feedback') return this.feedback.submit(owner, client, input);
    if (['playlist_save', 'playlist_unsave', 'history_pause', 'history_resume', 'history_clear'].includes(input.action))
      return this.account.run(owner, client, input);
    if (input.action === 'channel' || input.action === 'channel_search') {
      const opened = await channelPage(client, input, LibraryError, query);
      const author = { name: opened.info.title, id: input.channel };
      const feed = opened.info.tab === 'home' ? channelHome(owner, opened.feed, client, this.channelCatalog, node => normalizeVideo(node)) :
        opened.info.tab === 'shorts' ? mappedFeed(opened.feed, node => normalizeVideo(shortsCard(node, author)), normalizePlaylist) : opened.feed;
      if (['videos', 'live'].includes(opened.info.tab)) extra.video = ownedBy(author);
      return { ...this.page(owner, 'catalog', feed, null, '', opened.indexed ? input.channel : '', 0, false, '', extra), channel: opened.info };
    }
    if (input.action === 'search_suggestions') return searchSuggestions(client, input, LibraryError);
    if (input.action === 'description') return videoDescription(client, input, LibraryError);
    if (input.action === 'video_details') return videoDetails(client, input, LibraryError, imageUrl);
    if (input.action === 'stream_reminder') return this.reminders.run(owner, client, input);
    if (input.action === 'playlist_order') return this.playlistOrders.run(owner, client, input);
    if (['comment_post', 'comment_reply'].includes(input.action)) return this.commentPosts.run(owner, client, input);
    if(input.action==='history_remove')return this.historyActions.remove(owner,client,input);
    if(input.action==='mark_watched')return this.historyActions.markWatched(owner,client,input);
    if (input.action === 'topics' || input.action === 'topic_open') {
      const feed = input.section === 'kids' ?
        (input.action === 'topics' ? await this.kids.root(owner, client) : await this.kids.open(owner, input.id)) :
        input.action === 'topics' ? await this.topics.root(owner, input.section, client,
        authenticated && client.session?.logged_in === true) :
        await this.topics.open(owner, input.section, input.id);
      return this.page(owner, 'catalog', feed, null, '', '', 0, false, '', extra);
    }
    if (input.action === 'comment_rate') {
      const entry = this.commentRatings.get(input.cursor);
      if (!entry || entry.owner !== owner || entry.deadline <= this.clock())
        throw new LibraryError('cursor_expired', 'Reload the comments before rating this comment.', 409);
      if (!['like', 'dislike', 'none'].includes(input.rating) || !entry.methods[input.rating])
        throw new LibraryError('invalid_rating', 'Select an available comment rating.');
      if (!client.session?.logged_in) throw new LibraryError('account_required', 'Sign in to the selected account first.', 401);
      if (entry.attempted) {
        if (entry.result && entry.rating === input.rating) return entry.result;
        throw new LibraryError('comment_unconfirmed', 'Reload the comments; this rating intent cannot be repeated.', 409);
      }
      entry.attempted = true; entry.rating = input.rating;
      const response = await entry.node[entry.methods[input.rating]]();
      if (response?.success !== true || !Number.isInteger(response.status_code) ||
          response.status_code < 200 || response.status_code >= 300 || response.data?.error)
        throw new LibraryError('comment_unconfirmed', 'YouTube did not accept the comment rating.', 502);
      entry.result = { api_version: 1, success: true, rating: input.rating };
      return entry.result;
    }
    if (input.action === 'watch_progress') {
      if (!client.session?.logged_in) throw new LibraryError('account_required', 'Sign in to the selected account first.', 401);
      if (!/^[A-Za-z0-9_-]{1,64}$/.test(input.session ?? '') ||
          !Number.isFinite(input.position_sec) || input.position_sec < 0 || input.position_sec > 1000000 ||
          !['playing', 'stopped'].includes(input.state))
        throw new LibraryError('invalid_progress', 'The playback report is invalid.');
      for (const [key, entry] of this.watches) if (entry.deadline <= this.clock()) this.watches.delete(key);
      const key = `${owner}:${input.session}`;
      let entry = this.watches.get(key);
      if (entry && entry.id !== input.id) throw new LibraryError('invalid_progress', 'The playback session belongs to another video.');
      if (entry?.failed) throw new LibraryError('history_unconfirmed', 'The history change was not confirmed; this session will not repeat it.', 409);
      if (entry?.position === input.position_sec && entry?.state === input.state) return entry.result;
      if (entry?.closed) throw new LibraryError('session_closed', 'This playback session has ended.', 409);
      if (!entry) {
        if (this.watches.size >= 64) throw new LibraryError('history_busy', 'Too many playback sessions are retained.', 429);
        entry = { id: input.id, failed: true, deadline: this.clock() + 1800000 };
        this.watches.set(key, entry);
        entry.info = await client.getBasicInfo(input.id);
        if (entry.info?.basic_info?.id !== input.id)
          throw new LibraryError('history_unconfirmed', 'The tracking response belongs to another video.', 502);
        const started = await entry.info.addToWatchHistory();
        if (started?.ok !== true) throw new LibraryError('history_unconfirmed', 'YouTube did not confirm the history change.', 502);
      }
      entry.failed = true;
      const updated = await entry.info.updateWatchTime(input.position_sec);
      if (updated?.ok !== true) throw new LibraryError('history_unconfirmed', 'YouTube did not confirm the watch position.', 502);
      entry.failed = false; entry.position = input.position_sec; entry.state = input.state;
      entry.closed = input.state === 'stopped'; entry.deadline = this.clock() + 1800000;
      entry.result = { api_version: 1, success: true, position_sec: input.position_sec };
      return entry.result;
    }
    let feed;
    switch (input.action) {
      case 'home': feed = this.homeCatalog.root(owner,await client.getHomeFeed(),client,true,!flags || flags.has('watched_home')); break;
      case 'suggestions': {
        const info=await client.getInfo(input.id);
        if(info?.basic_info?.id!==input.id)
          throw new LibraryError('upstream_format','YouTube returned suggestions for a different video.',502);
        feed=this.suggestionFilters.root(owner,info,client);break;
      }
      case 'search': feed = searchFeed(await client.search(query, filters), node=>normalizeVideo(node), normalizePlaylist,
        node=>normalizeChannel(node, imageUrl), LibraryError); break;
      case 'channels': {
        // The marker needs one extra guide request, so it is opt-in.
        const marks = input.new_content === true ? await channelMarks(client) : null;
        feed = channelsFeed(await client.getChannelsFeed(), node => {
          const item = normalizeChannel(node, imageUrl);
          return item && marks?.has(item.channel_id) ? { ...item, has_new: marks.get(item.channel_id) } : item;
        }, LibraryError); break;
      }
      case 'notifications': feed = notificationsFeed(await client.getNotifications(), node=>normalizeVideo(node), LibraryError); break;
      // Subscriptions cards keep their private hide token behind a feedback reference.
      case 'subscriptions': feed = await client.getSubscriptionsFeed();
        extra.video = (node, item) => this.feedback.decorate(owner, node, item); break;
      case 'history': feed = await client.getHistory(); break;
      case 'library': feed = await client.getLibrary(); break;
      case 'playlists': feed = await client.getPlaylists(); break;
      case 'watch_later': feed = await client.getPlaylist('WL'); break;
      case 'playlist': feed = await client.getPlaylist(requireId(input.id, playlistId)); break;
      case 'comments':
        if (!['top', 'newest', undefined].includes(input.sort)) throw new LibraryError('invalid_sort', 'Select top or newest comments.');
        feed = await client.getComments(requireId(input.id, videoId), input.sort === 'newest' ? 'NEWEST_FIRST' : 'TOP_COMMENTS');
        return { ...this.page(owner, 'comments', feed), ...commentHeader(feed, authenticated && client.session?.logged_in === true) };
      default: break;
    }
    if (feed && input.first_playable === true) {
      // Voice and phone "play ..." requests: the first video of the first page.
      const page = this.page(owner, 'catalog', feed, null, '', '', 0, false, '', extra);
      if (page.cursor) this.cursors.delete(page.cursor);
      const item = page.items.find(row => row.kind === 'video' && !row.is_folder && !row.is_upcoming) ?? null;
      return { api_version: 1, kind: 'search_first', query, found: !!item, item };
    }
    if (feed) {
      const page = this.page(owner, 'catalog', feed, null,
        input.action === 'playlist' ? input.id : input.action === 'watch_later' ? 'WL' : '', '', 0, input.action==='history', '', extra);
      const paused = input.action === 'history' ? historyPaused(feed) : undefined;
      return paused === undefined ? page : { ...page, history_paused: paused };
    }
    let response;
    switch (input.action) {
      case 'like': response = await client.interact.like(requireId(input.id, videoId)); break;
      case 'dislike': response = await client.interact.dislike(requireId(input.id, videoId)); break;
      case 'unrate': response = await client.interact.removeRating(requireId(input.id, videoId)); break;
      case 'subscribe': response = await client.interact.subscribe(requireId(input.id, channelId)); break;
      case 'unsubscribe': response = await client.interact.unsubscribe(requireId(input.id, channelId)); break;
      case 'subscribe_notifications':
        if (!client.session?.logged_in) throw new LibraryError('account_required', 'Sign in to the selected account first.', 401);
        response = await client.interact.setNotificationPreferences(requireId(input.id, channelId), level.toUpperCase()); break;
      case 'playlist_create': {
        const title = playlistTitle(input.title);
        if (!client.session?.logged_in) throw new LibraryError('account_required', 'Sign in to the selected account first.', 401);
        const created = await client.actions.execute('playlist/create', {
          title, videoIds: input.id ? [requireId(input.id, videoId)] : [], privacyStatus: 'PRIVATE'
        });
        if (!playlistId(created?.data?.playlistId))
          throw new LibraryError('operation_failed', 'YouTube did not return the created playlist.', 502);
        response = { success: created.success, status_code: created.status_code, playlist_id: created.data.playlistId }; break;
      }
      case 'playlist_rename': {
        const title = playlistTitle(input.title);
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
    const done = { api_version: 1, success: true, playlist_id: playlistId(response.playlist_id) ? response.playlist_id : '' };
    if (input.action === 'subscribe_notifications') return { ...done, notifications: level };
    if (input.action !== 'subscribe' || !level) return done;
    // The subscription stands even when the level could not be confirmed.
    let applied = false;
    try { applied = accepted(await client.interact.setNotificationPreferences(input.id, level.toUpperCase())); }
    catch { /* Reported as an unset level. */ }
    return { ...done, notifications: applied ? level : '' };
  }
}
