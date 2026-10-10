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
        def decode(seek, duration):
            args = [ffmpeg, '-nostdin', '-hide_banner', '-loglevel', 'error', '-i', url]
            if seek: args += ['-ss', str(seek)]
            args += ['-t', str(duration), '-map', '0:v:0', '-map', '0:a:0',
                     '-progress', 'pipe:1', '-nostats', '-f', 'null', '-']
            result = subprocess.run(args, capture_output=True, text=True, timeout=45)
            times = re.findall(r'^out_time_us=([0-9]+)$', result.stdout, re.M)
            seconds = int(times[-1]) / 1000000 if times else 0
            if result.returncode or seconds < duration - .25:
                raise RuntimeError(f'Generated HLS decode failed ({result.returncode}, {seconds:.3f}s).')
            return seconds
        first = decode(0, 8)
        buffered_count, buffered_size = len(app.state.hls_buffer.items), app.state.hls_buffer.size
        if not buffered_count: raise RuntimeError('No complete HLS resources were buffered.')
        origin_before = sum(name.endswith('.ts') for name in requests)
        expired[0] = True
        second = decode(3, 2)
        origin_after = sum(name.endswith('.ts') for name in requests)
        if origin_before != origin_after:
            raise RuntimeError('Seek contacted an expired origin resource.')
        return {'scope': 'generated finite HLS / host FFmpeg', 'initial_decode_seconds': first,
                'seek_seconds': 3, 'seek_decode_seconds': second, 'buffered_resources': buffered_count,
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
