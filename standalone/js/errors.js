// Errors that cross the HTTP boundary, with the companion's codes, messages
// and statuses (models.py, resolver_errors.py). SPDX-License-Identifier: GPL-3.0-or-later
export class ServiceError extends Error {
  constructor(code, message, status = 502) { super(message); this.code = code; this.status = status; }
}

const RESOLVER = {
  video_unavailable: ['This YouTube video is unavailable or has been removed. Choose another video.', 404],
  authentication_required: ['This video requires an authorized YouTube account. Select a configured account profile on the companion.', 403],
  youtube_challenge: ["YouTube requires account verification for this connection. Check the companion's account configuration before retrying.", 403],
  youtube_rate_limited: ['YouTube is limiting this connection. Wait before retrying.', 429],
  resolver_network: ['The companion could not reach YouTube. Check its Internet connection and retry.', 502],
  extraction_failed: ["YouTube could not be resolved. Check the companion's yt-dlp/runtime configuration and retry.", 502]
};
export const resolverError = code => new ServiceError(code, ...RESOLVER[Object.hasOwn(RESOLVER, code) ? code : 'extraction_failed']);

// resolver_errors.classify_error, applied to a player answer or a thrown error.
export function classify(text) {
  const lowered = String(text ?? '').slice(0, 8192).toLowerCase();
  const has = (...phrases) => phrases.some(phrase => lowered.includes(phrase));
  if (has("confirm you're not a bot", 'confirm you’re not a bot')) return 'youtube_challenge';
  if (has('http error 429', 'too many requests', 'status code 429')) return 'youtube_rate_limited';
  if (has('private video', 'this video is private', 'sign in to confirm your age', 'members-only', 'join this channel', 'login required', 'login_required')) return 'authentication_required';
  if (has('this video is unavailable', 'video unavailable', 'video has been removed', 'video is not available', 'video has been deleted')) return 'video_unavailable';
  if (has('fetch failed', 'timed out', 'timeout', 'could not resolve', 'connection refused')) return 'resolver_network';
  return 'extraction_failed';
}

// Python's text(): no NUL, at most `limit` code points.
export function text(value, limit = 192) {
  const clean = String(value ?? '').replaceAll('\0', '');
  return clean.length <= limit ? clean : Array.from(clean).slice(0, limit).join('');
}
// Python's number(): a finite value in 0..1e9, otherwise the default.
export function number(value, fallback = 0) {
  const parsed = typeof value === 'number' ? value : typeof value === 'string' && value.trim() !== '' ? Number(value) : NaN;
  return Number.isFinite(parsed) && parsed >= 0 && parsed <= 1e9 ? parsed : fallback;
}
// models.safe_remote: resolver output is not permission to read arbitrary hosts.
const MEDIA_DOMAINS = ['googlevideo.com', 'youtube.com', 'ytimg.com', 'ggpht.com', 'googleusercontent.com'];
export function safeRemote(address) {
  let url;
  try { url = new URL(address); } catch { throw new ServiceError('unsupported_url', 'The resolver returned an unsupported media URL.'); }
  if (url.protocol !== 'https:' || url.username || url.password || url.port)
    throw new ServiceError('unsupported_url', 'The resolver returned an unsupported media URL.');
  if (!MEDIA_DOMAINS.some(domain => url.hostname === domain || url.hostname.endsWith(`.${domain}`)))
    throw new ServiceError('unsupported_origin', 'The resolver returned an unsupported media origin.');
  return String(address);
}

// Production diagnostics never include account identifiers, URLs, messages or response bodies.
export function libraryDiagnostic(error, operation, action) {
  const stage = ['execute', 'device', 'client', 'playback'].includes(operation) ? operation : 'other';
  const task = ['history', 'library', 'playlists', 'watch_later', 'playlist', 'home', 'subscriptions',
    'channels', 'notifications', 'notification_count', 'start', 'poll'].includes(action) ? action : 'other';
  const type = error?.constructor?.name;
  const category = ['TypeError', 'ReferenceError', 'SyntaxError', 'RangeError', 'InnertubeError',
    'ParsingError', 'OAuth2Error', 'SessionError', 'LibraryError', 'ServiceError'].includes(type) ? type : 'Error';
  return `YouTube operation failed: stage=${stage} action=${task} category=${category}`;
}
