"""Rolling HLS and active/idle capability lifetime using the shipping relay."""
import re

from fastapi.testclient import TestClient
import httpx
import pytest

from prosperotube.app import create_app
from prosperotube.hls import rewrite
from prosperotube.models import ResolveRequest, ServiceError
from prosperotube.sessions import Remote, Session, Sessions
from prosperotube.live_hls import apply_master, LiveHLSYoutubeIE


def split_live_info():
    master = 'https://r1.googlevideo.com/master.m3u8'
    info = {'id': 'abcdefghijk', 'is_live': True, 'formats': [
        {'url': 'https://r1.googlevideo.com/video.m3u8', 'manifest_url': master,
         'vcodec': 'avc1.4D4015', 'acodec': 'none', 'height': 240, 'width': 426, 'protocol': 'm3u8_native'},
        {'url': 'https://r1.googlevideo.com/audio.m3u8', 'manifest_url': master,
         'vcodec': 'none', 'acodec': None, 'protocol': 'm3u8_native'}]}
    content = ('#EXTM3U\n#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID="233",NAME="Default",URI="audio.m3u8"\n'
               '#EXT-X-STREAM-INF:BANDWIDTH=507418,CODECS="avc1.4D4015,mp4a.40.5",AUDIO="233"\nvideo.m3u8\n')
    return info, master, content


def test_split_live_master_requires_explicit_codec_and_exact_pair_then_relays_both_tracks():
    info, master, content = split_live_info()
    sessions = Sessions()
    request = ResolveRequest(id='abcdefghijk', codec='avc', max_height=240)
    with pytest.raises(ServiceError): sessions.create(info, request, 'http://local')
    apply_master(info, master, content.replace('mp4a.40.5', 'unknown'))
    with pytest.raises(ServiceError): sessions.create(info, request, 'http://local')
    apply_master(info, master, content)
    playback = sessions.create(info, request, 'http://local')
    assert len(playback.choices) == 1 and playback.choices[0].audio_codec == 'aac'
    assert playback.choices[0].is_live and playback.choices[0].container == 'hls'
    session = next(iter(sessions.sessions.values()))
    assert len(session.tracks) == 2 and all('googlevideo' not in value for value in session.hls_manifests.values())
    app = create_app(token='test-token')
    app.state.sessions = sessions
    with TestClient(app) as client:
        response = client.get('/media/' + session.token + '/h0.m3u8')
        assert response.status_code == 200 and 'mpegurl' in response.headers['content-type']
        assert 'GROUP-ID="audio"' in response.text
        assert '/h0a.m3u8' in response.text and '/h0v.m3u8' in response.text
    # A different manifest cannot supply this rendition's audio.
    info['formats'][1]['manifest_url'] = 'https://r1.googlevideo.com/other.m3u8'
    with pytest.raises(ServiceError): sessions.create(info, request, 'http://local')


def test_pinned_youtube_parser_retains_original_master_audio_pairing_without_extra_fetches():
    from yt_dlp import YoutubeDL
    info, master, content = split_live_info()
    with YoutubeDL({'quiet': True}) as ydl:
        ie = LiveHLSYoutubeIE()
        ydl.add_info_extractor(ie)
        def unexpected(*args, **kwargs): raise AssertionError('unexpected second manifest fetch')
        ie._download_webpage = unexpected
        formats, _ = ie._parse_m3u8_formats_and_subtitles(content, master, ext='mp4', live=True)
    audio = next(f for f in formats if f.get('vcodec') == 'none')
    video = next(f for f in formats if f.get('height') is None and f.get('vcodec') == 'avc1.4D4015')
    assert audio['acodec'] == 'mp4a.40.5'
    assert video['hls_audio_urls'] == [audio['url']]
    # Invalid master origins cannot add pairing capabilities.
    with YoutubeDL({'quiet': True}) as ydl:
        ie = LiveHLSYoutubeIE(); ydl.add_info_extractor(ie)
        formats, _ = ie._parse_m3u8_formats_and_subtitles(content, 'http://127.0.0.1/master.m3u8', ext='mp4')
        assert not any(f.get('hls_audio_urls') for f in formats)


def test_hdr_master_range_marks_muxed_variant_without_separate_audio():
    master = 'https://r1.googlevideo.com/master.m3u8'
    video = {'url': 'https://r1.googlevideo.com/video.m3u8', 'manifest_url': master,
             'vcodec': 'vp09.02.51.10', 'acodec': 'mp4a.40.2', 'protocol': 'm3u8_native',
             'dynamic_range': 'HDR10'}
    apply_master({'formats': [video]}, master,
                 '#EXTM3U\n#EXT-X-STREAM-INF:BANDWIDTH=1000000,VIDEO-RANGE=HLG,'
                 'CODECS="vp09.02.51.10,mp4a.40.2"\nvideo.m3u8\n')
    assert video['_tube_hls_video_range'] == video['dynamic_range'] == 'HLG'
    video['dynamic_range'] = 'HDR10'
    apply_master({'formats': [video]}, master,
                 '#EXTM3U\n#EXT-X-STREAM-INF:BANDWIDTH=1000000,'
                 'CODECS="vp09.02.51.10,mp4a.40.2"\nvideo.m3u8\n')
    assert video['dynamic_range'] == 'UNKNOWN' and '_tube_hls_video_range' not in video

