// Original behavior/failure tests; no network or account use. GPL-3.0-or-later
import test from 'node:test';
import assert from 'node:assert/strict';
import { Library, normalizeVideo, imageUrl } from './library.mjs';
import { YTNodes, YT } from 'youtubei.js';

test('watch history uses actual pinned tracking methods and isolates playback sessions', async () => {
  const calls = [];
  const info = new YT.VideoInfo([{ data: { videoDetails: { videoId: 'abcdefghijk', title: 'Fixture' },
    playabilityStatus: { status: 'OK' }, playbackTracking: {
      videostatsPlaybackUrl: { baseUrl: 'https://s.youtube.com/api/stats/playback?fixture=1' },
      videostatsWatchtimeUrl: { baseUrl: 'https://s.youtube.com/api/stats/watchtime?fixture=1' }
    } } }], { stats: async (url, client, params) => { calls.push([url, params]); return new Response(null, { status: 204 }); } }, 'fixture-cpn');
  const library = new Library(async profile => ({ session: { logged_in: true }, getBasicInfo: async id => {
    calls.push([profile.id, id]); return info;
  } }), [{ id: 'one', cookie: 'fixture-cookie' }, { id: 'two', cookie: 'other-fixture' }]);
  const request = { action: 'watch_progress', profile: 'one', id: 'abcdefghijk', session: 'playback-one', position_sec: 1, state: 'playing' };
  const first = await library.execute(request);
  assert.equal(first.success, true); assert.equal(calls.length, 3);
  assert.deepEqual(calls[0], ['one', 'abcdefghijk']);
  assert.equal(calls[1][0], 'https://www.youtube.com/api/stats/playback?fixture=1');
  assert.equal(calls[1][1].cpn, 'fixture-cpn'); assert.equal(calls[2][1].cmt, '1.000');
  assert.deepEqual(await library.execute(request), first); assert.equal(calls.length, 3);
  await library.execute({ ...request, position_sec: 31 }); assert.equal(calls.length, 4);
  await library.execute({ ...request, position_sec: 8, state: 'stopped' }); assert.equal(calls.length, 5);
  await library.execute({ ...request, position_sec: 8, state: 'stopped' }); assert.equal(calls.length, 5);
  await assert.rejects(library.execute({ ...request, position_sec: 9 }), { code: 'session_closed' });
  await assert.rejects(library.execute({ ...request, id: '12345678901' }), { code: 'invalid_progress' });
  await library.execute({ ...request, profile: 'two' }); assert.equal(calls.length, 8);
  await assert.rejects(library.execute({ ...request, profile: 'guest' }), { code: 'account_required' });
});

test('unconfirmed history calls stop retries, validate reports and bound retained sessions', async () => {
  let time = 0, starts = 0, updates = 0, logged_in = true, status = 204;
  const library = new Library(async () => ({ session: { get logged_in() { return logged_in; } },
    getBasicInfo: async () => ({ basic_info: { id: 'abcdefghijk' }, addToWatchHistory: async () => { starts++; return new Response(null, { status }); },
      updateWatchTime: async () => { updates++; return new Response(null, { status: 503 }); } }) }),
  [{ id: 'one', cookie: 'fixture' }], () => time);
  const request = { action: 'watch_progress', profile: 'one', id: 'abcdefghijk', session: 'playback-one', position_sec: 1, state: 'playing' };
  logged_in = false;
  await assert.rejects(library.execute(request), { code: 'account_required' }); assert.equal(starts, 0);
  logged_in = true;
  for (const extra of [{ position_sec: NaN }, { position_sec: -1 }, { position_sec: Infinity }, { position_sec: 1000001 }, { state: 'invalid' }, { session: '' }, { session: 'bad/session' }])
    await assert.rejects(library.execute({ ...request, ...extra }), { code: 'invalid_progress' });
  await assert.rejects(library.execute(request), { code: 'history_unconfirmed' });
  await assert.rejects(library.execute(request), { code: 'history_unconfirmed' });
  assert.equal(starts, 1); assert.equal(updates, 1);
  status = 503;
  for (let i = 1; i < 64; i++) await assert.rejects(library.execute({ ...request, session: `playback-${i}` }), { code: 'history_unconfirmed' });
  assert.equal(library.watches.size, 64);
  await assert.rejects(library.execute({ ...request, session: 'overflow' }), { code: 'history_busy' });
  assert.equal(starts, 64); assert.equal(updates, 1);
  time = 1800001;
  await assert.rejects(library.execute({ ...request, session: 'fresh' }), { code: 'history_unconfirmed' });
  assert.equal(library.watches.size, 1);
  const wrong = new Library(async () => ({ session: { logged_in: true }, getBasicInfo: async () => ({ basic_info: { id: '12345678901' },
    addToWatchHistory: async () => { throw new Error('must not be called'); } }) }), [{ id: 'one', cookie: 'fixture' }]);
  await assert.rejects(wrong.execute(request), { code: 'history_unconfirmed' });
});

