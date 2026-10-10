"""Shipping live root renewal must retain paired capabilities and scoped ownership."""
import asyncio
import copy
import re

import httpx
import pytest
from fastapi.testclient import TestClient

from prosperotube.app import create_app
from prosperotube.hls import rewrite
from prosperotube.media_refresh import renew
from prosperotube.models import ResolveRequest, ServiceError
from prosperotube.sessions import Sessions
from test_live_relay import split_live_info
from prosperotube.live_hls import apply_master
from test_companion import Chunks


def live_info(fresh=False):
    info, master, content = split_live_info()
    apply_master(info, master, content)
    info['id'] = 'abcdefghijk'
    info['http_headers'] = {'Cookie': 'fresh-profile-cookie' if fresh else 'old-profile-cookie'}
    for i, fmt in enumerate(info['formats']):
        fmt['format_id'] = 'live-' + str(i)
    if fresh:
        for fmt in info['formats']:
            fmt['url'] += '?generation=2'
            fmt['manifest_url'] += '?generation=2'
            if fmt.get('hls_audio_urls'):
                fmt['hls_audio_urls'] = [url + '?generation=2' for url in fmt['hls_audio_urls']]
    return info


class LiveExtractor:
    def __init__(self, change=None): self.calls, self.change = [], change
    async def extract(self, target, **kwargs):
        self.calls.append(kwargs)
        info = live_info(kwargs.get('fresh', False))
        if kwargs.get('fresh') and self.change: self.change(info)
        return info


@pytest.mark.parametrize('method', ['GET', 'HEAD'])
def test_shipping_live_roots_renew_pair_with_stable_manifest_and_new_scoped_headers(method):
    extractor = LiveExtractor()
    requests = []
    def handler(request):
        requests.append(request)
        if 'generation=2' not in str(request.url): return httpx.Response(403)
        assert request.headers['cookie'] == 'fresh-profile-cookie'
        assert 'range' not in request.headers and 'if-range' not in request.headers
        return httpx.Response(200, headers={'content-type': 'application/vnd.apple.mpegurl'},
                              stream=Chunks(b'#EXTM3U\n#EXT-X-MEDIA-SEQUENCE:55\n#EXTINF:2,\nsegment.ts\n'))
    transport = httpx.AsyncClient(transport=httpx.MockTransport(handler))
    app = create_app(extractor=extractor, token='test-token', http_client=transport)
    auth = {'Authorization': 'Bearer test-token'}
    with TestClient(app) as client:
        playback = client.post('/v1/resolve', headers=auth, json={'id': 'abcdefghijk'}).json()
        master = client.get(playback['choices'][0]['url']).text
        session = next(iter(app.state.sessions.sessions.values()))
        for key in ('h0v.m3u8', 'h0a.m3u8'):
            response = client.request(method, '/media/' + session.token + '/' + key,
                                      headers={'Range': 'bytes=0-'})
            assert response.status_code == 200, response.text
            if method == 'GET':
                assert 'MEDIA-SEQUENCE:55' in response.text and 'googlevideo' not in response.text
                segment = re.search(r'/media/[^\s]+/hls-[^\s]+', response.text).group()
                assert session.tracks[segment.rsplit('/', 1)[-1]].headers['Cookie'] == 'fresh-profile-cookie'
        assert extractor.calls == [{'fresh': False}, {'fresh': True}]
        assert client.get(playback['choices'][0]['url']).text == master
        assert all('?generation=2' in session.tracks[k].url for k in ('h0v.m3u8', 'h0a.m3u8'))


@pytest.mark.parametrize('change', [
    lambda i: i.update(id='wrongvideo1'),
    lambda i: i.update(is_live=False),
    lambda i: i['formats'].pop(),
    lambda i: i['formats'].append(copy.deepcopy(i['formats'][0])),
    lambda i: i['formats'][0].update(height=360),
    lambda i: i['formats'][0].update(hls_audio_urls=[]),
    lambda i: i['formats'][1].update(manifest_url='https://r1.googlevideo.com/other.m3u8'),
])
def test_live_refresh_rejects_changed_missing_ambiguous_or_unpaired_roots_atomically(change):
    async def run():
        sessions = Sessions()
        sessions.create(live_info(), ResolveRequest(id='abcdefghijk'), 'http://local')
        session = next(iter(sessions.sessions.values()))
        old = dict(session.tracks)
        extractor = LiveExtractor(change)
        with pytest.raises(ServiceError):
            await renew(sessions, session, 'h0v.m3u8', old['h0v.m3u8'], lambda _: extractor)
        assert session.tracks == old
    asyncio.run(run())


def test_two_expired_live_roots_share_one_resolution_and_old_response_cannot_restore_old_url():
    async def run():
        sessions = Sessions()
        sessions.create(live_info(), ResolveRequest(id='abcdefghijk'), 'http://local')
        session = next(iter(sessions.sessions.values()))
        old = dict(session.tracks); extractor = LiveExtractor()
        results = await asyncio.gather(*(renew(sessions, session, key, old[key], lambda _: extractor)
                                        for key in ('h0v.m3u8', 'h0a.m3u8')))
        assert len(extractor.calls) == 1
        new = session.tracks['h0v.m3u8']
        assert all('generation=2' in result.url for result in results)
        rewrite('#EXTM3U\nold.ts\n', old['h0v.m3u8'].url, session, 'http://local', 'h0v.m3u8', old['h0v.m3u8'])
        assert session.tracks['h0v.m3u8'] is new
        with pytest.raises(ServiceError):
            await renew(sessions, session, 'h0v.m3u8', new, lambda _: extractor)
        assert len(extractor.calls) == 1
    asyncio.run(run())


def test_segments_cannot_be_remapped_by_live_root_renewal():
    async def run():
        sessions = Sessions()
        sessions.create(live_info(), ResolveRequest(id='abcdefghijk'), 'http://local')
        session = next(iter(sessions.sessions.values())); root = session.tracks['h0v.m3u8']
        content = rewrite('#EXTM3U\nold.ts\n', root.url, session, 'http://local', 'h0v.m3u8', root)
        key = content.splitlines()[-1].rsplit('/', 1)[-1]
        extractor = LiveExtractor()
        with pytest.raises(ServiceError): await renew(sessions, session, key, session.tracks[key], lambda _: extractor)
        assert not extractor.calls
    asyncio.run(run())


@pytest.mark.parametrize('cancel', [False, True])
def test_live_resolution_cannot_resurrect_removed_profile_session_or_leave_lock_held(cancel):
    async def run():
        sessions = Sessions()
        sessions.create(live_info(), ResolveRequest(id='abcdefghijk', profile='owned'), 'http://local')
        session = next(iter(sessions.sessions.values())); old = dict(session.tracks)
        entered, release = asyncio.Event(), asyncio.Event()
        owners = []
        class Pending:
            async def extract(self, target, **kwargs):
                assert kwargs == {'fresh': True}
                entered.set(); await release.wait(); return live_info(True)
        def scoped(profile): owners.append(profile); return Pending()
        task = asyncio.create_task(renew(sessions, session, 'h0v.m3u8', old['h0v.m3u8'], scoped))
        await entered.wait()
        if cancel:
            task.cancel()
            with pytest.raises(asyncio.CancelledError): await task
        else:
            sessions.sessions.pop(session.token); release.set()
            with pytest.raises(ServiceError): await task
            assert session.token not in sessions.sessions
        assert owners and set(owners) == {'owned'}
        assert not session.refresh_lock.locked() and session.tracks == old
    asyncio.run(run())
