#!/usr/bin/env python3
"""Decode generated HLS, expire its origin objects, then seek the exact buffered bytes."""
import argparse
import json
from pathlib import Path
import re
import shutil
import socket
import subprocess
import threading
import time

import httpx
import uvicorn

from prosperotube.app import create_app


class Bytes(httpx.AsyncByteStream):
    def __init__(self, data): self.data = data
    async def __aiter__(self): yield self.data


def decode(ffmpeg, source, seek, duration, *, fingerprints=False, require=True):
    args = [ffmpeg, '-nostdin', '-hide_banner', '-loglevel', 'error']
    # An input seek exercises the demuxer. An output -ss would just discard
    # decoded frames from the beginning and cannot establish this behavior.
    if seek: args += ['-ss', str(seek)]
    args += ['-i', str(source), '-t', str(duration), '-map', '0:v:0', '-map', '0:a:0',
             '-progress', 'pipe:2' if fingerprints else 'pipe:1', '-nostats',
             '-f', 'framemd5' if fingerprints else 'null', '-']
    result = subprocess.run(args, capture_output=True, text=True, timeout=45)
    times = re.findall(r'^out_time_us=([0-9]+)$', result.stderr if fingerprints else result.stdout, re.M)
    seconds = int(times[-1]) / 1000000 if times else 0
    valid = result.returncode == 0 and seconds >= duration - .25
    if require and not valid:
        raise RuntimeError(f'Generated HLS decode failed ({result.returncode}, {seconds:.3f}s).')
    hashes = []
    if fingerprints:
        for line in result.stdout.splitlines():
            if not line or line.startswith('#'): continue
            parts = [part.strip() for part in line.split(',')]
            if len(parts) != 6 or not re.fullmatch('[0-9a-f]{32}', parts[-1]):
                raise RuntimeError('Invalid generated frame fingerprint.')
            # Compare stream, duration, byte size and content. DTS/PTS are
            # rebased by FFmpeg and are checked separately through duration.
            hashes.append((int(parts[0]), int(parts[3]), int(parts[4]), parts[5]))
        if require and not {0, 1} <= {row[0] for row in hashes}:
            raise RuntimeError('Both decoded audio and video fingerprints are required.')
    return seconds, hashes, valid


def run(fixtures):
    ffmpeg = shutil.which('ffmpeg')
    if not ffmpeg: raise RuntimeError('Host FFmpeg is required.')
    playlist = (fixtures / 'fixture.m3u8').read_bytes()
    expired, requests = [False], []
    class Extractor:
        async def extract(self, target, **kwargs):
            return {'id': 'BUFFERHLS01', 'title': 'Generated buffered HLS', 'is_live': True,
                    'formats': [{'format_id': 'fixture-hls', 'url': 'https://r1.googlevideo.com/fixture.m3u8',
                                 'protocol': 'm3u8_native', 'ext': 'mp4', 'height': 360, 'width': 640,
                                 'vcodec': 'avc1.4D401E', 'acodec': 'mp4a.40.2'}]}
    def handler(request):
        name = request.url.path.rsplit('/', 1)[-1]
        requests.append(name)
        if name == 'fixture.m3u8':
            return httpx.Response(200, headers={'content-type': 'application/vnd.apple.mpegurl'},
                                  stream=Bytes(playlist))
        if not re.fullmatch(r'fixture[0-9]+\.ts', name): return httpx.Response(404)
        if expired[0]: return httpx.Response(403)
        data = (fixtures / name).read_bytes()
        return httpx.Response(200, headers={'content-type': 'video/mp2t', 'content-length': str(len(data))},
                              stream=Bytes(data))
    sock = socket.socket()
    sock.bind(('127.0.0.1', 0))
    sock.listen(128)
    port = sock.getsockname()[1]
    token = 'generated-buffer-probe'
    app = create_app(extractor=Extractor(), token=token,
                     http_client=httpx.AsyncClient(transport=httpx.MockTransport(handler)))
    server = uvicorn.Server(uvicorn.Config(app, log_level='error', access_log=False))
    thread = threading.Thread(target=lambda: server.run(sockets=[sock]), daemon=True)
    thread.start()
    try:
        deadline = time.monotonic() + 15
        while not server.started:
            if not thread.is_alive() or time.monotonic() >= deadline:
                raise RuntimeError('Fixture server did not start.')
            time.sleep(.05)
        with httpx.Client(base_url=f'http://127.0.0.1:{port}', timeout=10) as client:
            reply = client.post('/v1/resolve', headers={'Authorization': 'Bearer ' + token},
                                json={'id': 'BUFFERHLS01'})
            reply.raise_for_status()
            url = reply.json()['choices'][0]['url']
        first, _, _ = decode(ffmpeg, url, 0, 8)
        reference = (fixtures / 'fixture.m3u8').resolve()
        _, expected, _ = decode(ffmpeg, reference, 3, 2, fingerprints=True)
        _, beginning, _ = decode(ffmpeg, reference, 0, 2, fingerprints=True)
        if [row for row in expected if row[0] == 0] == [row for row in beginning if row[0] == 0]:
            raise RuntimeError('The fixture cannot distinguish a seek from playback at the beginning.')
        buffered_count, buffered_size = len(app.state.hls_buffer.items), app.state.hls_buffer.size
        if not buffered_count: raise RuntimeError('No complete HLS resources were buffered.')
        origin_before = sum(name.endswith('.ts') for name in requests)
        expired[0] = True
        second, actual, _ = decode(ffmpeg, url, 3, 2, fingerprints=True)
        origin_after = sum(name.endswith('.ts') for name in requests)
        if origin_before != origin_after:
            raise RuntimeError('Seek contacted an expired origin resource.')
        if actual != expected:
            raise RuntimeError('Buffered input seek differs from the local decoded audio/video reference.')
        # A negative control proves that a successful cached seek cannot be
        # explained by an origin still serving bytes or by ignoring expiry.
        app.state.hls_buffer.clear()
        _, _, uncached_succeeded = decode(ffmpeg, url, 3, 2, require=False)
        missed_origin = sum(name.endswith('.ts') for name in requests) - origin_after
        if uncached_succeeded or missed_origin <= 0:
            raise RuntimeError('Unbuffered expired-origin negative control did not fail as required.')
        return {'scope': 'generated finite HLS / host FFmpeg', 'initial_decode_seconds': first,
                'seek_seconds': 3, 'seek_mode': 'input', 'seek_decode_seconds': second,
                'decoded_video_frames': sum(row[0] == 0 for row in actual),
                'decoded_audio_blocks': sum(row[0] == 1 for row in actual),
                'decoded_audio_video_match_local_reference': True,
                'seek_differs_from_beginning': True, 'uncached_expiry_rejected': True,
                'uncached_origin_requests': missed_origin, 'buffered_resources': buffered_count,
                'buffered_bytes': buffered_size, 'origin_resources_before_expiry': origin_before,
                'origin_resources_during_seek': origin_after - origin_before,
                'console_traffic': False, 'youtube_traffic': False}
    finally:
        server.should_exit = True
        thread.join(timeout=15)
        sock.close()
        if thread.is_alive(): raise RuntimeError('Fixture server did not retire.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--fixtures', type=Path, default=Path('fixtures/generated'))
    parser.add_argument('--result', type=Path, required=True)
    args = parser.parse_args()
    result = run(args.fixtures)
    args.result.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result))
