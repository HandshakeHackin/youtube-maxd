// Request models of companion/prosperotube/app.py and models.py: the same
// fields, defaults, bounds and cross-field rules. SPDX-License-Identifier: GPL-3.0-or-later
//
// A refused request is answered 422 like FastAPI does; only the status and the
// `detail` list shape are kept, not pydantic's wording.
export class Invalid extends Error {
  constructor(message, field = '') { super(message); this.field = field; }
}

const LANGUAGES = new Set(('ar az bg bn ca cs da de el en es et fa fi fr gl hi hr hu hy id it ja ko lt lv nl pl pt-PT ro ru sk sl sq sr sv ' +
  'te th tr uk vi zh-CN zh-TW en-NZ he mo nb pt-BR tjl').split(' '));
const COUNTRIES = new Set(('US GB HR CA UA RU BY KZ AZ AM RS EE BG DE RO ES PL FI LV FR HU NL BE NO CZ IT GR DK LT MD SK SI SE SG NZ NG AU ' +
  'CH IL SA MA BD BR PT AR MX HK MY JP IN KE TR EG VN CN TW ID KR PK BO CL CO CR EC SV GT HN NI PA PR PY PE DO UY ' +
  'VE IE PH IQ AT TZ NP ZA ZW DZ TH MK TN LK JO').split(' '));
export const validLocale = (hl, gl) => typeof hl === 'string' && typeof gl === 'string' && (!hl || LANGUAGES.has(hl)) && (!gl || COUNTRIES.has(gl));

const HIDE = ['shorts_all', 'mixes', 'watched_watch_later', 'watched_home', 'watched_subscriptions', 'watched_notifications',
  'shorts_subscriptions', 'shorts_search', 'shorts_home', 'shorts_channel', 'shorts_history', 'shorts_trending',
  'streams_subscriptions', 'upcoming_subscriptions', 'upcoming_home', 'upcoming_channel'];
const THUMBNAILS = ['default', 'start', 'middle', 'end'];
const PROFILE = /^[A-Za-z0-9_-]{1,32}$/, VIDEO = /^[A-Za-z0-9_-]{11}$/, SESSION = /^[A-Za-z0-9_-]{1,64}$/;
const CHANNEL = /^UC[A-Za-z0-9_-]{22}$/, LIST = /^(?:WL|LL|[A-Za-z0-9_-]{10,100})$/, GROUP = /^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/;
const length = text => { let count = 0; for (const _ of text) count++; return count; };   // code points, as Python counts

// Field kinds. Each returns the accepted value or throws.
const text = (fallback, max, pattern) => (value, name) => {
  if (value === undefined) return fallback;
  if (typeof value !== 'string' || (max !== undefined && length(value) > max) || (pattern && !pattern.test(value))) throw new Invalid('String should match the field rules', name);
  return value;
};
const choice = (fallback, values) => (value, name) => {
  if (value === undefined) return fallback;
  if (!values.includes(value)) throw new Invalid('Input should be one of the listed values', name);
  return value;
};
const integer = (fallback, min, max) => (value, name) => {
  if (value === undefined || (value === null && fallback === null)) return fallback;
  if (!Number.isInteger(value) || value < min || value > max) throw new Invalid('Input should be an integer in range', name);
  return value;
};
const number = (fallback, min, max) => (value, name) => {
  if (value === undefined || (value === null && fallback === null)) return fallback;
  if (typeof value !== 'number' || !Number.isFinite(value) || value < min || value > max) throw new Invalid('Input should be a number in range', name);
  return value;
};
const flag = fallback => (value, name) => {
  if (value === undefined) return fallback;
  if (typeof value !== 'boolean') throw new Invalid('Input should be a boolean', name);
  return value;
};
const list = (fallback, values, max) => (value, name) => {
  if (value === undefined || (value === null && fallback === null)) return fallback === null ? null : [];
  if (!Array.isArray(value) || value.length > max || value.some(entry => !values.includes(entry))) throw new Invalid('Input should be a list of the listed values', name);
  return value.slice();
};

function build(fields, input, { forbidExtra = false } = {}) {
  if (!input || typeof input !== 'object' || Array.isArray(input)) throw new Invalid('Input should be an object');
  if (forbidExtra) for (const name of Object.keys(input)) if (!Object.hasOwn(fields, name)) throw new Invalid('Extra inputs are not permitted', name);
  const output = {};
  for (const name of Object.keys(fields)) output[name] = fields[name](input[name], name);
  return output;
}
const distinct = values => !values || new Set(values).size === values.length;

