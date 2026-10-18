#!/usr/bin/env python3
"""Run the actual VP9 send hook with destructive BSF ownership and real SHA256."""
from pathlib import Path
import csv
import hashlib
import os
import subprocess
import tempfile
from vp9_packet_identity_diag_overlay import RECORD_C, FIELDS_C, HELPERS_C, apply

root = Path(__file__).resolve().parents[1]
work = Path(os.environ.get('PROSPEROTUBE_NATIVE_WORK', Path.home()/'.cache/prosperotube/native'))
source = (work/'projects/evoplayer/media/src/evo_vdec_native.c').read_text()
if 'struct tube_packet_diag_record {' not in source:
    def replace(_path, old, new):
        global source
        assert source.count(old) == 1, old
        source = source.replace(old, new)
    apply(replace, 'projects/evoplayer/')

def function(signature):
    start = source.index(signature)
    end = source.index('{', start) + 1
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]

send = function('static int tube_vp9_send(')
assert send.index('tube_packet_diag_prepare(') < send.index('av_bsf_send_packet(v->bsf, v->in_pkt)') < send.index('tube_packet_diag_accept(')
flush = function('void evo_vdec_native_flush(')
assert flush.index('if (!v)') < flush.index('tube_packet_diag_seek(v);') < flush.index('ro_reset(v);')
close = function('void evo_vdec_native_close(')
assert close.index('if (nwd_hung)') < close.index('tube_packet_diag_dump(v);') < close.index('ro_reset(v);')
assert close.index('tube_native_drain_for_close(v)') < close.index('av_free(v->tube_packet_sha);') < close.index('free(v);')
# Execute the actual seek entry hook; downstream hardware reset is covered by video-drain tests.
flush_entry = flush[:flush.index('    ro_reset(v);')] + '}\n'

