#!/usr/bin/env python3
"""Offline regression: ranged MP4 and WebM seek across fragment boundaries.

Uses seek_lab's generated ten-minute files and a loopback range server.
Pass the FFmpeg built with scripts/patch_ffmpeg_dash_seek.py and tube_js.
"""
import argparse
from pathlib import Path
import re

from seek_lab import DURATION, Server, fixtures, manifests, play


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ffmpeg', required=True)
    parser.add_argument('--tube-js', required=True)
    args = parser.parse_args()
    server = Server(fixtures())
    try:
        for container in ('mp4', 'webm'):
            shapes = manifests(server, str(Path(args.tube_js).expanduser()), container, 30)
            for shape in ('ranged', 'ranged30'):
                for seek in (0, DURATION // 2, DURATION - 20):
                    server.reset()
                    code, wall, _ = play(str(Path(args.ffmpeg).expanduser()),
                                         f'{server.base}/{shapes[shape][0]}', seek,
                                         seconds=12, limit=15)
                    assert code == 0, (container, shape, seek, code, wall)
                    assert 0 < server.bytes < 8 * 1024 * 1024, (container, shape, seek, server.bytes)
                    assert server.ranges and all(re.fullmatch(r'bytes=\d+-\d+', value) for value in server.ranges), server.ranges
                    print(f'{container}/{shape} seek={seek}: passed in {wall:.2f}s, {server.requests} requests')
    finally:
        server.httpd.shutdown()
        server.httpd.server_close()


if __name__ == '__main__':
    main()