const LIBRARY = {
  action: choice(undefined, ['profiles', 'home', 'subscriptions', 'history', 'library', 'playlists', 'suggestions', 'suggestions_filter',
    'watch_later', 'playlist', 'comments', 'replies', 'continue', 'like', 'dislike', 'unrate', 'subscribe', 'unsubscribe',
    'playlist_create', 'playlist_add', 'playlist_remove', 'playlist_rename', 'playlist_delete', 'watch_later_add',
    'watch_later_remove', 'watch_progress', 'profile_remove', 'comment_rate', 'mark_watched', 'live_chat', 'chat_poll',
    'chat_close', 'chat_send', 'topics', 'topic_open', 'playback_queue', 'playback_at', 'playback_next', 'channel_window',
    'group_window', 'history_remove', 'search', 'search_suggestions', 'description', 'channels', 'notifications', 'feedback',
    'channel', 'channel_search', 'playlist_save', 'playlist_unsave', 'history_pause', 'history_resume', 'history_clear',
    'video_details', 'stream_reminder', 'playlist_order', 'subscribe_notifications', 'comment_post', 'comment_reply']),
  section: choice('', ['', 'music', 'gaming', 'news', 'sports', 'live', 'movies', 'trending', 'shorts', 'my_videos', 'kids']),
  profile: text('guest', undefined, PROFILE),
  id: text('', 100), playlist: text('', 100), channel: text('', 24),
  group: text('', undefined, /^(|[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12})$/),
  target: integer(null, 0, 100000), index: integer(null, 0, 100000), direction: integer(0, -1, 1),
  require_channel: flag(false), require_name: flag(false), remote_origin: flag(false),
  title: text('', 150), sort: choice('top', ['top', 'newest']), rating: choice('none', ['like', 'dislike', 'none']),
  chat_mode: choice('top', ['top', 'live']), chat_history: flag(false), chat_page: integer(null, 0, 1000000),
  chat_position_sec: number(null, 0, 1000000), cursor: text('', 64), session: text('', 64, /^[A-Za-z0-9_-]*$/),
  text: text('', 1000), position_sec: number(0, 0, 1000000), state: choice('playing', ['playing', 'stopped']),
  query: text('', 200), upload_date: choice('any', ['any', 'today', 'week', 'month', 'year']),
  duration: choice('any', ['any', 'short', 'medium', 'long']),
  type: choice('all', ['all', 'video', 'channel', 'playlist', 'movie', 'shorts']),
  features: list([], ['hd', 'subtitles', 'creative_commons', '3d', 'live', 'purchased', '4k', '360', 'location', 'hdr', 'vr180'], 11),
  order: choice('relevance', ['relevance', 'view_count']),
  feedback: choice('', ['', 'not_interested', 'dont_recommend_channel', 'hide']),
  hl: text('', 8), gl: text('', 2), hide: list(null, HIDE, 16),
  tab: choice('', ['', 'home', 'videos', 'shorts', 'live', 'playlists']),
  channel_sort: choice('', ['', 'newest', 'popular', 'oldest']), thumbnails: choice('default', THUMBNAILS),
  reminder: choice('', ['', 'status', 'set', 'unset']),
  playlist_order: choice('', ['', 'added_newest', 'added_oldest', 'popular', 'published_newest', 'published_oldest']),
  notifications: choice('', ['', 'all', 'personalized', 'none']), first_playable: flag(false), new_content: flag(false)
};
const ACCOUNT_INTENTS = ['playlist_save', 'playlist_unsave', 'history_pause', 'history_resume', 'history_clear'];
const NAVIGATION = ['playback_next', 'playback_queue', 'playback_at', 'channel_window', 'group_window'];

