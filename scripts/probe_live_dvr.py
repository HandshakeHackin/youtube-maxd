"""Synthetic rolling HLS acceptance, including earlier-window and expired leases.

Compile tests/test_live_dvr.c against the exact candidate FFmpeg first. This
runner performs only bounded loopback requests and generates its own media.
"""
import argparse
from datetime import datetime, timezone
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
from pathlib import Path
import secrets
import subprocess
import tempfile
from threading import Thread


def run(binary, *, ffmpeg='ffmpeg', diagnose=False, candidate=False, runtime=False):
    with tempfile.TemporaryDirectory(prefix='tube-live-dvr-') as directory:
        root = Path(directory)
        subprocess.run([ffmpeg, '-hide_banner', '-v', 'error', '-y', '-f', 'lavfi', '-i',
                        'testsrc2=size=160x90:rate=10', '-f', 'lavfi', '-i', 'sine=frequency=880:sample_rate=48000',
                        '-t', '20', '-c:a', 'aac', '-b:a', '48k', '-c:v', 'libx264',
                        '-preset', 'ultrafast', '-g', '20', '-sc_threshold', '0', '-hls_time', '2',
                        '-hls_list_size', '0', '-hls_flags', 'independent_segments', str(root / 'source.m3u8')],
                       check=True, timeout=30)
        for index in range(8):
            subprocess.run([ffmpeg, '-hide_banner', '-v', 'error', '-y', '-copyts', '-avoid_negative_ts', 'disabled',
                            '-i', str(root / f'source{index}.ts'),
                            '-map', '0:v:0', '-c', 'copy', str(root / f'video{index}.ts'),
                            '-map', '0:a:0', '-c', 'copy', str(root / f'audio{index}.ts')],
                           check=True, timeout=10)
        lease = secrets.token_urlsafe(18)
        state = {'start': 0, 'active': True, 'requests': 0, 'gap': False, 'time': 0, 'audio_time': 0}
        class Handler(BaseHTTPRequestHandler):
            def log_message(self, *_): pass
            def do_GET(self):
                state['requests'] += 1
                if state['requests'] > 240: self.send_error(429); return
                parts = self.path.strip('/').split('/')
                if len(parts) != 2 or parts[0] != lease or not state['active']:
                    self.send_error(410); return
                key = parts[1]
                if key.startswith('invalidate-'):
                    if key == 'invalidate-gap': state['gap'] = True
                    elif key == 'invalidate-time': state['time'] = 100
                    elif key == 'invalidate-small-time': state['time'] = 1
                    elif key == 'invalidate-audio-time': state['audio_time'] = 1
                    elif key == 'invalidate-sequence': state['start'] = -1
                    elif key == 'invalidate-jump': state['start'] = 4
                    elif key == 'invalidate-expiry': state['active'] = False
                    self.send_response(200); self.send_header('Content-Length', '1'); self.end_headers(); self.wfile.write(b'1'); return
                if key == 'advance':
                    state['start'] = 2
                    self.send_response(200); self.send_header('Content-Length', '1'); self.end_headers(); self.wfile.write(b'1'); return
                if key == 'evict':
                    state['start'] = 4
                    self.send_response(200); self.send_header('Content-Length', '1'); self.end_headers(); self.wfile.write(b'1'); return
                if key.endswith('.m3u8'):
                    start = state['start']
                    if key == 'master.m3u8':
                        body = ('#EXTM3U\n#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID="sound",NAME="Sound",DEFAULT=YES,AUTOSELECT=YES,URI="audio.m3u8"\n'
                                '#EXT-X-STREAM-INF:BANDWIDTH=500000,CODECS="avc1.42c00a,mp4a.40.2",AUDIO="sound"\nvideo.m3u8\n')
                    else:
                        body = '#EXTM3U\n#EXT-X-VERSION:3\n#EXT-X-TARGETDURATION:2\n'
                        body += f'#EXT-X-MEDIA-SEQUENCE:{start}\n'
                    if key == 'event.m3u8': body += '#EXT-X-PLAYLIST-TYPE:EVENT\n'
                    if key != 'master.m3u8':
                        offset = state['time'] + (state['audio_time'] if key == 'audio.m3u8' else 0)
                        track = key[:-5] if key in ('video.m3u8', 'audio.m3u8') else 'source'
                        body += ''.join('#EXT-X-PROGRAM-DATE-TIME:' + datetime.fromtimestamp(1760000000 + i * 2 + offset, timezone.utc).isoformat().replace('+00:00', 'Z') + f'\n#EXTINF:2.000000,\n{track}{i}.ts\n' for i in range(start, start + 4))
                    if key == 'empty-selected.m3u8': body = body.replace('source2.ts', 'empty2.ts')
                    if key == 'transport-reset.m3u8': body = body.replace('source', 'reset')
                    if key == 'missing-time.m3u8': body = '\n'.join(line for line in body.splitlines() if not line.startswith('#EXT-X-PROGRAM-DATE-TIME:')) + '\n'
                    if key == 'discontinuity.m3u8' or state['gap']: body = body.replace('#EXTINF:', '#EXT-X-DISCONTINUITY\n#EXTINF:', 1)
                    if key == 'gap.m3u8': body = body.replace('#EXTINF:', '#EXT-X-GAP\n#EXTINF:', 1)
                    if key == 'finite.m3u8': body += '#EXT-X-ENDLIST\n'
                    if key == 'variants.m3u8': body = '#EXTM3U\n#EXT-X-STREAM-INF:BANDWIDTH=100000,CODECS="avc1.42c00a"\nrolling.m3u8\n#EXT-X-STREAM-INF:BANDWIDTH=90000,CODECS="avc1.42c00a"\nevent.m3u8\n'
                    data = body.encode(); mime = 'application/vnd.apple.mpegurl'
                elif key == 'empty2.ts':
                    data = b''; mime = 'video/mp2t'
                elif key.endswith('.ts') and any(key.startswith(prefix) and key[len(prefix):-3].isdigit() for prefix in ('source', 'reset', 'video', 'audio')):
                    prefix = next(prefix for prefix in ('source', 'reset', 'video', 'audio') if key.startswith(prefix))
                    index = int(key[len(prefix):-3])
                    if not state['start'] <= index < state['start'] + 4:
                        self.send_error(410); return
                    data = (root / (f'{"source" if prefix == "reset" else prefix}{0 if prefix == "reset" and index >= 3 else index}.ts')).read_bytes(); mime = 'video/mp2t'
                else: self.send_error(404); return
                self.send_response(200)
                self.send_header('Content-Type', mime)
                self.send_header('Content-Length', str(len(data)))
                self.send_header('Cache-Control', 'no-store')
                self.end_headers(); self.wfile.write(data)
        server = ThreadingHTTPServer(('127.0.0.1', 0), Handler)
        thread = Thread(target=server.serve_forever, daemon=True); thread.start()
        base = f'http://127.0.0.1:{server.server_port}/{lease}'
        def probe(key, target, clear=False, mode=None):
            result = subprocess.run([str(binary), f'{base}/{key}', str(target),
                                     mode or ('diagnose-clear-gate' if clear else 'shipping')],
                                    capture_output=True, text=True, timeout=15)
            if result.returncode: raise RuntimeError(f"Probe {mode or key} failed: {result.stderr}")
            lines = result.stdout.strip().splitlines()
            value = [json.loads(line) for line in lines] if len(lines) > 1 else json.loads(lines[0])
            if mode and mode.startswith('split') and not (value[0] if isinstance(value, list) else value)['opened']:
                raise RuntimeError(f'Split HLS did not open: {value}; {result.stderr}')
            return value
        try:
            # The finite control gives the actual packet origin (MPEG-TS adds
            # a nonzero offset). We do not equate playlist seconds with PTS.
            finite = probe('finite.m3u8', 1.5)
            assert finite['opened'] and finite['seek_result'] >= 0, finite
            origin = finite['origin']
            target = origin + .25
            rolling = probe('rolling.m3u8', target)
            bypass = probe('rolling.m3u8', target, True)
            event = probe('event.m3u8', target)
            additional = {}
            if candidate:
                for key in ('missing-time.m3u8', 'discontinuity.m3u8', 'gap.m3u8', 'variants.m3u8', 'empty-selected.m3u8'):
                    value = probe(key, target)
                    assert (key == 'empty-selected.m3u8' and not value['opened']) or (value['unseekable'] and value['seek_result'] < 0), value
                    additional[key] = value
                additional['active_rollover'] = probe('rolling.m3u8', target, mode='rollover')
                state['start'] = 0
                additional['split_av'] = probe('master.m3u8', target, mode='split')
                additional['split_audio_shift'] = probe('master.m3u8', target, mode='split-invalidate-audio')
                assert additional['split_av'][0]['seek_result'] >= 0 and additional['split_av'][1]['split_audio_video_calibrated']
                assert additional['split_audio_shift'][1]['audio_shift_refused']
                state['audio_time'] = 0
                additional['split_restore'] = probe('master.m3u8', target, mode='split-runtime') if runtime else None
                if runtime:
                    assert additional['split_restore'][-1]['same_point_restored']
                    assert additional['split_restore'][-1]['evicted_point_refused']
                state['start'] = 0
                if runtime:
                    additional['runtime'] = probe('rolling.m3u8', target, mode='runtime')
                    state['start'] = 0
                    additional['runtime_shifted'] = probe('rolling.m3u8', target, mode='runtime-shifted')
                    state['time'] = 0
                additional['prepare'] = probe('rolling.m3u8', target, mode='prepare')
                state['start'] = 0
                additional['deadline'] = probe('rolling.m3u8', target, mode='deadline')
                additional['transport_reset'] = probe('transport-reset.m3u8', target, mode='transport-reset')
                additional['reopen'] = probe('rolling.m3u8', target, mode='reopen')
                state['start'] = 0
                additional['other_timeline'] = probe('rolling.m3u8', target, mode='reopen-other-timeline')
                state['time'] = 0
                for mode in ('invalidate-gap', 'invalidate-time', 'invalidate-sequence', 'invalidate-jump', 'invalidate-expiry'):
                    additional[mode] = probe('rolling.m3u8', target, mode=mode)
                    state.update(start=0, active=True, gap=False, time=0)
            # Move the server window without downloading/retaining history.
            state['start'] = 2
            shifted = probe('finite.m3u8', target)
            current = probe('finite.m3u8', origin + 4.25)
            assert shifted['seek_result'] < 0 and current['seek_result'] >= 0, (shifted, current)
            state['active'] = False
            expired = probe('finite.m3u8', origin + 4.25)
            assert expired['opened'] is False, expired
            report = {'scope': 'synthetic bounded rolling HLS; no YouTube/account/console',
                      'finite_control': finite, 'rolling_live': rolling, 'gate_bypass_diagnostic': bypass,
                      'event_live': event, 'expired_position': shifted, 'current_window_position': current,
                      'expired_session': expired, 'upstream_requests': state['requests'], **additional}
            if diagnose:
                assert rolling['unseekable'] and rolling['seek_result'] < 0, rolling
                assert bypass['seek_result'] < 0 and event['seek_result'] < 0, (bypass, event)
                assert rolling['origin'] >= origin + 3.9, (rolling, finite)
                report['acceptance'] = 'blocked: rolling gate and live-edge/window-origin calibration'
            else:
                assert rolling['seek_result'] >= 0, 'DVR acceptance failed: earlier live-window seek rejected'
                assert rolling['landed'] is not None and abs(rolling['landed'] - origin) < .2, rolling
                report['acceptance'] = ('earlier live-window seek and native restore/expiry accepted'
                                        if runtime else 'earlier live-window seek accepted')
            return report
        finally:
            server.shutdown(); server.server_close(); thread.join(timeout=5)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, required=True)
    parser.add_argument('--ffmpeg', default='ffmpeg')
    parser.add_argument('--diagnose-current-limitations', action='store_true')
    parser.add_argument('--candidate-window', action='store_true')
    parser.add_argument('--runtime-window', action='store_true')
    parser.add_argument('--result', type=Path)
    args = parser.parse_args()
    report = run(args.binary.resolve(), ffmpeg=args.ffmpeg, diagnose=args.diagnose_current_limitations, candidate=args.candidate_window, runtime=args.runtime_window)
    text = json.dumps(report, indent=2) + '\n'
    if args.result: args.result.parent.mkdir(parents=True, exist_ok=True); args.result.write_text(text)
    print(text, end='')


if __name__ == '__main__': main()
