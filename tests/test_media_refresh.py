"""Expired VOD links must renew once without changing byte identity or account."""
import asyncio
import copy

from fastapi.testclient import TestClient
import httpx
import pytest

from prosperotube.media_refresh import entity, observe, renew
from prosperotube.models import ResolveRequest, ServiceError
from prosperotube.sessions import Sessions
from test_companion import app, AUTH, Chunks, ID, info


class RotatingExtractor:
    def __init__(self, change=None):
        self.calls = []
        self.change = change

    async def extract(self, target, **kwargs):
        self.calls.append(kwargs)
        value = info()
        if kwargs.get('fresh'):
            for fmt in value['formats']:
                fmt['url'] += '&renewed=1' if '?' in fmt['url'] else '?renewed=1'
            value['http_headers']['Cookie'] = 'fresh-owned-cookie'
            if self.change:
                self.change(value)
        return value


def track(client, playback, ending='v'):
    from xml.etree.ElementTree import fromstring
    manifest = client.get(playback['choices'][0]['url']).text
    return next(node.attrib['media'] for node in fromstring(manifest).iter()
                if node.tag.endswith('SegmentURL') and node.attrib['media'].endswith(ending))


@pytest.mark.parametrize('validator', ['etag', 'modified', 'first_open', 'obs_tag'])
def test_shipping_api_renews_expired_ranges_and_dash_pair_using_fresh_resolver(validator):
    extractor = RotatingExtractor()
    requests = []
    def handler(request):
        requests.append(request)
        renewed = 'renewed=1' in str(request.url)
        if not renewed and (validator == 'first_open' or request.headers.get('range') == 'bytes=4-7'):
            return httpx.Response(403)
        headers = {'content-range': 'bytes 4-7/20' if request.headers.get('range') == 'bytes=4-7' else 'bytes 0-3/20'}
        if validator == 'etag':
            headers['etag'] = '"same-representation"'
        if validator == 'obs_tag':
            headers['etag'] = b'"caf\xe9"'
        if validator == 'modified':
            headers.update({'last-modified': 'Tue, 01 Oct 2024 00:00:00 GMT', 'date': 'Tue, 01 Oct 2024 00:02:00 GMT'})
        headers['content-length'] = '4'
        return httpx.Response(206, headers=headers, stream=Chunks(b'4567' if '4-7' in headers['content-range'] else b'0123'))
    application = app(extractor, handler)
    with TestClient(application) as client:
        playback = client.post('/v1/resolve', headers=AUTH, json={'id': ID}).json()
        video, audio = track(client, playback), track(client, playback, 'a')
        session = next(iter(application.state.sessions.sessions.values()))
        original_keys, original_manifests = set(session.tracks), dict(session.manifests)
        if validator != 'first_open':
            assert client.get(video, headers={'Range': 'bytes=0-3'}).content == b'0123'
        response = client.get(video, headers={'Range': 'bytes=4-7'})
        assert response.status_code == 206 and response.content == b'4567'
        assert extractor.calls == [{'fresh': False}, {'fresh': True}]
        assert application.state.sessions.get(session.token) is session
        assert set(session.tracks) == original_keys and session.manifests == original_manifests
        assert all('renewed=1' in r.url for r in session.tracks.values() if r.binding)
        assert requests[-1].headers['cookie'] == 'fresh-owned-cookie' and 'authorization' not in requests[-1].headers
        if validator != 'first_open':
            assert requests[-1].headers['if-range'] == {'etag': '"same-representation"', 'obs_tag': '"café"',
                'modified': 'Tue, 01 Oct 2024 00:00:00 GMT'}[validator]
        assert client.get(audio, headers={'Range': 'bytes=0-3'}).status_code == 206
        assert len(extractor.calls) == 2  # The audio track was renewed with video.


@pytest.mark.parametrize('failure', ['changed_etag', 'changed_size', 'ignored_range', 'wrong_range',
                                    'no_validator', 'weak_etag', 'recent_modified', 'missing_audio',
                                    'ambiguous_format', 'changed_format', 'changed_duration', 'wrong_video', 'still_expired'])
