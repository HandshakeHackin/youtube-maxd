// Offline checks of the engine, the shims and one parser path, answered from
// canned responses (TUBE_JS_FIXTURES). SPDX-License-Identifier: GPL-3.0-or-later
import { Player, Platform } from 'youtubei.js';
import { createHash } from './node_crypto.js';
import { codecName, dashManifest, redact, selectAudio, selectVideo, vttToSrt } from './resolve.js';

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

export async function selfTest({ catalog }) {
  const failed = [], passed = [];
  const check = async (name, work) => {
    try { if (await work() === false) throw new Error('unexpected result'); passed.push(name); }
    catch (error) { failed.push(`${name}: ${error?.message ?? error}`); }
  };
  const same = (actual, expected) => { if (JSON.stringify(actual) !== JSON.stringify(expected)) throw new Error(`got ${JSON.stringify(actual)}`); };

  await check('engine has no host modules', () => typeof std === 'undefined' && typeof os === 'undefined');
  await check('sha1', () => host.sha1('abc') === 'a9993e364706816aba3e25717850c26c9cd0d89d');
  await check('sha256', () => createHash('sha256').update('abc').digest('hex') === 'ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad');
  await check('utf-8 round trip', () => new TextDecoder().decode(new TextEncoder().encode('zoo – 動物園 🐘')) === 'zoo – 動物園 🐘');
  await check('base64', () => btoa('Me at the zoo') === 'TWUgYXQgdGhlIHpvbw==' && atob('TWUgYXQgdGhlIHpvbw==') === 'Me at the zoo');
  await check('random', () => /^[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$/.test(crypto.randomUUID()) && crypto.randomUUID() !== crypto.randomUUID());
  await check('timers', async () => {
    const order = [];
    await new Promise(done => { setTimeout(() => { order.push('late'); done(); }, 20); setTimeout(() => order.push('early'), 5); const never = setTimeout(() => order.push('never'), 1); clearTimeout(never); });
    same(order, ['early', 'late']);
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
      format(140, 'audio/mp4; codecs="mp4a.40.2"', 0, 0, 128e3), format(251, 'audio/webm; codecs="opus"', 0, 0, 160e3)];
    same(selectVideo(list, { max_height: 1080 }).map(row => row.itag), [137, 248, 399]);
    same(selectVideo(list, { max_height: 2160, codec: 'vp9' }).map(row => row.itag), [313, 248]);
    same(selectAudio(list).map(row => row.itag), [140, 251]);
    same(codecName('video/mp4; codecs="hvc1.2.4.L120"'), 'hevc');
  });
  await check('manifest and captions', () => {
    const video = { mime_type: 'video/mp4; codecs="avc1.640028"', width: 1920, height: 1080, bitrate: 4e6 };
    const audio = { mime_type: 'audio/mp4; codecs="mp4a.40.2"', audio_sample_rate: 44100, bitrate: 128e3, language: 'en' };
    const manifest = dashManifest(19.2, video, audio, 'https://v/?a=1&b=2', 'https://a/');
    if (!manifest.includes('mediaPresentationDuration="PT19.200S"') || !manifest.includes('<BaseURL>https://v/?a=1&amp;b=2</BaseURL>') || !manifest.includes('lang="en"')) return false;
    same(vttToSrt('WEBVTT\n\n00:01.000 --> 00:04.000 align:start\n<c>All right</c>, so &amp; here\n\n00:00:04.000 --> 00:00:06.500\n\n'),
      '1\n00:00:01,000 --> 00:00:04,000\nAll right, so & here\n');
    return !redact('https://rr1---sn-a.googlevideo.com/videoplayback?expire=99&sig=SECRET').includes('SECRET');
  });
  // The library adapter, the youtubei.js session and its search parser, fed
  // from the canned InnerTube answers.
  await check('search parser on a canned response', async () => {
    const page = await catalog().execute({ action: 'search', query: 'me at the zoo', profile: 'guest' });
    same(page.items.map(item => [item.id, item.title, item.subtitle, item.channel_id, item.duration_sec, item.art_url]),
      [['jNQXAC9IVRw', 'Me at the zoo', 'jawed', 'UC4QobU6STFB0P71PMvOGN5A', 19, 'https://i.ytimg.com/vi/jNQXAC9IVRw/hq720.jpg'],
        ['AAAAAAAAAAA', 'Second canned row', 'Canned channel', 'UCaaaaaaaaaaaaaaaaaaaaaa', 3723, '']]);
    same([page.api_version, page.kind, page.has_more], [1, 'catalog', false]);
  });
  await check('unknown origins are refused', async () => {
    try { await fetch('https://example.org/'); return false; } catch (error) { return /no canned response/.test(error.message); }
  });
  return { ok: !failed.length, passed: passed.length, failed };
}
