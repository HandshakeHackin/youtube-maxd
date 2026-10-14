"""Exercise the shipping initial calibration helper with a moved inner reader."""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'scripts/ffmpeg_hls_dvr.inc').read_text()
start = source.index('static void tube_dvr_anchor(')
end = source.index('\n}', start) + 2
helper = source[start:end]
scaffold = r"""
#include <assert.h>
#include <libavformat/avformat.h>
#include <libavutil/mathematics.h>
struct segment { int64_t dvr_utc; };
struct playlist { int finished,dvr_bad,n_segments,dvr_kind,dvr_seen,index; int64_t cur_seq_no,dvr_selected_seq,start_seq_no,dvr_first_pts,dvr_first_utc;
    AVPacket *pkt;struct segment **segments;AVFormatContext *ctx; };
typedef struct HLSContext {int tube_dvr,dvr_anchored,dvr_disabled,n_playlists,dvr_video_playlist;
    int64_t dvr_offset,dvr_first_utc;struct playlist **playlists;} HLSContext;
static int published;
static void tube_dvr_publish(HLSContext *c) { assert(c->dvr_anchored || c->dvr_disabled);++published; }
static AVRational get_timebase(struct playlist *p) {(void)p;return (AVRational){1,90000};}
"""
checks = r"""
int main(void) {
    struct segment a[4]={{1760000000000000LL},{1760000002000000LL},{1760000004000000LL},{1760000006000000LL}};
    struct segment *segments[4]={a,a+1,a+2,a+3};
    unsigned char data=1;
    AVPacket packet={.pts=540000,.dts=540000,.flags=AV_PKT_FLAG_KEY,.data=&data};
    AVCodecParameters codec={.codec_type=AVMEDIA_TYPE_VIDEO};
    AVStream stream={.codecpar=&codec};AVStream *streams[]={&stream};
    AVFormatContext format={.nb_streams=1,.streams=streams};
    struct playlist p={.n_segments=4,.cur_seq_no=2,.dvr_selected_seq=2,.start_seq_no=0,.pkt=&packet,.segments=segments,.ctx=&format};
    struct playlist *playlists[]={&p};
    HLSContext initial={.tube_dvr=1,.n_playlists=1,.playlists=playlists},c=initial;
    tube_dvr_anchor(&c,&p);assert(c.dvr_anchored && published==1 && c.dvr_offset==-1759999998000000LL);
    c=initial;p.dvr_seen=0;p.cur_seq_no=3;tube_dvr_anchor(&c,&p);assert(!c.dvr_anchored && published==1);
    c=initial;p.cur_seq_no=2;p.start_seq_no=3;tube_dvr_anchor(&c,&p);assert(!c.dvr_anchored);
    p.start_seq_no=0;packet.pts++;tube_dvr_anchor(&c,&p);assert(!c.dvr_anchored);
    packet.pts=packet.dts;packet.flags=0;tube_dvr_anchor(&c,&p);assert(!c.dvr_anchored);
    packet.flags=AV_PKT_FLAG_KEY;packet.pts=packet.dts=-1;tube_dvr_anchor(&c,&p);assert(!c.dvr_anchored);
    packet.pts=packet.dts=1LL<<33;tube_dvr_anchor(&c,&p);assert(!c.dvr_anchored);
    packet.pts=packet.dts=540000;p.dvr_seen=0;
    AVPacket audio={.pts=541800,.dts=541800,.data=&data};
    AVCodecParameters audio_codec={.codec_type=AVMEDIA_TYPE_AUDIO};
    AVStream audio_stream={.codecpar=&audio_codec};AVStream *audio_streams[]={&audio_stream};
    AVFormatContext audio_format={.nb_streams=1,.streams=audio_streams};
    struct playlist a_track={.n_segments=4,.cur_seq_no=2,.dvr_selected_seq=2,.start_seq_no=0,
        .pkt=&audio,.segments=segments,.ctx=&audio_format,.index=1};
    struct playlist *pair[]={&p,&a_track};c=initial;c.n_playlists=2;c.playlists=pair;
    tube_dvr_anchor(&c,&a_track);assert(!c.dvr_anchored);
    tube_dvr_anchor(&c,&p);assert(c.dvr_anchored && c.dvr_video_playlist==0 && published==2);
    a_track.dvr_seen=p.dvr_seen=0;audio.pts=audio.dts=630000;c=initial;c.n_playlists=2;c.playlists=pair;
    tube_dvr_anchor(&c,&a_track);tube_dvr_anchor(&c,&p);assert(!c.dvr_anchored && c.dvr_disabled);
    return 0;
}
"""
ffmpeg = Path(os.environ['PROSPEROTUBE_DVR_FFMPEG_SOURCE'])
with tempfile.TemporaryDirectory(prefix='tube-dvr-anchor-') as directory:
    test = Path(directory) / 'test.c'; binary = Path(directory) / 'test'
    test.write_text(scaffold + helper + checks)
    subprocess.run(['cc', '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined',
                    '-I'+str(ffmpeg), str(test), str(ffmpeg/'libavutil/libavutil.a'), '-lm', '-pthread', '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True, timeout=5)
print('PASS: initial DVR calibration refuses advanced/expired segment identity and invalid packet timestamps')
