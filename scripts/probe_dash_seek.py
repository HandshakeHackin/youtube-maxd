"""Measure DASH seek bytes, video decode and audio packet delivery over HTTP."""
import argparse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
from pathlib import Path
import re
import subprocess
import threading
import time
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'companion'))
from prosperotube.sessions import dash_manifest


def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--ffmpeg', required=True)
    ap.add_argument('--output', type=Path, required=True)
    ap.add_argument('--require-efficient', action='store_true')
    ap.add_argument('--layout', choices=('single','segmented','separate-init'), default='single')
    args=ap.parse_args()
    work=Path.home()/'.cache/prosperotube/dash-seek-fixture'
    work.mkdir(parents=True, exist_ok=True)
    specs=[('video.mp4',['testsrc2=size=320x180:rate=15','-an','-c:v','libx264','-preset','ultrafast','-g','30']),
           ('audio.m4a',['sine=frequency=880:sample_rate=48000','-vn','-c:a','aac'])]
    for name, options in specs:
        target=work/name
        if not target.exists():
            subprocess.run(['ffmpeg','-v','error','-y','-f','lavfi','-i',options[0],'-t','240',*options[1:],
                            '-movflags','+empty_moov+frag_keyframe+default_base_moof+global_sidx',str(target)],check=True)
    resources={name:(work/name).read_bytes() for name,_ in specs}
    resources['test.mpd']=dash_manifest(240,{'ext':'mp4','vcodec':'avc1.42c00c','width':320,'height':180},
                                       {'ext':'m4a','acodec':'mp4a.40.2','asr':48000},'video.mp4','audio.m4a').encode()
    if args.layout!='single':
        segmented=work/args.layout
        segmented.mkdir(exist_ok=True)
        if not (segmented/'test.mpd').exists():
            subprocess.run(['ffmpeg','-v','error','-y','-i',str(work/'video.mp4'),'-i',str(work/'audio.m4a'),
                            '-map','0:v','-map','1:a','-c','copy','-f','dash','-seg_duration',
                            '10' if args.layout=='segmented' else '300',str(segmented/'test.mpd')],check=True)
        resources={p.name:p.read_bytes() for p in segmented.iterdir() if p.is_file()}
    stats={'bytes':0,'requests':[]}
    class Handler(BaseHTTPRequestHandler):
        def log_message(self,*args): pass
        def do_GET(self):
            key=self.path.lstrip('/')
            if key not in resources:
                self.send_error(404); return
            data=resources[key];start=0;end=len(data)-1
            raw=self.headers.get('Range','')
            if raw:
                match=re.fullmatch(r'bytes=(\d+)-(\d*)',raw)
                if not match: self.send_error(416);return
                start=int(match[1]);end=min(int(match[2]),end) if match[2] else end
            if start>end: self.send_error(416);return
            self.send_response(206 if raw else 200)
            self.send_header('Accept-Ranges','bytes')
            self.send_header('Content-Length',str(end-start+1))
            if raw:self.send_header('Content-Range',f'bytes {start}-{end}/{len(data)}')
            self.end_headers()
            record={'resource':key,'start':start,'bytes':0};stats['requests'].append(record)
            try:
                for offset in range(start,end+1,8192):
                    part=data[offset:min(offset+8192,end+1)]
                    self.wfile.write(part);self.wfile.flush()
                    record['bytes']+=len(part)
                    if not key.endswith('.mpd'): stats['bytes']+=len(part)
                    time.sleep(.001)
            except (BrokenPipeError,ConnectionResetError): pass
    server=ThreadingHTTPServer(('127.0.0.1',0),Handler)
    threading.Thread(target=server.serve_forever,daemon=True).start()
    try:
        progress=work/'seek-progress.txt'
        progress.unlink(missing_ok=True)
        started=time.monotonic()
        run=subprocess.run([args.ffmpeg,'-v','error','-ss','180','-i',f'http://127.0.0.1:{server.server_port}/test.mpd',
                            '-t','1','-map','0:v','-map','0:a','-c:a','copy','-progress',str(progress),'-f','null','-'],
                           capture_output=True,timeout=90)
        decoded_progress=progress.read_bytes() if progress.exists() else b''
        result={'exit_code':run.returncode,'wall_seconds':round(time.monotonic()-started,3),
                'layout':args.layout,'total_media_bytes':sum(len(v) for k,v in resources.items() if not k.endswith('.mpd')),**stats,
                'decoded_video_frames':max([int(v) for v in re.findall(rb'^frame=(\d+)',decoded_progress,re.M)] or [0]),
                'advanced_one_second':b'out_time_us=1000000' in decoded_progress or any(int(v)>=950000 for v in re.findall(rb'out_time_us=(\d+)',decoded_progress))}
        args.output.parent.mkdir(parents=True,exist_ok=True)
        args.output.write_text(json.dumps(result,indent=2)+'\n')
        assert run.returncode==0,run.stderr.decode(errors='replace')[-2000:]
        assert result['advanced_one_second'] and result['decoded_video_frames'] >= 10,decoded_progress
        if args.require_efficient:
            assert stats['bytes'] < result['total_media_bytes']/3,result
        print(json.dumps({k:v for k,v in result.items() if k!='requests'}))
    finally:
        server.shutdown();server.server_close()

if __name__=='__main__': main()