def test_rolling_hls_retains_current_graph_and_retires_old_windows_atomically():
    root = Remote("https://r1.googlevideo.com/root.m3u8", {"Cookie": "root-private", "User-Agent": "test"}, "hls")
    session = Session("capability", "abcdefghijk", 99999, tracks={"root": root})
    rewrite('#EXTM3U\n#EXT-X-MEDIA:TYPE=AUDIO,URI="audio.m3u8"\nvariant.m3u8\n', root.url, session, "http://local", "root", root, limit=10)
    variant = next(key for key, value in session.tracks.items() if value.url.endswith("/variant.m3u8"))
    audio = next(key for key, value in session.tracks.items() if value.url.endswith("/audio.m3u8"))
    for i in range(40):
        source = session.tracks[variant]
        result = rewrite(f'#EXTM3U\n#EXT-X-KEY:METHOD=AES-128,URI="key.key"\n#EXT-X-MAP:URI="init.mp4"\n{i}.ts\n{i+1}.ts\n',
                         source.url, session, "http://local", variant, source, limit=10)
        assert len(session.tracks) <= 10 and variant in session.tracks and audio in session.tracks
        for key in re.findall(r"/media/capability/([^\s\"]+)", result):
            assert key in session.tracks
    assert not any(remote.url.endswith("/0.ts") for remote in session.tracks.values())
    previous = (dict(session.tracks), dict(session.hls_resources), dict(session.hls_playlists))
    for content in ('#EXTM3U\nnew.ts\nhttp://127.0.0.1/private\n', '#EXTM3U\n' + '\n'.join(f'overflow{i}.ts' for i in range(20))):
        with pytest.raises(ServiceError): rewrite(content, root.url, session, "http://local", "root", root, limit=10)
        assert (session.tracks, session.hls_resources, session.hls_playlists) == previous
    # Removing a variant retires its graph; it cannot pin every past segment.
    rewrite('#EXTM3U\nnew-variant.m3u8\n', root.url, session, "http://local", "root", root, limit=10)
    assert variant not in session.hls_playlists


def test_hls_children_inherit_their_actual_parent_and_strip_cross_host_auth():
    wrong = Remote("https://r1.googlevideo.com/root.m3u8", {"Cookie": "wrong-root"}, "hls")
    parent = Remote("https://r2.googlevideo.com/child.m3u8", {"Cookie": "parent-private", "Authorization": "parent-auth", "User-Agent": "test"}, "hls")
    session = Session("cap", "abcdefghijk", 99, tracks={"root": wrong, "child": parent})
    rewrite('#EXTM3U\nsegment.ts\nhttps://r3.googlevideo.com/other.ts\n', parent.url, session, "http://local", "child", parent)
    same = next(value for value in session.tracks.values() if value.url.endswith("/segment.ts"))
    cross = next(value for value in session.tracks.values() if value.url.endswith("/other.ts"))
    assert same.headers["Cookie"] == "parent-private" and same.headers["Authorization"] == "parent-auth"
    assert cross.headers == {"User-Agent": "test"}


def test_shipping_live_requests_renew_idle_lease_but_never_restore_revoked_tokens(monkeypatch):
    clock = [100.0]
    monkeypatch.setattr("prosperotube.sessions.time.monotonic", lambda: clock[0])
    root = Remote("https://r1.googlevideo.com/live.m3u8", {}, "application/vnd.apple.mpegurl")
    def handler(request):
        data = b'#EXTM3U\n#EXT-X-TARGETDURATION:2\n#EXTINF:2,\nsegment.ts\n' if request.url.path.endswith(".m3u8") else b"media"
        return httpx.Response(200, stream=httpx.ByteStream(data), headers={"content-type": root.mime if request.url.path.endswith(".m3u8") else "video/mp2t"})
    app = create_app(token="test-token", http_client=httpx.AsyncClient(transport=httpx.MockTransport(handler)))
    session = Session("live-cap", "abcdefghijk", 110, tracks={"root.m3u8": root})
    app.state.sessions.ttl = 10
    app.state.sessions.sessions[session.token] = session
    with TestClient(app) as client:
        for current in (109, 118, 127, 136):
            clock[0] = current
            playlist = client.get("/media/live-cap/root.m3u8")
            assert playlist.status_code == 200 and session.deadline == current + 10
        segment = next(key for key in session.tracks if key.endswith(".ts"))
        clock[0] = 145
        assert client.get("/media/live-cap/" + segment).content == b"media" and session.deadline == 155
        clock[0] = 154
        assert client.get("/media/live-cap/missing").status_code == 404 and session.deadline == 155
        clock[0] = 156
        assert client.get("/media/live-cap/root.m3u8").status_code == 410
        app.state.sessions.touch(session)
        assert "live-cap" not in app.state.sessions.sessions
