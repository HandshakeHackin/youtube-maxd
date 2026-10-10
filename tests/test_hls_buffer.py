"""Shipping relay retains exact completed HLS bytes, never partial downloads."""
import re
import time

import httpx
import pytest
from fastapi.testclient import TestClient
from starlette.requests import Request

from prosperotube.app import create_app
from prosperotube.hls_buffer import HLSBuffer
from test_companion import Chunks
from test_live_refresh import LiveExtractor


AUTH = {'Authorization': 'Bearer test-token'}


def open_segment(client):
    playback = client.post('/v1/resolve', headers=AUTH, json={'id': 'abcdefghijk'}).json()
    root = playback['choices'][0]['url'].replace('h0.m3u8', 'h0v.m3u8')
    playlist = client.get(root)
    assert playlist.status_code == 200
    return root, re.search(r'/media/[^\s]+/hls-[^\s]+', playlist.text).group()


def test_complete_hls_bytes_survive_origin_expiry_with_exact_ranges_and_session_isolation():
    requests = []
    expired = [False]
    def handler(request):
        requests.append(request)
        if request.url.path.endswith('.m3u8'):
            return httpx.Response(200, headers={'content-type': 'application/vnd.apple.mpegurl'},
                stream=Chunks(b'#EXTM3U\n#EXT-X-MEDIA-SEQUENCE:55\n#EXTINF:2,\nsegment.ts\n'))
        if expired[0]: return httpx.Response(403)
        return httpx.Response(200, headers={'content-type': 'video/mp2t', 'content-length': '10'},
                              stream=Chunks(b'0123456789'))
    extractor = LiveExtractor()
    app = create_app(extractor=extractor, token='test-token',
                     http_client=httpx.AsyncClient(transport=httpx.MockTransport(handler)))
    with TestClient(app) as client:
        root, segment = open_segment(client)
        assert client.get(segment).content == b'0123456789'
        expired[0] = True
        before = len(requests)
        assert client.get(segment).content == b'0123456789'
        for value, content, content_range in [('bytes=2-5', b'2345', 'bytes 2-5/10'),
                ('bytes=7-', b'789', 'bytes 7-9/10'), ('bytes=-3', b'789', 'bytes 7-9/10'),
                ('bytes=1-99', b'123456789', 'bytes 1-9/10')]:
            reply = client.get(segment, headers={'Range': value})
            assert reply.status_code == 206 and reply.content == content
            assert reply.headers['content-range'] == content_range
            assert int(reply.headers['content-length']) == len(content)
        for value in ('bytes=10-', 'bytes=8-2', 'bytes=-0'):
            reply = client.get(segment, headers={'Range': value})
            assert reply.status_code == 416 and reply.headers['content-range'] == 'bytes */10'
        head = client.head(segment)
        assert head.status_code == 200 and head.content == b'' and head.headers['content-length'] == '10'
        assert client.head(segment, headers={'Range': 'bytes=2-5'}).status_code == 200
        assert len(requests) == before and extractor.calls == [{'fresh': False}]
        # Reloading the root still contacts upstream and preserves identical URLs.
        assert client.get(root).status_code == 200
        assert client.get(segment).content == b'0123456789'
        _, foreign_segment = open_segment(client)
        assert foreign_segment != segment
        assert client.get(foreign_segment).status_code == 410
        # Another session's failed request cannot retire this session's bytes.
        assert client.get(segment).content == b'0123456789'
        token = segment.split('/')[2]
        app.state.sessions.sessions[token].deadline = time.monotonic() - 1
        assert client.get(segment).status_code == 410
        app.state.hls_buffer.prune()
        assert not app.state.hls_buffer.items
    assert app.state.hls_buffer.size == 0


@pytest.mark.parametrize('headers', [
    {'content-length': '12'}, {'content-encoding': 'gzip'},
    {'content-range': 'bytes 0-9/20'},
])
def test_incomplete_encoded_or_partial_resources_are_not_buffered(headers):
    def handler(request):
        if request.url.path.endswith('.m3u8'):
            return httpx.Response(200, headers={'content-type': 'application/vnd.apple.mpegurl'},
                                  stream=Chunks(b'#EXTM3U\nsegment.ts\n'))
        return httpx.Response(200, headers={'content-type': 'video/mp2t', **headers},
                              stream=Chunks(b'0123456789'))
    app = create_app(extractor=LiveExtractor(), token='test-token',
                     http_client=httpx.AsyncClient(transport=httpx.MockTransport(handler)))
    with TestClient(app) as client:
        _, segment = open_segment(client)
        # Invalid gzip is still rejected by the receiving HTTP client; it must
        # never populate our raw-byte buffer on either success or failure.
        try: client.get(segment)
        except httpx.DecodingError: pass
        assert not app.state.hls_buffer.items and app.state.hls_buffer.staging == 0


