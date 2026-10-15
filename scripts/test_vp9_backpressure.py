#!/usr/bin/env python3
"""Exercise generated send/receive with real reorder bounds and retained BSF bursts."""
from pathlib import Path
import os
import re
import subprocess
import sys
import tempfile

work = Path(os.environ.get('PROSPEROTUBE_NATIVE_WORK', str(Path.home() / '.cache/prosperotube/native')))
source_path = Path(sys.argv[1]) if len(sys.argv) > 1 else work / 'projects/evoplayer/media/src/evo_vdec_native.c'
source = source_path.read_text()
# Keep the model's capacity tied to production, where receive retains four slots.
assert re.search(r'#define\s+EVO_VDEC_REORDER_DEPTH\s+4\b', source)
assert re.search(r'#define\s+RO_SLOTS\s+\(EVO_VDEC_REORDER_DEPTH \+ 1\)', source)
assert re.search(r'#define\s+FRAME_POOL_SLOTS\s+8\b', source)


def function(signature):
    start = source.index(signature)
    end = source.index('{', start) + 1
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


program = r'''
#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define PTS_POOL 16
#define INPUT_SLOT_BYTES 64
#define PIPELINE_BUFFER_COUNT 4
#define FRAME_POOL_SLOTS 8
#define EVO_VDEC_REORDER_DEPTH 4
#define RO_SLOTS (EVO_VDEC_REORDER_DEPTH + 1)
#define SCE_VIDEODEC2_CODEC_HEVC 2
#define HEVC_NAL_IRAP_HI 23
#define NAT_HEVC10 4
#define NAT_VP92 5
#define NWD_DECODE_MS 100
#define NWD_RESET_MS 100
#define POST_FLUSH_ERR_TOLERANCE 5
typedef struct { size_t size; void *au; uint64_t au_size, pts, dts; int attached; } SceVideodec2InputData;
typedef struct { size_t size, buffer_size; void *buffer; unsigned accepted; } SceVideodec2FrameBuffer;
typedef struct { size_t size; void *buffer; unsigned valid, picture_count, error, width, height, pitch, pitch_bytes; } SceVideodec2OutputInfo;
typedef struct { int is_vp9, codec_type, idx; } nat_codec_desc;
typedef struct { struct { unsigned profile; } cfg; void *decoder; } dec_slot;
#define AV_NOPTS_VALUE INT64_MIN
#define AVERROR_EOF -2
#define AVERROR(e) (-(e))
#define EAGAIN 11
#define PP_FRAME_NV12_10 3
#define PP_FRAME_NV12 2
#define PP_FRAME_YUV420P 1
#define DOVI_RING 1
typedef int evo_dovi_params;
typedef struct { uint8_t bytes[32]; uint8_t *data; int size; int64_t pts,dts; } AVPacket;
typedef struct { uint8_t bytes[32]; int count, next, eof, sends; int64_t pts; int error; } AVBSFContext;
typedef struct { int format,width,height,coded_height,strides[3],color_trc,dovi_size; uint8_t *planes[3]; int64_t pts_us; const void *dovi; } pp_frame;
struct nat_slot { int used,is_10bit,nv12,w,h,coded_h,borrowed; uint8_t *data; unsigned u_off,v_off,y_stride,c_stride; int64_t pts; };
typedef struct evo_vdec_native {
    AVBSFContext *bsf;
    AVPacket *in_pkt, *filt_pkt;
    struct nat_slot ro[RO_SLOTS];
    void *tube_received;
    int color_trc, flushing;
    int tube_vp9_bsf_pending;
    const char *bsf_name; void *bsf_par;
    uint8_t *annexb_extradata; int annexb_extradata_size;
    int drop_leading, leading_dropped;
    struct { int valid; } dovi_ring[DOVI_RING];
    const nat_codec_desc *desc;
    dec_slot *slot;
    unsigned au_ring, dec_calls, dec_fails, first_valid_logged, first_err_logged;
    unsigned first_pic_err_logged, since_flush_out, post_flush_errs, frames_out;
    unsigned disp_w, disp_h, tube_drain_calls;
    int fatal, need_irap, dv_strip_logged, tube_drain_finished, ro_count;
    void *dovi, *dec;
    uint8_t *input_mem, *frame_mem;
    size_t frame_size;
    int64_t pts_pool[PTS_POOL]; int pts_n;
    struct { int64_t pts; int present; } tube_vp9_pending[PTS_POOL];
    unsigned tube_vp9_head, tube_vp9_count;
} evo_vdec_native;
static uint8_t pictures[256][2];
static int hardware[256], queued;
static int decode_calls, overflows, peak_ro, bsf_receives;
static void note(const char *format, ...) { (void)format; }
#define nwd_start_once() ((void)0)
#define nwd_enter(a,b) ((void)0)
#define nwd_test_hang(a) ((void)0)
#define nwd_leave() ((void)0)
#define drop_undecodable_leading(n,a,s) 0
#define hevc_copy_base_layer(dst,src,size,stripped,dups) (memcpy(dst,src,size), (size))
#define hevc_au_nal_type(a,s) 19
#define tube_native_claim_frame(n,i) ((int)((i) % FRAME_POOL_SLOTS))
#define dovi_note_au(n,a,s,p) ((void)(s))
/* Lease ownership itself is covered by test_native_frame_lease.py. */
static void tube_frame_lease_release(const void *p) { (void)p; }
static void output(SceVideodec2OutputInfo *out) {
    out->valid = out->picture_count = 1;
    pictures[hardware[0]][0] = (uint8_t)hardware[0];
    out->buffer = pictures[hardware[0]];
    memmove(hardware, hardware + 1, (size_t)--queued * sizeof hardware[0]);
}
static int sceVideodec2Decode(void *dec, SceVideodec2InputData *in, SceVideodec2FrameBuffer *fb, SceVideodec2OutputInfo *out) {
    (void)dec; decode_calls++;
    hardware[queued++] = ((uint8_t *)in->au)[1];
    if (queued >= 4) { output(out); fb->accepted = 1; }
    return 0;
}
static int sceVideodec2Flush(void *dec, SceVideodec2FrameBuffer *fb, SceVideodec2OutputInfo *out) {
    (void)dec; (void)fb;
    if (queued) output(out);
    return 0;
}
'''
program += function('static void pts_push(') + '\n' + function('static int64_t pts_take(')
program += r'''
static int tube_native_surface_valid(const evo_vdec_native *n, const SceVideodec2OutputInfo *out, uint32_t pitch, int ten_bit) {
    (void)n; (void)pitch; (void)ten_bit;
    return out->valid && out->picture_count == 1 && !out->error && out->buffer;
}
static void ro_harvest(evo_vdec_native *n, const SceVideodec2OutputInfo *out) {
    for (int i = 0; i < RO_SLOTS; i++) if (!n->ro[i].used) {
        n->ro[i] = (struct nat_slot){.used=1, .data=out->buffer, .pts=pts_take(n)};
        n->ro_count++;
        if (n->ro_count > peak_ro) peak_ro = n->ro_count;
        return;
    }
    overflows++;
}
static void av_packet_unref(AVPacket *p) { p->size=0; p->data=NULL; }
static int av_new_packet(AVPacket *p, int size) {
    assert(size <= (int)sizeof p->bytes); p->data=p->bytes; p->size=size; return 0;
}
static int av_bsf_send_packet(AVBSFContext *b, AVPacket *p) {
    if (b->next < b->count) return AVERROR(EAGAIN);
    if (b->eof) return AVERROR_EOF;
    if (!p) { b->eof=1; return 0; }
    assert(p->size % 2 == 0 && p->size <= 16);
    memcpy(b->bytes,p->data,(size_t)p->size); b->count=p->size/2;
    b->next=0; b->pts=p->pts; b->sends++; av_packet_unref(p); return 0;
}
static int av_bsf_receive_packet(AVBSFContext *b, AVPacket *p) {
    if (b->error) return b->error;
    if (b->next == b->count) return b->eof ? AVERROR_EOF : AVERROR(EAGAIN);
    av_new_packet(p,2); memcpy(p->data,b->bytes+b->next*2,2);
    p->pts=b->pts; p->dts=AV_NOPTS_VALUE; b->next++; bsf_receives++; return 0;
}
static void av_bsf_flush(AVBSFContext *b) { memset(b,0,sizeof *b); }
static void ro_reset(evo_vdec_native *n) { memset(n->ro,0,sizeof n->ro); n->ro_count=0; }
static int bsf_build(evo_vdec_native *n) { av_bsf_flush(n->bsf); return 0; }
static int slot_renew_decoder(dec_slot *s) { (void)s; queued=0; return 0; }
static int sceVideodec2Reset(void *d) { (void)d; queued=0; return 0; }
static void evo_dovi_parser_flush(void *d) { (void)d; }
static const void *dovi_for_pts(evo_vdec_native *n,int64_t p) { (void)n; (void)p; return NULL; }
'''
for signature in ['static void tube_native_output(', 'static int decode_one(', 'static void drain_decoder(',
                  'static int bitrd(', 'static int vp9_frame_is_shown(', 'static int pkt_present(',
                  'static void tube_vp9_pump_bsf(', 'static int tube_vp9_send(',
                  'int evo_vdec_native_send(', 'int evo_vdec_native_receive(',
                  'int evo_vdec_native_drained(', 'void evo_vdec_native_flush(']:
    if signature in source:
        program += '\n' + function(signature) + '\n'
