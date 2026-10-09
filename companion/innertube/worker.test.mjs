// Original behavior/failure tests; no network or account use. GPL-3.0-or-later
import test from 'node:test';
import assert from 'node:assert/strict';
import { Library, normalizeVideo, imageUrl } from './library.mjs';
import { YTNodes } from 'youtubei.js';

const video = index => ({ video_id: String(index).padStart(11, '0'), title: `Video ${index}`,
  author: { id: `UC${'a'.repeat(22)}`, name: 'Fixture channel' }, duration: { seconds: 8 },
  thumbnails: [{ url: 'https://i.ytimg.com/fixture.jpg' }] });
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
