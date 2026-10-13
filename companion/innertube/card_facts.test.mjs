// What a video row says beyond its fixed fields, for the Afterglow interface.
import assert from 'node:assert/strict';
import test from 'node:test';
import {normalizeVideo} from './library.mjs';

const channel = 'UC' + 'a'.repeat(22);
const card = extra => ({video_id: 'abcdefghijk', title: 'Fixture', author: {name: 'Fixture channel', id: channel,
  thumbnails: [{url: 'https://yt3.ggpht.com/fixture=s88'}]}, thumbnails: [{url: 'https://i.ytimg.com/vi/abcdefghijk/hq720.jpg'}],
  duration: {seconds: 61}, ...extra});

test('a card names its views, age, channel picture and badges when it has them', () => {
  const row = normalizeVideo(card({short_view_count: '1.2M views', published: '3 weeks ago',
    badges: [{label: '4K'}, {label: 'CC'}, {label: 'New'}, {label: 'Something else'}]}));
  assert.equal(row.views, '1.2M views');
  assert.equal(row.published, '3 weeks ago');
  assert.equal(row.channel_art_url, 'https://yt3.ggpht.com/fixture=s88');
  assert.deepEqual(row.badges, ['new', '4k', 'cc']);
});

test('a card without them carries none of the names', () => {
  const row = normalizeVideo({video_id: 'abcdefghijk', title: 'Fixture', author: {name: 'Fixture channel', id: channel}});
  for (const name of ['views', 'published', 'channel_art_url', 'badges']) assert.ok(!(name in row), name);
});

test('the full count stands in for the short one, and a picture from elsewhere is refused', () => {
  const row = normalizeVideo(card({view_count: '1,204,331 views', author: {name: 'Fixture channel', id: channel,
    thumbnails: [{url: 'https://example.com/avatar.jpg'}]}}));
  assert.equal(row.views, '1,204,331 views');
  assert.ok(!('channel_art_url' in row));
});

test('a lockup card reads the second line of its metadata, and only when it starts with a count', () => {
  const lockup = second => ({content_id: 'abcdefghijk', content_type: 'VIDEO', metadata: {title: 'Fixture', metadata: {metadata_rows: [
    {metadata_parts: [{text: {toString: () => 'Fixture channel', endpoint: {payload: {browseId: channel}}}}]},
    {metadata_parts: second}]}}});
  const row = normalizeVideo(lockup([{text: '842K views'}, {text: '5 days ago'}]));
  assert.equal(row.views, '842K views');
  assert.equal(row.published, '5 days ago');
  const other = normalizeVideo(lockup([{text: 'Playlist'}, {text: 'Updated today'}]));
  assert.ok(!('views' in other) && !('published' in other));
});
