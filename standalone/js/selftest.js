// Offline checks of the engine, the shims and the pure parts of the service,
// answered from canned responses (TUBE_JS_FIXTURES). The HTTP routes are
// tested from outside by test/service_test.py. SPDX-License-Identifier: GPL-3.0-or-later
import { Player, Platform } from 'youtubei.js';
import { createHash } from './node_crypto.js';
import { catalog } from './core.js';
import { branding, thumbnails } from './extras.js';
import { chaptersOf, codecName, dashManifest, hlsVariants, redact, selectAudio, selectVideo, vttToSrt } from './resolve.js';
import { Invalid, jobRequest, libraryRequest, resolveRequest } from './schema.js';
import { cueSegments, evenSegments, sidxSegments } from './segments.js';

const host = globalThis.__host;

// A stand-in for the object the real player script builds around an address:
// methods on its prototype rewrite `n` and the signature.
const syntheticPlayer = `
class Address {
  constructor(url, sp, s) { this.sp = sp; this.values = new Map([['n', decodeURIComponent(/[?&]n=([^&]*)/.exec(url)[1])]]); if (sp) this.values.set(sp, s); }
  get(name) { return this.values.get(name); }
  set(name, value) { this.values.set(name, value); }
  transform() {
    this.values.set('n', this.values.get('n').split('').reverse().join(''));
    if (this.sp) this.values.set(this.sp, this.values.get(this.sp).toUpperCase());
  }
}
const exportedVars = { nsigFunction: (url, sp, s) => new Address(url, sp, s) };`;

// What companion/prosperotube/sessions.py, dash_manifest, prints for the same input.
const PYTHON_MANIFEST = `<?xml version='1.0' encoding='utf-8'?>\n<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" type="static" mediaPresentationDuration="PT635.200S" minBufferTime="PT1S" profiles="urn:mpeg:dash:profile:isoff-on-demand:2011"><Period duration="PT635.200S"><AdaptationSet contentType="video" mimeType="video/mp4" codecs="avc1.64002a"><Representation id="video" bandwidth="3248500" width="1920" height="1080"><BaseURL>https://v.example/x?a=1&amp;b=&lt;2&gt;"</BaseURL><SegmentList duration="635200" timescale="1000"><SegmentURL media="https://v.example/x?a=1&amp;b=&lt;2&gt;&quot;" /></SegmentList></Representation></AdaptationSet><AdaptationSet contentType="audio" mimeType="audio/mp4" codecs="mp4a.40.2" lang="en"><Representation id="audio" bandwidth="129500" audioSamplingRate="44100"><BaseURL>https://a.example/y?c=3</BaseURL><SegmentList duration="635200" timescale="1000"><SegmentURL media="https://a.example/y?c=3" /></SegmentList></Representation></AdaptationSet></Period></MPD>`;

