"""Check reproducible pinned HLS overlays without compiling/installing an SDK."""
from pathlib import Path
import hashlib
import importlib.util
import os
import sys
import tempfile
import subprocess

root = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('hls_patch', root / 'scripts/patch_ffmpeg_hls_dvr.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
def check_patch(pristine_path):
    pristine = Path(pristine_path).read_bytes()
    assert hashlib.sha256(pristine).hexdigest() == module.PRISTINE_SHA256
    with tempfile.TemporaryDirectory(prefix='tube-hls-patch-') as directory:
        source = Path(directory) / 'hls.c'
        backup = source.with_name('hls.c.prosperotube-original')
        source.write_bytes(pristine)
        module.apply_file(source)
        expected = source.read_bytes()
        assert backup.read_bytes() == pristine
        assert source.with_name('prosperotube_hls_dvr.inc').read_bytes() == (root/'scripts/ffmpeg_hls_dvr.inc').read_bytes()
        module.apply_file(source)
        assert source.read_bytes() == expected
        # A prior layout is replaced from the checked original, never patched twice.
        source.write_bytes(expected.replace(expected.splitlines()[0], b'/* ProsperoTube HLS DVR overlay: older */', 1))
        module.apply_file(source)
        assert source.read_bytes() == expected
        def refused():
            before=source.read_bytes()
            try: module.apply_file(source)
            except RuntimeError: pass
            else: raise AssertionError('unverified HLS source accepted')
            assert source.read_bytes()==before
        source.write_bytes(expected+b'/* unexpected edit */\n')
        refused()
        source.write_bytes(expected)
        backup.write_bytes(pristine+b'/* damaged original */\n')
        refused()
        backup.unlink()
        source.write_bytes(pristine+b'/* unknown source */\n')
        refused()
        assert not backup.exists()
        source.write_bytes(expected.replace(expected.splitlines()[0], b'/* ProsperoTube HLS DVR overlay: older */', 1))
        refused()
    check_retained_input(expected.decode())
    print('PASS: pinned HLS original, idempotent apply, changed-layout regeneration and corrupt/unknown source refusal')


def test_hls_dvr_patch():
    source = os.environ.get('PROSPEROTUBE_DVR_FFMPEG_SOURCE')
    if not source:
        import pytest
        pytest.skip('Set PROSPEROTUBE_DVR_FFMPEG_SOURCE for the pinned HLS patch check')
    check_patch(Path(source) / 'libavformat/hls.c.prosperotube-original')


def check_retained_input(source):
    """Execute the shipped HLS read callback with an input past its segment list."""
    def function(name):
        start = source.index(name)
        end = source.index('\n}', start) + 2
        return source[start:end]
    functions = '\n'.join(function(name) for name in (
        'static struct segment *current_segment(', 'static struct segment *next_segment(',
        'static int read_from_url(', 'static int read_data('))
    scaffold = r"""
#include <assert.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <stdlib.h>
#define AVERROR_EOF (-100)
#define AVERROR_EXIT (-101)
#define AV_LOG_INFO 0
#define AV_LOG_WARNING 1
#define AV_OPT_SEARCH_CHILDREN 0
#define KEY_NONE 0
#define FFMIN(a,b) ((a)<(b)?(a):(b))
#define FFSWAP(T,a,b) do { T tmp=(a);(a)=(b);(b)=tmp; } while(0)
#define av_log(...) ((void)0)
typedef struct { int unused; } AVIOContext;
typedef struct { void *priv_data; } AVFormatContext;
struct segment { int64_t size; int key_type; const char *url; };
struct playlist {
    AVFormatContext *parent; int needed, input_read_done, finished, index;
    int64_t last_load_time, target_duration, cur_seq_no, start_seq_no, last_seq_no, cur_seg_offset;
    int n_segments, m3u8_hold_counters, input_next_requested, init_sec_buf_read_offset, init_sec_data_len, is_id3_timestamped;
    const char *url; uint8_t *init_sec_buf; AVIOContext *input, *input_next;
    struct segment **segments;
};
typedef struct { int http_persistent, max_reload, m3u8_hold_counters, seg_max_retry, http_multiple;
    int64_t cur_seq_no; void *interrupt_callback; } HLSContext;
static int reads;
static int avio_read(AVIOContext *input,uint8_t *buf,int size) { assert(input && buf); ++reads; return size; }
static int playlist_needed(struct playlist *p) { return p->needed; }
static int64_t default_reload_interval(struct playlist *p) { (void)p;return 1; }
static int64_t av_gettime_relative(void) { return 100; }
static int parse_playlist(HLSContext *c,const char *u,struct playlist *p,void *v) { (void)c;(void)u;(void)p;(void)v;abort(); }
static int ff_check_interrupt(void *p) { (void)p;return 0; }
static void av_usleep(int n) { (void)n;abort(); }
static int update_init_section(struct playlist *p,struct segment *s) { assert(p && s);return 0; }
static int open_input(HLSContext *c,struct playlist *p,struct segment *s,AVIOContext **i) { (void)c;(void)p;(void)s;(void)i;abort(); }
static int av_opt_get(void *p,const char *n,int f,uint8_t **v) { (void)p;(void)n;(void)f;(void)v;abort(); }
static void av_freep(void *p) { (void)p;abort(); }
static int av_strstart(const char *s,const char *p,void *v) { (void)v;return strncmp(s,p,strlen(p))==0; }
static void intercept_id3(struct playlist *p,uint8_t *b,int s,int *r) { (void)p;(void)b;(void)s;(void)r;abort(); }
static void ff_format_io_close(AVFormatContext *p,AVIOContext **i) { (void)p;*i=NULL; }
"""
    checks = r"""
int main(void) {
    HLSContext c={.http_multiple=0,.max_reload=10}; AVFormatContext format={.priv_data=&c};
    AVIOContext input={0}; struct segment segment={.size=-1}; struct segment *segments[]={&segment};
    struct playlist p={.parent=&format,.needed=1,.input=&input,.finished=1,
        .n_segments=1,.start_seq_no=50,.cur_seq_no=51,.segments=segments};
    uint8_t buffer[16];
    /* Crash shape: input remains, but current_segment is NULL at/past EOF. */
    assert(read_data(&p,buffer,sizeof buffer)==AVERROR_EOF && reads==0);
    p.cur_seq_no=55; assert(read_data(&p,buffer,sizeof buffer)==AVERROR_EOF && reads==0);
    p.cur_seq_no=50;p.n_segments=0; assert(read_data(&p,buffer,sizeof buffer)==AVERROR_EOF && reads==0);
    /* Existing valid-segment reads and byte-range limits remain unchanged. */
    p.n_segments=1; assert(read_data(&p,buffer,sizeof buffer)==16 && reads==1 && p.cur_seg_offset==16);
    segment.size=20; assert(read_data(&p,buffer,sizeof buffer)==4 && reads==2 && p.cur_seg_offset==20);
    return 0;
}
"""
    with tempfile.TemporaryDirectory(prefix='tube-hls-read-') as directory:
        c = Path(directory)/'test.c'; binary=Path(directory)/'test'
        c.write_text(scaffold + functions + checks)
        subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',
                        '-fno-omit-frame-pointer',str(c),'-o',str(binary)],check=True)
        subprocess.run([str(binary)],check=True)
    print('PASS: real HLS callback retains EOF safely with stale input and preserves valid/ranged reads')


if __name__ == '__main__':
    if len(sys.argv) != 2:
        raise SystemExit('Usage: test_hls_dvr_patch.py PRISTINE_HLS_C')
    check_patch(sys.argv[1])