const video = index => ({ video_id: String(index).padStart(11, '0'), title: `Video ${index}`,
  author: { id: `UC${'a'.repeat(22)}`, name: 'Fixture channel' }, duration: { seconds: 8 },
  thumbnails: [{ url: 'https://i.ytimg.com/fixture.jpg' }] });
test('playlist creation explicitly requests private visibility and stable video identity', async () => {
  const calls = [];
  const client = { session: { logged_in: true }, actions: { execute: async (path, payload) => {
    calls.push([path, payload]); return { success: true, status_code: 200, data: { playlistId: 'PLabcdefghijk' } };
  } } };
  const library = new Library(async () => client, [{ id: 'one', cookie: 'fixture-cookie' }]);
  const result = await library.execute({ action: 'playlist_create', profile: 'one', title: ' New "playlist" ', id: 'abcdefghijk' });
  assert.equal(result.playlist_id, 'PLabcdefghijk');
  assert.deepEqual(calls[0], ['playlist/create', { title: 'New "playlist"', videoIds: ['abcdefghijk'], privacyStatus: 'PRIVATE' }]);
  const endpoint = new YTNodes.CreatePlaylistServiceEndpoint(calls[0][1]);
  assert.deepEqual(endpoint.buildRequest(), calls[0][1]);
});
test('playlist names preserve Unicode code points and reject invalid or oversized input', async () => {
  const calls = [];
  const library = new Library(async () => ({ session: { logged_in: true }, actions: { execute: async (_, payload) => {
    calls.push(payload); return { success: true, data: { playlistId: 'PLabcdefghijk' } };
  } }, playlist: { setName: async (id, title) => { calls.push([id, title]); return { success: true }; } } }),
  [{ id: 'one', cookie: 'fixture-cookie' }]);
  const title = '🌍'.repeat(150);
  await library.execute({ action: 'playlist_create', profile: 'one', title });
  assert.equal(calls[0].title, title); assert.deepEqual(calls[0].videoIds, []);
  await library.execute({ action: 'playlist_rename', profile: 'one', playlist: 'PLabcdefghijk', title });
  assert.deepEqual(calls[1], ['PLabcdefghijk', title]);
  for (const invalid of ['', ' ', 'x'.repeat(151), 'a\nb', 'a\0b'])
    await assert.rejects(library.execute({ action: 'playlist_create', profile: 'one', title: invalid }), { code: 'invalid_title' });
  assert.equal(calls.length, 2);
});
test('playlist creation requires a signed-in session and a confirmed new identity', async () => {
  let calls = 0, logged_in = false;
  const client = { session: { get logged_in() { return logged_in; } }, actions: { execute: async () => {
    calls++; return { success: true, data: {} };
  } } };
  const library = new Library(async () => client, [{ id: 'one', cookie: 'fixture-cookie' }]);
  await assert.rejects(library.execute({ action: 'playlist_create', profile: 'one', title: 'Name' }), { code: 'account_required' });
  assert.equal(calls, 0); logged_in = true;
  await assert.rejects(library.execute({ action: 'playlist_create', profile: 'one', title: 'Name' }), { code: 'operation_failed' });
  assert.equal(calls, 1);
});
test('pagination retains every item and scopes cursors to the profile', async () => {
  const second = { videos: [video(53)], has_continuation: false };
  const feed = { videos: Array.from({ length: 53 }, (_, i) => video(i)), has_continuation: true,
    getContinuation: async () => second };
  const library = new Library(async () => ({ getHomeFeed: async () => feed }), [{ id: 'other', name: 'Other', cookie: '' }]);
  const seen = [];
  let page = await library.execute({ action: 'home' });
  await assert.rejects(library.execute({ action: 'continue', profile: 'other', cursor: page.cursor }), { code: 'cursor_expired' });
  while (true) {
    seen.push(...page.items.map(item => item.id));
    if (!page.has_more) break;
    page = await library.execute({ action: 'continue', cursor: page.cursor });
  }
  assert.deepEqual(seen, Array.from({ length: 54 }, (_, i) => String(i).padStart(11, '0')));
});
test('expired pagination rejects without upstream calls', async () => {
  let time = 0, calls = 0;
  const library = new Library(async () => ({ getHomeFeed: async () => ({ videos: [video(1)], has_continuation: true,
    getContinuation: async () => { calls++; return {}; } }) }), [], () => time);
  const page = await library.execute({ action: 'home' }); time = 120001;
  await assert.rejects(library.execute({ action: 'continue', cursor: page.cursor }), { code: 'cursor_expired' });
  assert.equal(calls, 0);
});
test('a continuation retry returns the same page without an extra fetch', async () => {
  let calls = 0;
  const library = new Library(async () => ({ getHomeFeed: async () => ({ videos: [video(0)], has_continuation: true,
    getContinuation: async () => { calls++; return { videos: [video(1)], has_continuation: false }; } }) }));
  const first = await library.execute({ action: 'home' });
  const one = await library.execute({ action: 'continue', cursor: first.cursor });
  const two = await library.execute({ action: 'continue', cursor: first.cursor });
  assert.deepEqual(one, two); assert.equal(calls, 1);
});
test('account-only operations reject guests before opening a client', async () => {
  let calls = 0;
  const library = new Library(async () => { calls++; return {}; });
  await assert.rejects(library.execute({ action: 'subscriptions' }), { code: 'account_required' });
  await assert.rejects(library.execute({ action: 'like', id: '00000000000' }), { code: 'account_required' });
  await assert.rejects(library.execute({ action: 'invented' }), { code: 'unknown_operation' });
  assert.equal(calls, 0);
});
test('profiles never expose credentials and clients remain separate', async () => {
  const used = [];
  const library = new Library(async profile => { used.push(profile.id); return { getHomeFeed: async () => ({ videos: [] }) }; },
    [{ id: 'one', name: 'One', cookie: 'secret-one' }, { id: 'two', name: 'Two', cookie: 'secret-two' }]);
  const profiles = await library.execute({ action: 'profiles' });
  assert.equal(JSON.stringify(profiles).includes('secret'), false);
  await library.execute({ action: 'home', profile: 'one' });
  await library.execute({ action: 'home', profile: 'two' });
  await library.execute({ action: 'home', profile: 'one' });
  assert.deepEqual(used, ['one', 'two']);
});
test('mutations use the requested stable IDs and require a confirmed response', async () => {
  const calls = [];
  const client = { interact: { subscribe: async id => { calls.push(id); return { success: false }; } },
    playlist: { addVideos: async (id, videos) => { calls.push([id, videos]); return { action_result: [] }; } } };
  const library = new Library(async () => client, [{ id: 'one', name: 'One', cookie: 'cookie' }]);
  await assert.rejects(library.execute({ action: 'subscribe', profile: 'one', id: `UC${'a'.repeat(22)}` }), { code: 'operation_failed' });
  await assert.rejects(library.execute({ action: 'watch_later_add', profile: 'one', id: '00000000000' }), { code: 'operation_failed' });
  assert.deepEqual(calls, [`UC${'a'.repeat(22)}`, ['WL', ['00000000000']]]);
  await assert.rejects(library.execute({ action: 'like', profile: 'one', id: 'wrong' }), { code: 'invalid_identity' });
});
test('comment normalization returns display data and continuation', async () => {
  const library = new Library(async () => ({ getComments: async () => ({ contents: [{ comment: {
    comment_id: 'comment-1', content: { toString: () => 'Text café' }, author: { name: 'Fixture' },
    keys: { shared: 'private-implementation-token' }, is_pinned: true
  } }], has_continuation: false }) }));
  const page = await library.execute({ action: 'comments', id: '00000000000' });
  assert.equal(page.items[0].content, 'Text café'); assert.equal(page.items[0].pinned, true);
  assert.equal(JSON.stringify(page).includes('implementation-token'), false);
});
test('normalization rejects arbitrary origins and identities', () => {
  assert.equal(imageUrl('http://127.0.0.1/image'), '');
  assert.equal(imageUrl('https://i.ytimg.com.attacker.invalid/image'), '');
  assert.equal(imageUrl('https://user:password@i.ytimg.com/image'), '');
  assert.equal(normalizeVideo({ video_id: '/data/file' }), null);
  assert.equal(normalizeVideo(video(1)).duration_sec, 8);
});
test('account playlists retain a separate navigation namespace', async () => {
  const library = new Library(async () => ({ getPlaylists: async () => ({ playlists: [{ id: 'PLabcdefghijk', title: 'Library' }], has_continuation: false }) }),
    [{ id: 'one', name: 'One', cookie: 'cookie' }]);
  const page = await library.execute({ action: 'playlists', profile: 'one' });
  assert.equal(page.items[0].id, 'library:playlist:PLabcdefghijk');
  assert.equal(page.items[0].is_folder, true);
});