program += r'''
static nat_codec_desc vp9 = {1,0,0};
static dec_slot slot;
static uint8_t inputs[PIPELINE_BUFFER_COUNT * INPUT_SLOT_BYTES], frames[128];
static AVPacket input, filtered;
static AVBSFContext bsf;
static int expected[256], expected_count, received_count;
static int64_t expected_pts[256];
static evo_vdec_native fresh(void) {
    evo_vdec_native n = {0};
    n.desc=&vp9; n.slot=&slot; n.dec=&slot; n.input_mem=inputs; n.frame_mem=frames; n.frame_size=16;
    n.bsf=&bsf; n.in_pkt=&input; n.filt_pkt=&filtered;
    memset(&bsf,0,sizeof bsf); memset(&input,0,sizeof input); memset(&filtered,0,sizeof filtered);
    queued=decode_calls=overflows=peak_ro=bsf_receives=0;
    expected_count=received_count=0;
    return n;
}
static int receive_one(evo_vdec_native *n) {
    pp_frame out;
    int rc=evo_vdec_native_receive(n,&out);
    if (n->fatal || overflows)
        fprintf(stderr,"capacity failure: ro=%d overflow=%d decoded=%d BSF remaining=%d\n",n->ro_count,overflows,decode_calls,bsf.count-bsf.next);
    assert(rc>=0 && !n->fatal && !overflows && n->ro_count<=RO_SLOTS);
    if (rc) {
        assert(received_count<expected_count);
        if (out.planes[0][0]!=expected[received_count])
            fprintf(stderr,"picture order: expected=%d got=%d at PTS=%lld\n",
                    expected[received_count],out.planes[0][0],(long long)out.pts_us);
        assert(out.planes[0][0]==expected[received_count]);
        assert(out.pts_us==expected_pts[received_count]);
        received_count++;
    }
    return rc;
}
static void receive_available(evo_vdec_native *n) {
    for (int guard=0;guard<300;guard++) if (!receive_one(n)) return;
    assert(!"receive did not stop");
}
static int send_packet(evo_vdec_native *n,int first,int amount,unsigned hidden,int64_t pts) {
    uint8_t bytes[16];
    for (int i=0;i<amount;i++) { bytes[i*2]=(hidden&(1u<<i))?0x84:0x86; bytes[i*2+1]=(uint8_t)(first+i); }
    int sends=bsf.sends;
    int rc=evo_vdec_native_send(n,bytes,amount*2,pts);
    if (n->fatal || overflows)
        fprintf(stderr,"capacity failure on %d-AU packet: ro=%d overflow=%d decoded=%d\n",amount,n->ro_count,overflows,decode_calls);
    assert(rc>=0 && !n->fatal && !overflows && n->ro_count<=RO_SLOTS);
    if (rc==0) {
        assert(bsf.sends==sends+1);
        for (int i=0;i<amount;i++) if (!(hidden&(1u<<i))) {
            expected[expected_count]=first+i; expected_pts[expected_count++]=pts;
        }
    } else assert(bsf.sends==sends);
    return rc;
}
static void submit(evo_vdec_native *n,int first,int amount,unsigned hidden,int64_t pts) {
    for (int guard=0;guard<300;guard++) {
        if (send_packet(n,first,amount,hidden,pts)==0) return;
        assert(receive_one(n)==1);
    }
    assert(!"send backpressure did not clear");
}
static void finish(evo_vdec_native *n) {
    for (int guard=0;guard<300;guard++) {
        int rc=evo_vdec_native_send(n,NULL,0,0);
        assert(rc>=0 && !n->fatal);
        if (rc==0) break;
        assert(receive_one(n)==1);
        assert(guard<299);
    }
    receive_available(n);
    assert(n->tube_drain_finished && !n->ro_count && !n->tube_vp9_count);
    assert(received_count==expected_count && !overflows);
    assert(decode_calls==bsf_receives);
}
static void prime(evo_vdec_native *n) {
    for (int id=1;id<=8;id++) { submit(n,id,1,0,id*1000); receive_available(n); }
    assert(n->ro_count==EVO_VDEC_REORDER_DEPTH);
}
int main(void) {
    evo_vdec_native n=fresh(); prime(&n);
    /* Both delayed outputs can be visible even though the new packet starts hidden. */
    submit(&n,9,2,1,9000); receive_available(&n); finish(&n);
    assert(peak_ro==RO_SLOTS);
    /* A maximal superframe remains in the BSF; retries must not drop or resend its AUs. */
    n=fresh(); prime(&n); submit(&n,9,8,0x55,9000);
    int before=bsf.sends;
    assert(send_packet(&n,17,1,0,17000)==1 && bsf.sends==before);
    receive_available(&n); submit(&n,17,1,0,17000); finish(&n);
    assert(peak_ro==RO_SLOTS);
    /* EOF arrives while a maximal burst is still buffered; tail pictures survive. */
    n=fresh(); prime(&n); submit(&n,9,8,0x55,9000); finish(&n);
    /* The maximum visible-output burst also fits without growing the reorder queue. */
    n=fresh(); prime(&n); submit(&n,9,8,0,9000); finish(&n);
    /* Seek cancels unconsumed BSF AUs as well as decoder metadata and output slots. */
    n=fresh(); prime(&n); submit(&n,9,8,0x55,9000);
    assert(bsf.next<bsf.count);
    evo_vdec_native_flush(&n);
    assert(!n.fatal && !n.tube_vp9_count && !n.ro_count && bsf.next==bsf.count && !queued);
    expected_count=received_count=decode_calls=bsf_receives=0;
    submit(&n,30,2,1,30000); finish(&n);
    /* Unexpected BSF errors are fatal, never mistaken for ordinary backpressure. */
    n=fresh(); bsf.error=-99;
    const uint8_t one[2]={0x86,1};
    assert(evo_vdec_native_send(&n,one,2,1000)<0 && n.fatal);
    puts("VP9 actual send/receive: RO5, split bursts, retry, EOF and seek: PASS");
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='vp9-backpressure-') as directory:
    path = Path(directory) / 'test.c'
    exe = Path(directory) / 'test'
    path.write_text(program)
    subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-Wno-unused-function', '-fsanitize=undefined', str(path), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