export function libraryRequest(input) {
  const r = build(LIBRARY, input, { forbidExtra: true });
  if (r.action === undefined) throw new Invalid('Field required', 'action');
  const refuse = (condition, message) => { if (condition) throw new Invalid(`Value error, ${message}`); };
  const video = VIDEO.test(r.id), session = SESSION.test(r.session), a = r.action;
  // third_batch
  refuse((a === 'stream_reminder') !== !!r.reminder, 'A reminder choice belongs to the stream_reminder action.');
  refuse(a === 'stream_reminder' && (!video || (r.reminder !== 'status' && !session)), 'A stream reminder requires a video and, for a change, a confirmation session.');
  refuse(r.playlist_order && a !== 'playlist_order', 'A playlist order belongs to the playlist_order action.');
  refuse(a === 'playlist_order' && (!LIST.test(r.playlist) || r.id || (!!r.playlist_order && !session)), 'A playlist order requires a playlist and, for a change, a confirmation session.');
  refuse(r.notifications && !['subscribe', 'subscribe_notifications'].includes(a), 'A notification level belongs to a subscription.');
  refuse(a === 'subscribe_notifications' && (!r.notifications || !CHANNEL.test(r.id)), 'Changing the notification level requires a channel and a level.');
  refuse(r.first_playable && a !== 'search', 'Only a search can return its first playable video.');
  refuse(r.new_content && a !== 'channels', 'New-content marks belong to the channels action.');
  refuse(a === 'video_details' && !video, 'Video details require a video identity.');
  const posting = ['comment_post', 'comment_reply'].includes(a);
  refuse(posting && (!video || !session || !r.text.trim() || /[\x00-\x09\x0b-\x1f\x7f]/.test(r.text) || (a === 'comment_reply' && !SESSION.test(r.cursor))),
    'Posting a comment requires a video, text, a confirmation session and, for a reply, its reference.');
  refuse(length(r.text) > 200 && !posting, 'Only comments may be longer than 200 characters.');
  // locale_filters_channels_and_intents
  refuse(!validLocale(r.hl, r.gl), 'Select a supported content language and country.');
  refuse(!distinct(r.hide), 'Content filters must be distinct.');
  refuse((r.tab || r.channel_sort) && a !== 'channel', 'Channel sections and orders require the channel action.');
  refuse(['channel', 'channel_search'].includes(a) && (!CHANNEL.test(r.channel) || r.id || r.playlist), 'Channel pages require a channel identity.');
  if (ACCOUNT_INTENTS.includes(a)) {
    refuse(!session, 'Account changes require a confirmation session.');
    refuse(a.startsWith('playlist_') && (['WL', 'LL'].includes(r.playlist) || !/^[A-Za-z0-9_-]{10,100}$/.test(r.playlist)), "Saving a playlist requires another channel's playlist identity.");
  }
  // search_and_feedback
  const options = r.upload_date !== 'any' || r.duration !== 'any' || r.type !== 'all' || r.order !== 'relevance' || r.features.length > 0;
  refuse(options && a !== 'search', 'Search options require the search action.');
  refuse(!distinct(r.features), 'Search features must be distinct.');
  refuse(r.query && !['search', 'search_suggestions', 'channel_search'].includes(a), 'Query text requires a search action.');
  refuse(/[\x00-\x1f\x7f]/.test(r.query) || (['search', 'channel_search'].includes(a) && !r.query.trim()), 'Enter search text without control characters.');
  refuse(a === 'description' && !video, 'A description requires a video identity.');
  if (a === 'feedback') refuse(!r.feedback || !video || !SESSION.test(r.cursor), 'Feedback requires a video, its retained reference and a choice.');
  else refuse(r.feedback, 'Feedback choices require the feedback action.');
  // playback_occurrence
  refuse(r.remote_origin && a !== 'playback_next', 'Remote fallback requires playback_next.');
  refuse(a === 'suggestions_filter' && !GROUP.test(r.id), 'Select an available suggestions filter.');
  refuse(a === 'suggestions' && !video, 'Suggestions require a video identity.');
  refuse(r.require_name && !r.require_channel, 'Channel-name verification requires channel verification.');
  refuse(r.require_channel && !NAVIGATION.includes(a), 'Channel verification requires a playback navigation request.');
  refuse(a === 'mark_watched' && (!video || !session), 'Mark as watched requires a video and confirmation session.');
  refuse(a === 'history_remove' && (!video || !SESSION.test(r.cursor)), 'History removal requires a video and its retained history reference.');
  if (a === 'group_window') refuse(!r.group || !video || r.index === null || r.index > 4095 || (r.target !== null && r.target > 4095) || r.playlist || r.channel, 'Group navigation requires a retained source occurrence.');
  else refuse(r.group, 'Group references require group_window.');
  if (a === 'channel_window') refuse(!CHANNEL.test(r.channel) || !video || r.index === null || r.playlist, 'Channel navigation requires a channel and anchored video occurrence.');
  else refuse((r.channel && !['channel', 'channel_search'].includes(a)) || (r.target !== null && a !== 'group_window'), 'Channel positions require channel_window.');
  refuse(a === 'playback_next' && (!video || (r.playlist ? !LIST.test(r.playlist) || r.index === null : r.index !== null)), 'Next-video requests require a video ID and optional playlist occurrence.');
  refuse(r.direction && !['playback_at', 'channel_window', 'group_window'].includes(a), 'Directional navigation requires playback_at.');
  refuse(['playback_queue', 'playback_at'].includes(a) && ((a === 'playback_queue' ? !video : r.id !== '') || !LIST.test(r.playlist) || r.index === null),
    'Playback requests require a playlist and occurrence index; only playback_queue accepts a video ID.');
  return r;
}