function lockup(kind = 'VIDEO', duration = '1:02:03') {
  const thumbnail = { thumbnailViewModel: { image: { sources: [{ url: 'https://i.ytimg.com/modern.jpg', width: 640, height: 360 }] },
    overlays: [{ thumbnailOverlayBadgeViewModel: { thumbnailBadges: [{ thumbnailBadgeViewModel: { text: duration, badgeStyle: 'THUMBNAIL_BADGE_STYLE_DEFAULT' } }] } }] } };
  return new YTNodes.LockupView({ contentId: kind === 'VIDEO' ? '00000000000' : 'PLabcdefghijk',
    contentType: `LOCKUP_CONTENT_TYPE_${kind}`,
    contentImage: kind === 'VIDEO' ? thumbnail : { collectionThumbnailViewModel: { primaryThumbnail: thumbnail } },
    metadata: { lockupMetadataViewModel: { title: { content: 'Modern café card' }, metadata: { contentMetadataViewModel: {
      metadataRows: [{ metadataParts: [{ text: { content: 'Fixture channel', commandRuns: [{ startIndex: 0, length: 15,
        onTap: { browseEndpoint: { browseId: `UC${'a'.repeat(22)}` } } }] } }] }], delimiter: ' · '
    } } } }
  });
}

test('actual pinned modern video parser retains title, channel, duration and artwork', () => {
  const item = normalizeVideo(lockup());
  assert.equal(item.title, 'Modern café card');
  assert.equal(item.subtitle, 'Fixture channel');
  assert.equal(item.channel_id, `UC${'a'.repeat(22)}`);
  assert.equal(item.duration_sec, 3723);
  assert.equal(item.art_url, 'https://i.ytimg.com/modern.jpg');
  assert.equal(normalizeVideo(lockup('PLAYLIST')), null);
  assert.equal(normalizeVideo(lockup('VIDEO', '1:99')).duration_sec, 0);
});

