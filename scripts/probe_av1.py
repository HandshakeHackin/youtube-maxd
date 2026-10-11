"""Offline AV1 or HEVC/AAC decode and seek through the real companion DASH/relay path."""
from pathlib import Path
import json
import re
import socket
import subprocess
import sys
import threading
import time

import httpx
import uvicorn

from generate_fixtures import generate
from prosperotube.demo import create_demo_app


def run(*args):
    return subprocess.run(args, check=True, capture_output=True, text=True, timeout=90)


def main(codec="av1", destination=None, *, generate_media=True):
    destination = Path(destination if destination is not None else
                       sys.argv[1] if len(sys.argv) > 1 else f'.deps/{codec}-host')
    assert codec in {'av1', 'hevc'}
    decoder = 'libdav1d' if codec == 'av1' else 'hevc'
    if generate_media:
        generate(destination)
    listener = socket.socket()
    listener.bind(('127.0.0.1', 0))
    base = f'http://127.0.0.1:{listener.getsockname()[1]}'
    server = uvicorn.Server(uvicorn.Config(create_demo_app(destination, token='offline-av1-probe'),
                                         log_level='error', access_log=False))
    thread = threading.Thread(target=lambda: server.run(sockets=[listener]), daemon=True)
    thread.start()
    try:
        deadline = time.monotonic() + 10
        while not server.started:
            if not thread.is_alive() or time.monotonic() >= deadline:
                raise RuntimeError('Offline relay did not start')
            time.sleep(.05)
        result = httpx.post(base + '/v1/resolve', headers={'Authorization': 'Bearer offline-av1-probe'},
                            json={'id': 'DEMOAV10001' if codec == 'av1' else 'DEMOHEVC001', 'codec': codec}, timeout=20)
        result.raise_for_status()
        choice = result.json()['choices'][0]
        assert choice['video_codec'] == codec and choice['audio_codec'] == 'aac' and choice['container'] == 'dash'
        url = choice['url']
        probe = json.loads(run('ffprobe', '-v', 'error', '-show_streams', '-of', 'json', url).stdout)
        assert {s['codec_name'] for s in probe['streams']} == {codec, 'aac'}
        decoded = run('ffmpeg', '-v', 'error', '-c:v', decoder, '-i', url,
                      '-map', '0:v', '-map', '0:a', '-progress', 'pipe:1', '-nostats', '-f', 'null', '-')
        sought = run('ffmpeg', '-v', 'error', '-ss', '4', '-c:v', decoder, '-i', url,
                     '-t', '2', '-map', '0:v', '-map', '0:a', '-progress', 'pipe:1', '-nostats', '-f', 'null', '-')
        assert not decoded.stderr and not sought.stderr
        frames = [max(map(int, re.findall(r'^frame=(\d+)$', result.stdout, re.MULTILINE))) for result in (decoded, sought)]
        assert frames[0] >= 239 and frames[1] >= 59, frames
        def seek_hash(source):
            return run('ffmpeg', '-v', 'error', '-ss', '4', '-c:v', decoder, '-i', source,
                       '-map', '0:v', '-frames:v', '1', '-an', '-f', 'hash', '-hash', 'sha256', '-').stdout.strip()
        assert seek_hash(url) == seek_hash(str(destination / f'video-{codec}.mp4'))
        print(json.dumps({'result': 'pass', 'decoder': decoder, 'transport': 'companion DASH relay',
                          'decoded_frames': frames[0], 'seek_frames': frames[1],
                          'checks': [f'{codec.upper()}/AAC probe', 'complete decode', 'seek to four seconds',
                                     'seek frame matches local original']}, indent=2))
    finally:
        server.should_exit = True
        thread.join(timeout=10)
        listener.close()
        if thread.is_alive():
            raise RuntimeError('Offline relay did not stop')


if __name__ == '__main__':
    main()