program = r"""
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <libavutil/sha.h>
#include <libavutil/mem.h>
#define AV_NOPTS_VALUE INT64_MIN
#define AVERROR(e) (-(e))
#define AVERROR_EOF -12345
#define EVO_VDEC_REORDER_DEPTH 4
struct Packet { uint8_t *data; int size; int64_t pts,dts; };
""" + RECORD_C + r"""
typedef struct evo_vdec_native {
    void *bsf;
    struct Packet *in_pkt;
    int fatal,flushing,ro_count,tube_vp9_bsf_pending;
""" + FIELDS_C + r"""
} evo_vdec_native;
static int bsf_result,sha_calls,alloc_fail,pump_hold,drains,notes,accepted;
static struct AVSHA *test_sha_alloc(void) {return alloc_fail?NULL:av_sha_alloc();}
static void test_sha_update(struct AVSHA *sha,const uint8_t *data,size_t size) {
    ++sha_calls; av_sha_update(sha,data,size);
}
#define av_sha_alloc test_sha_alloc
#define av_sha_update test_sha_update
static void note(const char *format,...) {(void)format;++notes;}
static void tube_vp9_pump_bsf(evo_vdec_native *v) {if(!pump_hold)v->tube_vp9_bsf_pending=0;}
static void drain_decoder(evo_vdec_native *v) {(void)v;++drains;}
static void av_packet_unref(struct Packet *p) {free(p->data);memset(p,0,sizeof(*p));}
static int av_new_packet(struct Packet *p,int size) {p->data=malloc((size_t)size);p->size=size;return p->data?0:-1;}
static int av_bsf_send_packet(void *bsf,struct Packet *p) {
    assert(bsf);
    if(bsf_result<0)return bsf_result;
    if(p){++accepted;memset(p->data,0xcc,(size_t)p->size);av_packet_unref(p);}
    return 0;
}
""" + HELPERS_C + send + flush_entry + r"""
static struct Packet packet;
static evo_vdec_native v;
static const uint8_t abc[]={'a','b','c'}, abd[]={'a','b','d'};
int main(void) {
    v.bsf=&v;v.in_pkt=&packet;
    // Hashing alone neither publishes an accepted row nor writes a file.
    struct tube_packet_diag_record pending;
    assert(tube_packet_diag_prepare(&v,abc,3,0,&pending));
    assert(!v.tube_packet_count && !v.tube_packet_epoch_count);
    tube_packet_diag_dump(&v);assert(!notes);
    assert(tube_vp9_send(&v,abc,3,0)==0 && v.tube_packet_count==1 && accepted==1);
    assert(v.tube_packet_records[0].packet==1 && !v.tube_packet_records[0].epoch);
    assert(v.tube_packet_records[0].epoch_packet==1 && v.tube_packet_records[0].valid);
    // Same PTS, different bytes remains a distinct packet.
    assert(tube_vp9_send(&v,abd,3,0)==0 && v.tube_packet_count==2);
    assert(memcmp(v.tube_packet_records[0].sha256,v.tube_packet_records[1].sha256,32));
    bsf_result=AVERROR(EAGAIN);assert(tube_vp9_send(&v,abc,3,1)==1);
    assert(v.tube_packet_count==2 && v.tube_packet_epoch_count==2);
    bsf_result=0;assert(tube_vp9_send(&v,abc,3,1)==0 && v.tube_packet_count==3);
    bsf_result=-9;assert(tube_vp9_send(&v,abc,3,2)==-1);
    assert(v.tube_packet_count==3 && v.tube_packet_epoch_count==3);
    bsf_result=0;v.fatal=0;
    // Existing retained BSF/reorder backpressure does not hash an unaccepted retry.
    int before=sha_calls;v.tube_vp9_bsf_pending=1;pump_hold=1;
    assert(tube_vp9_send(&v,abc,3,3)==1 && sha_calls==before);
    pump_hold=0;v.ro_count=5;assert(tube_vp9_send(&v,abc,3,3)==1 && sha_calls==before);v.ro_count=0;
    // Seek keeps prior evidence and starts a new sequence, including NOPTS.
    evo_vdec_native_flush(NULL);evo_vdec_native_flush(&v);
    assert(v.tube_packet_epoch==1 && !v.tube_packet_epoch_count && v.tube_packet_count==3);
    assert(tube_vp9_send(&v,abc,3,INT64_MIN)==0);
    assert(v.tube_packet_records[3].epoch==1 && v.tube_packet_records[3].epoch_packet==1);
    assert(v.tube_packet_records[3].pts==INT64_MIN);
    before=sha_calls;
    bsf_result=AVERROR(EAGAIN);assert(tube_vp9_send(&v,NULL,0,0)==1);
    bsf_result=0;assert(tube_vp9_send(&v,NULL,0,0)==0);
    assert(tube_vp9_send(&v,NULL,0,0)==0 && sha_calls==before && v.tube_packet_count==4);
    assert(drains==1);v.flushing=0;
    // SHA allocation failure is explicit evidence, never a playback failure.
    av_free(v.tube_packet_sha);v.tube_packet_sha=NULL;alloc_fail=1;
    assert(tube_vp9_send(&v,abc,3,4)==0 && !v.tube_packet_records[4].valid && !v.fatal);
    alloc_fail=0;
    for(unsigned i=5;i<TUBE_PACKET_DIAG_CAPACITY;++i)assert(tube_vp9_send(&v,abc,3,i)==0);
    before=sha_calls;
    assert(tube_vp9_send(&v,abc,3,9999)==0 && v.tube_packet_omitted==1 && sha_calls==before);
    bsf_result=AVERROR(EAGAIN);assert(tube_vp9_send(&v,abc,3,9999)==1 && v.tube_packet_omitted==1);
    assert(v.tube_packet_count==TUBE_PACKET_DIAG_CAPACITY && !notes);
    tube_packet_diag_dump(&v);assert(notes==1);
    tube_packet_diag_dump(&v);assert(notes==1);
    av_packet_unref(&packet);av_free(v.tube_packet_sha);
}
"""
with tempfile.TemporaryDirectory(prefix='tube-packet-diag-') as directory:
    temp=Path(directory); target=temp/'vp9_packet_diag.csv'
    program=program.replace('/data/youtube-maxd/vp9_packet_diag.csv',str(target))
    (temp/'test.c').write_text(program)
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer',str(temp/'test.c'),'-lavutil','-o',str(temp/'test')],check=True)
    subprocess.run([str(temp/'test')],check=True)
    rows=list(csv.DictReader(target.open()))
    assert len(rows)==2048
    for row,data in zip(rows[:4], [b'abc',b'abd',b'abc',b'abc']):
        assert row['sha256_valid']=='1' and row['sha256']==hashlib.sha256(data).hexdigest()
    assert rows[3]['epoch']=='1' and rows[3]['epoch_packet']=='1' and rows[3]['pts_us']==str(-(1<<63))
    assert rows[4]['sha256_valid']=='0' and rows[4]['sha256']=='0'*64
print('PASS: accepted VP9 packet SHA256, equal PTS, EAGAIN/errors/EOF, pre-BSF ownership, seek epochs, capacity, allocation failure and close-only CSV')