test('actual pinned collection parser retains account playlist title and artwork', async () => {
  const library = new Library(async () => ({ getPlaylists: async () => ({ playlists: [lockup('PLAYLIST')], has_continuation: false }) }),
    [{ id: 'one', name: 'One', cookie: 'cookie' }]);
  const page = await library.execute({ action: 'playlists', profile: 'one' });
  assert.equal(page.items[0].title, 'Modern café card');
  assert.equal(page.items[0].art_url, 'https://i.ytimg.com/modern.jpg');
  assert.equal(page.items[0].id, 'library:playlist:PLabcdefghijk');
});

test('explicit promoted records are excluded without matching localized title text', () => {
  assert.equal(normalizeVideo({ ...video(1), is_ad: true }), null);
  assert.equal(normalizeVideo({ ...video(1), is_promoted: true }), null);
  assert.equal(normalizeVideo({ ...video(1), title: 'Advertising explained' }).title, 'Advertising explained');
});

test('reply browsing retains all pages, scopes references and caches retries', async () => {
  let calls = 0;
  const reply = index => ({ comment: { comment_id: `reply-${index}`, content: `Reply ${index}` } });
  const replyFeed = { replies: Array.from({ length: 30 }, (_, i) => reply(i)), has_continuation: true,
    getContinuation: async () => ({ replies: [reply(30)], has_continuation: false }) };
  const thread = { comment: { comment_id: 'parent', content: 'Parent', keys: { shared: 'never-export' } },
    has_replies: true, getReplies: async () => { calls++; return replyFeed; } };
  const library = new Library(async () => ({ getComments: async () => ({ contents: [thread], has_continuation: false }) }),
    [{ id: 'other', name: 'Other', cookie: '' }]);
  const first = await library.execute({ action: 'comments', id: '00000000000' });
  const cursor = first.items[0].reply_cursor;
  assert.ok(cursor); assert.equal(JSON.stringify(first).includes('never-export'), false);
  await assert.rejects(library.execute({ action: 'replies', profile: 'other', cursor }), { code: 'cursor_expired' });
  const start = await library.execute({ action: 'replies', cursor });
  assert.deepEqual(await library.execute({ action: 'replies', cursor }), start);
  const seen = [...start.items.map(item => item.id)];
  let page = start;
  while (page.has_more) {
    page = await library.execute({ action: 'continue', cursor: page.cursor });
    seen.push(...page.items.map(item => item.id));
  }
  assert.equal(calls, 1); assert.deepEqual(seen, Array.from({ length: 31 }, (_, i) => `reply-${i}`));
});

test('expired or evicted reply references never call the upstream thread', async () => {
  let time = 0, calls = 0;
  const make = index => ({ comment: { comment_id: `parent-${index}`, content: 'Parent' }, has_replies: true,
    getReplies: async () => { calls++; return { replies: [], has_continuation: false }; } });
  const library = new Library(async () => ({ getComments: async () => ({ contents: Array.from({ length: 24 }, (_, i) => make(i)), has_continuation: false }) }), [], () => time);
  const first = await library.execute({ action: 'comments', id: '00000000000' });
  for (let i = 0; i < 11; i++) await library.execute({ action: 'comments', id: '00000000000' });
  assert.equal(library.threads.size, 256);
  await assert.rejects(library.execute({ action: 'replies', cursor: first.items[0].reply_cursor }), { code: 'cursor_expired' });
  const newest = await library.execute({ action: 'comments', id: '00000000000' }); time = 120001;
  await assert.rejects(library.execute({ action: 'replies', cursor: newest.items[0].reply_cursor }), { code: 'cursor_expired' });
  assert.equal(calls, 0);
});