def test_buffer_bounds_eviction_staging_and_invalidated_session():
    requests = []
    def handler(request):
        requests.append(request)
        if request.url.path.endswith('.m3u8'):
            return httpx.Response(200, headers={'content-type': 'application/vnd.apple.mpegurl'},
                                  stream=Chunks(b'#EXTM3U\na.ts\nb.ts\nlarge.ts\n'))
        data = b'123456789' if request.url.path.endswith('large.ts') else b'1234'
        return httpx.Response(200, stream=Chunks(data))
    app = create_app(extractor=LiveExtractor(), token='test-token',
                     http_client=httpx.AsyncClient(transport=httpx.MockTransport(handler)))
    app.state.hls_buffer = HLSBuffer(app.state.sessions, capacity=4, item_limit=8,
                                   count_limit=1, staging_limit=1)
    with TestClient(app) as client:
        playback = client.post('/v1/resolve', headers=AUTH, json={'id': 'abcdefghijk'}).json()
        playlist = client.get(playback['choices'][0]['url'].replace('h0.m3u8', 'h0v.m3u8')).text
        segments = re.findall(r'/media/[^\s]+/hls-[^\s]+', playlist)
        assert client.get(segments[0]).content == b'1234'
        assert client.get(segments[1]).content == b'1234'
        assert len(app.state.hls_buffer.items) == 1 and app.state.hls_buffer.size == 4
        before = len(requests)
        assert client.get(segments[1]).content == b'1234' and len(requests) == before
        assert client.get(segments[0]).content == b'1234' and len(requests) == before + 1
        assert client.get(segments[2]).content == b'123456789'
        assert app.state.hls_buffer.size == 4 and app.state.hls_buffer.staging == 0
        session = next(iter(app.state.sessions.sessions.values()))
        remote = session.tracks[segments[0].rsplit('/', 1)[-1]]
        reply = httpx.Response(200)
        capture = app.state.hls_buffer.begin(session.token, segments[0].rsplit('/', 1)[-1], remote, reply)
        assert capture is not None
        assert app.state.hls_buffer.begin(session.token, segments[1].rsplit('/', 1)[-1], remote, reply) is None
        capture.feed(b'1234')
        app.state.sessions.invalidate_profile('guest')
        capture.finish()
        assert not app.state.hls_buffer.items and app.state.hls_buffer.staging == 0
        assert client.get(segments[0]).status_code == 410


def test_interrupted_stream_drops_staging_and_does_not_create_a_partial_copy():
    class Broken(httpx.AsyncByteStream):
        async def __aiter__(self):
            yield b'partial'
            raise httpx.ReadError('truncated fixture stream')
    def handler(request):
        if request.url.path.endswith('.m3u8'):
            return httpx.Response(200, headers={'content-type': 'application/vnd.apple.mpegurl'},
                                  stream=Chunks(b'#EXTM3U\nsegment.ts\n'))
        return httpx.Response(200, stream=Broken())
    app = create_app(extractor=LiveExtractor(), token='test-token',
                     http_client=httpx.AsyncClient(transport=httpx.MockTransport(handler)))
    with TestClient(app) as client:
        _, segment = open_segment(client)
        with pytest.raises(httpx.ReadError, match='truncated fixture stream'):
            client.get(segment)
        assert not app.state.hls_buffer.items and app.state.hls_buffer.staging == 0


def test_profile_removal_callback_retires_buffered_bytes_before_next_request():
    class Library:
        on_profile_removed = None
        def has_profile(self, profile): return profile == 'guest'
        async def close(self): pass
    library = Library()
    def handler(request):
        if request.url.path.endswith('.m3u8'):
            return httpx.Response(200, headers={'content-type': 'application/vnd.apple.mpegurl'},
                                  stream=Chunks(b'#EXTM3U\nsegment.ts\n'))
        return httpx.Response(200, stream=Chunks(b'complete'))
    app = create_app(extractor=LiveExtractor(), token='test-token', library=library,
                     http_client=httpx.AsyncClient(transport=httpx.MockTransport(handler)))
    with TestClient(app) as client:
        _, segment = open_segment(client)
        assert client.get(segment).content == b'complete'
        assert app.state.hls_buffer.size == 8
        # This is the installed callback used by the real library removal path.
        async def remove(): library.on_profile_removed('guest')
        client.portal.call(remove)
        assert not app.state.hls_buffer.items and app.state.hls_buffer.size == 0
        assert client.get(segment).status_code == 410


def test_open_buffered_response_stops_next_chunk_when_its_session_is_removed():
    data = b'x' * (3 * 64 * 1024)
    def handler(request):
        if request.url.path.endswith('.m3u8'):
            return httpx.Response(200, headers={'content-type': 'application/vnd.apple.mpegurl'},
                                  stream=Chunks(b'#EXTM3U\nsegment.ts\n'))
        return httpx.Response(200, stream=Chunks(data))
    app = create_app(extractor=LiveExtractor(), token='test-token',
                     http_client=httpx.AsyncClient(transport=httpx.MockTransport(handler)))
    with TestClient(app) as client:
        _, segment = open_segment(client)
        assert client.get(segment).content == data
        _, _, token, key = segment.split('/')
        endpoint = next(route.endpoint for route in app.routes
                        if route.path == '/media/{session_token}/{key}')
        async def read_then_remove():
            response = await endpoint(token, key, Request({'type': 'http', 'method': 'GET', 'headers': []}))
            assert await anext(response.body_iterator) == data[:64 * 1024]
            app.state.sessions.invalidate_profile('guest')
            with pytest.raises(StopAsyncIteration):
                await anext(response.body_iterator)
        client.portal.call(read_then_remove)