def test_unsafe_or_unsuccessful_renewal_ends_capability_without_forwarding_changed_media(failure):
    def change(value):
        if failure == 'missing_audio': value['formats'].pop(1)
        if failure == 'ambiguous_format': value['formats'].append(copy.deepcopy(value['formats'][0]))
        if failure == 'changed_format': value['formats'][0]['fps'] = 60
        if failure == 'changed_duration': value['duration'] = 25
        if failure == 'wrong_video': value['id'] = 'zyxwvutsrqp'
    extractor = RotatingExtractor(change)
    requests = []
    def handler(request):
        requests.append(request)
        if 'renewed=1' not in str(request.url) and request.headers.get('range') == 'bytes=4-7':
            return httpx.Response(410)
        renewed = 'renewed=1' in str(request.url)
        if renewed and failure == 'still_expired': return httpx.Response(403)
        headers = {'etag': '"original"', 'content-range': 'bytes 4-7/20' if renewed else 'bytes 0-3/20'}
        if failure in {'no_validator', 'recent_modified'}: headers.pop('etag')
        if failure == 'weak_etag': headers['etag'] = 'W/"weak"'
        if failure == 'recent_modified':
            headers.update({'last-modified': 'Tue, 01 Oct 2024 00:00:00 GMT', 'date': 'Tue, 01 Oct 2024 00:00:30 GMT'})
        if renewed and failure == 'changed_etag': headers['etag'] = '"different"'
        if renewed and failure == 'changed_size': headers['content-range'] = 'bytes 4-7/24'
        if renewed and failure == 'wrong_range': headers['content-range'] = 'bytes 0-3/20'
        if renewed and failure == 'ignored_range': return httpx.Response(200, headers={'etag': '"original"'}, content=b'changed-complete-file')
        headers['content-length'] = '4'
        return httpx.Response(206, headers=headers, stream=Chunks(b'BAD!' if renewed else b'0123'))
    application = app(extractor, handler)
    with TestClient(application) as client:
        playback = client.post('/v1/resolve', headers=AUTH, json={'id': ID}).json()
        url = track(client, playback)
        assert client.get(url, headers={'Range': 'bytes=0-3'}).content == b'0123'
        response = client.get(url, headers={'Range': 'bytes=4-7'})
        assert response.status_code == 410 and response.json()['error']['code'] == 'expired_media'
        assert b'BAD!' not in response.content and not application.state.sessions.sessions
        assert client.get(url).status_code == 410
        assert len(extractor.calls) <= 2 and len(requests) <= 3


@pytest.mark.parametrize('removed', [False, True])
def test_concurrent_track_failures_share_resolution_and_cannot_resurrect_removed_profile(removed):
    async def run():
        sessions = Sessions()
        sessions.create(info(), ResolveRequest(id=ID, profile='owned'), 'http://local')
        session = next(iter(sessions.sessions.values()))
        old_video, old_audio = session.tracks['d0v'], session.tracks['d0a']
        started, finish = asyncio.Event(), asyncio.Event()
        calls = []
        class Resolver:
            async def extract(self, target, **kwargs):
                calls.append((target, kwargs)); started.set(); await finish.wait()
                return info()
        def resolver(profile):
            assert profile == 'owned'
            return Resolver()
        first = asyncio.create_task(renew(sessions, session, 'd0v', old_video, resolver))
        await started.wait()
        second = asyncio.create_task(renew(sessions, session, 'd0a', old_audio, resolver))
        if removed: sessions.invalidate_profile('owned')
        finish.set()
        values = await asyncio.gather(first, second, return_exceptions=True)
        assert len(calls) == 1 and calls[0][1] == {'fresh': True}
        if removed:
            assert all(isinstance(v, ServiceError) for v in values) and not sessions.sessions
        else:
            assert values == [session.tracks['d0v'], session.tracks['d0a']]
            assert values[0] is not old_video and values[0].representation is old_video.representation
            with pytest.raises(ServiceError):
                await renew(sessions, session, 'd0v', values[0], resolver)
            assert len(calls) == 1
    asyncio.run(run())


def test_validator_and_content_range_parsing_rejects_unbounded_or_weak_values():
    assert entity({'etag': 'W/"weak"'}) is None
    assert entity({'etag': 'W/"weak"', 'last-modified': 'Tue, 01 Oct 2024 00:00:00 GMT',
                   'date': 'Tue, 01 Oct 2024 00:02:00 GMT'}) is None
    assert entity({'etag': '"' + 'a' * 1000 + '"'}) is None
    assert entity({'etag': '"ok"'}) == ('ETag', '"ok"')
    assert entity({'etag': '"café"'}) == ('ETag', '"café"')
    assert entity({'last-modified': 'not-a-date', 'date': 'invalid'}) is None
    sessions = Sessions(); sessions.create(info(), ResolveRequest(id=ID), 'http://local')
    remote = next(iter(sessions.sessions.values())).tracks['d0v']
    for value in ('bytes 2-1/20', 'bytes 0-20/20', 'bytes 0-1/*', 'bytes ' + '9' * 800 + '-1/20'):
        with pytest.raises(ServiceError): observe(remote, httpx.Response(206, headers={'content-range': value}))


def test_cancelled_fresh_resolution_releases_lock_without_partial_pair_or_retry_storm():
    async def run():
        sessions = Sessions(); sessions.create(info(), ResolveRequest(id=ID), 'http://local')
        session = next(iter(sessions.sessions.values())); old = dict(session.tracks)
        started = asyncio.Event()
        class Resolver:
            async def extract(self, *args, **kwargs):
                started.set(); await asyncio.Future()
        task = asyncio.create_task(renew(sessions, session, 'd0v', old['d0v'], lambda _: Resolver()))
        await started.wait(); task.cancel()
        with pytest.raises(asyncio.CancelledError): await task
        assert not session.refresh_lock.locked() and session.tracks == old
        with pytest.raises(ServiceError):
            await renew(sessions, session, 'd0v', old['d0v'], lambda _: Resolver())
    asyncio.run(run())


def test_oversized_range_is_rejected_before_upstream_or_resolution_renewal():
    requests = []
    extractor = RotatingExtractor()
    def handler(request):
        requests.append(request); return httpx.Response(403)
    with TestClient(app(extractor, handler)) as client:
        playback = client.post('/v1/resolve', headers=AUTH, json={'id': ID}).json()
        url = playback['choices'][1]['url']
        assert client.get(url, headers={'Range': 'bytes=' + '9' * 5000 + '-'}).status_code == 416
        assert not requests and len(extractor.calls) == 1