const RESOLVE = {
  id: (value, name) => { if (typeof value !== 'string' || length(value) !== 11) throw new Invalid('String should have 11 characters', name); return value; },
  profile: text('guest', undefined, PROFILE), max_height: integer(1080, 144, 2160), max_fps: integer(0, 0, 60),
  codec: choice('auto', ['auto', 'avc', 'vp9', 'av1', 'hevc']),
  caption_language: text('en', 16, /^[A-Za-z0-9_-]*$/), audio_language: text('', 16, /^[A-Za-z0-9_-]*$/),
  automatic_captions: flag(false), translated_captions: flag(false), fresh: flag(false), hl: text('', 8)
};
export function resolveRequest(input) {
  const r = build(RESOLVE, input);
  if (!validLocale(r.hl, '')) throw new Invalid('Value error, Select a supported content language and country.');
  return r;
}

const JOB = {
  dearrow_titles: flag(false), dearrow_thumbnails: flag(false),
  action: (value, name) => { if (!['search', 'catalog', 'resolve', 'details', 'library', 'auth'].includes(value)) throw new Invalid('String should match pattern', name); return value; },
  query: text('', 200), parent: text('', 160), page: integer(0, 0, 1000),
  playback: value => value === undefined || value === null ? null : resolveRequest(value),
  library: value => value === undefined || value === null ? null : libraryRequest(value),
  auth: value => value === undefined || value === null ? null : build({ action: choice(undefined, ['start', 'status', 'cancel']),
    name: text('YouTube account', 150), session: text('', undefined, /^(|[0-9a-f]{32})$/) }, value, { forbidExtra: true }),
  id: text('', 11), profile: text('guest', undefined, PROFILE), hl: text('', 8), gl: text('', 2),
  hide: list(null, HIDE, 16), thumbnails: choice('default', THUMBNAILS)
};
export function jobRequest(input) {
  const r = build(JOB, input);
  if (!validLocale(r.hl, r.gl)) throw new Invalid('Value error, Select a supported content language and country.');
  if (!distinct(r.hide)) throw new Invalid('Value error, Content filters must be distinct.');
  return r;
}

// Query-string parameters of the direct GET routes.
export function queryOptions(params, extra = {}) {
  const read = name => params.has(name) ? params.get(name) : undefined;
  const truth = name => { const value = read(name); if (value === undefined) return false;
    if (['true', '1', 'on', 'yes'].includes(value.toLowerCase())) return true;
    if (['false', '0', 'off', 'no'].includes(value.toLowerCase())) return false;
    throw new Invalid('Input should be a valid boolean', name); };
  const page = read('page') === undefined ? 0 : Number(read('page'));
  if (!Number.isInteger(page) || page < 0 || page > 1000) throw new Invalid('Input should be an integer in range', 'page');
  const output = { page, profile: text('guest', undefined, PROFILE)(read('profile'), 'profile'), hl: text('', 8)(read('hl'), 'hl'),
    gl: text('', 2)(read('gl'), 'gl'), thumbnails: choice('default', THUMBNAILS)(read('thumbnails'), 'thumbnails'),
    dearrow_titles: truth('dearrow_titles'), dearrow_thumbnails: truth('dearrow_thumbnails') };
  for (const [name, [min, max]] of Object.entries(extra)) {
    const value = read(name);
    if (value === undefined || length(value) < min || length(value) > max) throw new Invalid(value === undefined ? 'Field required' : 'String length out of range', name);
    output[name] = value;
  }
  return output;
}