export async function selfTest() {
  const failed = [], passed = [];
  const check = async (name, work) => {
    try { if (await work() === false) throw new Error('unexpected result'); passed.push(name); }
    catch (error) { failed.push(`${name}: ${error?.message ?? error}`); }
  };
  const same = (actual, expected) => { if (JSON.stringify(actual) !== JSON.stringify(expected)) throw new Error(`got ${JSON.stringify(actual)}`); };
  const refused = work => { try { work(); return false; } catch (error) { return error instanceof Invalid; } };

  await check('engine has no host modules', () => typeof std === 'undefined' && typeof os === 'undefined');
  await check('sha1', () => host.sha1('abc') === 'a9993e364706816aba3e25717850c26c9cd0d89d');
  await check('sha256', () => createHash('sha256').update('abc').digest('hex') === 'ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad');
  await check('utf-8 round trip', () => new TextDecoder().decode(new TextEncoder().encode('zoo – 動物園 🐘')) === 'zoo – 動物園 🐘');
  await check('base64', () => btoa('Me at the zoo') === 'TWUgYXQgdGhlIHpvbw==' && atob('TWUgYXQgdGhlIHpvbw==') === 'Me at the zoo');
  await check('random', () => /^[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$/.test(crypto.randomUUID()) && crypto.randomUUID() !== crypto.randomUUID());
  await check('timers', async () => {
    const order = [];
    let ticks = 0;
    await new Promise(done => {
      setTimeout(() => { order.push('late'); done(); }, 40);
      setTimeout(() => order.push('early'), 5);
      clearTimeout(setTimeout(() => order.push('never'), 1));
      const repeat = setInterval(() => { if (++ticks === 2) clearInterval(repeat); }, 5);
    });
    same([order, ticks], [['early', 'late'], 2]);
  });
  await check('structuredClone', () => { const source = { a: [1, { b: new Date(5) }], m: new Map([[1, 2]]) }; const copy = structuredClone(source);
    return copy !== source && copy.a[1] !== source.a[1] && copy.a[1].b.getTime() === 5 && copy.m.get(1) === 2; });
  await check('URL parsing', () => {
    const url = new URL('/youtubei/v1/search?prettyPrint=false#top', 'https://www.youtube.com/watch?v=x');
    same([url.href, url.origin, url.hostname, url.pathname, url.search, url.hash],
      ['https://www.youtube.com/youtubei/v1/search?prettyPrint=false#top', 'https://www.youtube.com', 'www.youtube.com', '/youtubei/v1/search', '?prettyPrint=false', '#top']);
    same(new URL('../b/./c?x=1', 'https://h.example/a/d/e').href, 'https://h.example/a/b/c?x=1');
    same(new URL('HTTPS://Example.COM:443/a b?q=ü').href, 'https://example.com/a%20b?q=%C3%BC');
  });
  await check('URL search parameters', () => {
    const url = new URL('https://rr1---sn-a.googlevideo.com/videoplayback?expire=1&n=abc&sig=a%2Bb&n=dup');
    url.searchParams.set('n', 'x y/z'); url.searchParams.append('cver', '1.0');
    same(url.href, 'https://rr1---sn-a.googlevideo.com/videoplayback?expire=1&n=x+y%2Fz&sig=a%2Bb&cver=1.0');
    same(new URLSearchParams('s=a%3Db&sp=sig&url=https%3A%2F%2Fh%2Fp%3Fa%3D1').get('url'), 'https://h/p?a=1');
    same(new URLSearchParams({ a: '1', b: 'two words' }).toString(), 'a=1&b=two+words');
  });
  await check('player code runs without the host object', async () =>
    host.evalIsolated('typeof __host + typeof fetch') === 'undefinedundefined' &&
    (await Platform.shim.eval({ output: 'return { n: "ab".repeat(2) }' })).n === 'abab');
  await check('address transform through the platform evaluator', async () => {
    const player = new Player('selftest', 20000, { output: syntheticPlayer });
    const plain = new URL(await player.decipher('https://rr1---sn-a.googlevideo.com/videoplayback?c=TVHTML5&n=abc123'));
    const signed = new URL(await player.decipher(undefined, 's=xyz&sp=sig&url=https%3A%2F%2Frr1---sn-a.googlevideo.com%2Fvideoplayback%3Fn%3Dabc'));
    same([plain.searchParams.get('n'), plain.searchParams.has('cver'), signed.searchParams.get('n'), signed.searchParams.get('sig')], ['321cba', true, 'cba', 'XYZ']);
  });
  await check('format policy', () => {
    const format = (itag, mime, height, fps, bitrate, extra = {}) => ({ itag, mime_type: mime, height, width: height * 16 / 9, fps, bitrate, url: 'https://x', has_video: height > 0, has_audio: height === 0, ...extra });
    const list = [format(399, 'video/mp4; codecs="av01.0.08M.08"', 1080, 30, 2e6), format(137, 'video/mp4; codecs="avc1.640028"', 1080, 30, 4e6),
      format(248, 'video/webm; codecs="vp9"', 1080, 30, 3e6), format(313, 'video/webm; codecs="vp9"', 2160, 30, 9e6),
      format(299, 'video/mp4; codecs="avc1.64002a"', 1080, 60, 6e6), format(337, 'video/webm; codecs="vp09.02.51.10"', 2160, 60, 2e7, { quality_label: '2160p60 HDR' }),
      format(140, 'audio/mp4; codecs="mp4a.40.2"', 0, 0, 128e3), format(141, 'audio/mp4; codecs="mp4a.40.2"', 0, 0, 128e3, { is_drc: true, itag: 141 }),
      format(251, 'audio/webm; codecs="opus"', 0, 0, 160e3), format(999, 'audio/mp4; codecs="mp4a.40.2"', 0, 0, 96e3, { language: 'fr', is_original: false })];
    same(selectVideo(list, { max_height: 1080 }).map(row => row.itag), [299, 137, 248, 399]);
    same(selectVideo(list, { max_height: 1080, max_fps: 30 }).map(row => row.itag), [137, 248, 399]);
    same(selectVideo(list, { max_height: 2160, codec: 'vp9' }).map(row => row.itag), [313, 248]);
    same(selectVideo(list, { max_height: 2160, codec: 'hevc' }), []);
    same(selectAudio(list).map(row => row.itag), [140, 251, 141, 999]);
    same(selectAudio(list, { audio_language: 'fr' })[0].itag, 999);
    same(['hvc1.2.4.L120', 'avc1.64002a,mp4a.40.2', 'vp09.00.40.08', 'av01.0.09M.08', 'opus', 'ec-3'].map(codecName), ['hevc', 'h264', 'vp9', 'av1', 'opus', '']);
  });
  await check('manifest equals the Python generator', () => {
    const video = { mime_type: 'video/mp4; codecs="avc1.64002a"', width: 1920, height: 1080, average_bitrate: 3248500, bitrate: 6e6 };
    const audio = { mime_type: 'audio/mp4; codecs="mp4a.40.2"', audio_sample_rate: 44100, average_bitrate: 129500, language: 'en' };
    same(dashManifest(635.2, video, audio, 'https://v.example/x?a=1&b=<2>"', 'https://a.example/y?c=3'), PYTHON_MANIFEST);
  });
  await check('captions, chapters, playlists', () => {
    same(vttToSrt('WEBVTT\n\n00:01.000 --> 00:04.000 align:start\n<c>All right</c>, so &amp; here\n\n00:00:04.000 --> 00:00:06.500\n\n'),
      '1\n00:00:01,000 --> 00:00:04,000\nAll right, so & here\n');
    same(chaptersOf('Intro text\n0:00 Start\n1:30 - Middle\n[1:02:03] End\n99:99:99 nothing', 4000),
      [{ start_time: 0, title: 'Start', end_time: 90 }, { start_time: 90, title: 'Middle', end_time: 3723 }, { start_time: 3723, title: 'End', end_time: 4000 }]);
    same(chaptersOf('1:00 not from zero\n2:00 two', 300), []);
    const variants = hlsVariants('#EXTM3U\n#EXT-X-MEDIA:URI="a/audio.m3u8",TYPE=AUDIO,GROUP-ID="g"\n#EXT-X-STREAM-INF:BANDWIDTH=5,CODECS="avc1.640028,mp4a.40.2",RESOLUTION=1920x1080,FRAME-RATE=60,AUDIO="g"\nv/video.m3u8\n',
      'https://manifest.googlevideo.com/api/master.m3u8');
    same([variants.length, variants[0].height, variants[0].fps, variants[0].video_codec, variants[0].audio_codec, variants[0].url],
      [1, 1080, 60, 'h264', 'aac', 'https://manifest.googlevideo.com/api/v/video.m3u8']);
    if (!variants[0].master.includes('URI="https://manifest.googlevideo.com/api/a/audio.m3u8"')) return false;
    return !redact('https://rr1---sn-a.googlevideo.com/videoplayback?expire=99&sig=SECRET').includes('SECRET');
  });
  await check('segment indexes', () => {
    // sidx, version 0: timescale 1000, three fragments.
    const sidx = new Uint8Array(68), data = new DataView(sidx.buffer);
    data.setUint32(0, 68); sidx.set([0x73, 0x69, 0x64, 0x78], 4); data.setUint32(16, 1000); data.setUint16(30, 3);
    [[1000, 4000], [2000, 6000], [1500, 5000]].forEach(([size, duration], at) => { data.setUint32(32 + at * 12, size); data.setUint32(36 + at * 12, duration); });
    const index = sidxSegments(sidx, 741);
    same(index, { timescale: 1000, segments: [{ start: 809, end: 1808, duration: 4000 }, { start: 1809, end: 3808, duration: 6000 }, { start: 3809, end: 5308, duration: 5000 }] });
    // Grouping on a grid of the longest fragment: the segment for time t starts at or before t.
    const uneven = { segments: [2, 2, 2, 10, 3, 3, 3, 3, 9, 1].map((duration, at) => ({ start: at * 100, end: at * 100 + 99, duration })) };
    const { step, segments } = evenSegments(uneven);
    same([step, segments.map(segment => segment.start)], [10, [0, 300, 500, 800]]);
    const starts = []; let time = 0;
    for (const fragment of uneven.segments) { starts.push([fragment.start, time]); time += fragment.duration; }
    for (let t = 0; t < time; t += 0.5) {
      const begins = starts.find(([start]) => start === segments[Math.floor(t / step)].start)[1];
      if (begins > t || t - begins > 2 * step) throw new Error(`seek to ${t} opens a segment that begins at ${begins}`);
    }
    // Matroska: EBML header, Segment with Info (tick 1 ms), and Cues naming two clusters.
    const head = Uint8Array.of(0x1a, 0x45, 0xdf, 0xa3, 0x80, 0x18, 0x53, 0x80, 0x67, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0x15, 0x49, 0xa9, 0x66, 0x87, 0x2a, 0xd7, 0xb1, 0x83, 0x0f, 0x42, 0x40);
    const point = (ms, position) => [0xbb, 0x8d, 0xb3, 0x82, ms >> 8, ms & 255, 0xb7, 0x87, 0xf7, 0x81, 1, 0xf1, 0x82, position >> 8, position & 255];
    const cues = Uint8Array.of(0x1c, 0x53, 0xbb, 0x6b, 0x9e, ...point(0, 500), ...point(5000, 9000));
    same(cueSegments(head, cues, 20000, 12), { timescale: 1000, segments: [{ start: 517, end: 9016, duration: 5000 }, { start: 9017, end: 19999, duration: 7000 }] });
  });
  await check('request models', () => {
    same(libraryRequest({ action: 'search', query: 'x' }).sort, 'top');
    same(Object.keys(libraryRequest({ action: 'home' })).length, 43);
    same(resolveRequest({ id: 'jNQXAC9IVRw' }), { id: 'jNQXAC9IVRw', profile: 'guest', max_height: 1080, max_fps: 0, codec: 'auto',
      caption_language: 'en', audio_language: '', automatic_captions: false, translated_captions: false, fresh: false, hl: '' });
    same(jobRequest({ action: 'search', query: 'x', unknown: 1 }).page, 0);
    return refused(() => libraryRequest({ action: 'search' })) && refused(() => libraryRequest({ action: 'home', extra: 1 })) &&
      refused(() => libraryRequest({ action: 'channel', channel: 'nope' })) && refused(() => libraryRequest({ action: 'home', hl: 'xx' })) &&
      refused(() => libraryRequest({ action: 'search', query: 'x', hide: ['mixes', 'mixes'] })) && refused(() => resolveRequest({ id: 'short' })) &&
      refused(() => resolveRequest({ id: 'jNQXAC9IVRw', max_height: 100 })) && refused(() => jobRequest({ action: 'delete' })) &&
      refused(() => jobRequest({ action: 'search', page: 1001 }));
  });
  await check('thumbnail frames and DeArrow rules', () => {
    const page = { kind: 'catalog', items: [{ id: 'jNQXAC9IVRw', art_url: 'https://i.ytimg.com/vi/jNQXAC9IVRw/hq720.jpg?sqp=x' },
      { id: 'AAAAAAAAAAA', art_url: 'https://i.ytimg.com/vi/AAAAAAAAAAA/x.jpg', is_live: true }, { id: 'library:playlist:PLx', is_folder: true, art_url: 'https://i.ytimg.com/a.jpg' }] };
    same(thumbnails(page, 'middle').items.map(row => row.art_url), ['https://i.ytimg.com/vi/jNQXAC9IVRw/hq2.jpg', 'https://i.ytimg.com/vi/AAAAAAAAAAA/x.jpg', 'https://i.ytimg.com/a.jpg']);
    same(thumbnails(page, 'default'), page);
    same(branding({ titles: [{ title: 'original', original: true, votes: 9 }, { title: ' A  > better\ttitle ', original: false, votes: 0 }], thumbnails: [{ timestamp: 3, original: false, votes: -1 }] }),
      { title: 'A better title', stamp: null });
  });
  // The library adapter, the youtubei.js session and its search parser, fed
  // from the canned InnerTube answers.
  await check('search parser on a canned response', async () => {
    const page = await catalog().execute({ action: 'search', query: 'me at the zoo', profile: 'guest' });
    same(page.items.slice(0, 2).map(item => [item.id, item.title, item.subtitle, item.channel_id, item.duration_sec, item.art_url]),
      [['jNQXAC9IVRw', 'Me at the zoo', 'jawed', 'UC4QobU6STFB0P71PMvOGN5A', 19, 'https://i.ytimg.com/vi/jNQXAC9IVRw/hq720.jpg'],
        ['AAAAAAAAAAA', 'Second canned row', 'Canned channel', 'UCaaaaaaaaaaaaaaaaaaaaaa', 3723, '']]);
    same([page.api_version, page.kind, page.items.length, page.has_more], [1, 'catalog', 24, true]);
  });
  await check('unknown origins are refused', async () => {
    try { await fetch('https://example.org/'); return false; } catch (error) { return /no canned response/.test(error.message); }
  });
  return { ok: !failed.length, passed: passed.length, failed };
}
