#!/usr/bin/env python3
"""Exercise actual completion hook and bounded raw trailer probe under sanitizers."""
from pathlib import Path
import csv
import os
import subprocess
import tempfile
from vp9_picture_info_diag_overlay import RECORD_C, FIELDS_C, HELPERS_C, TAG_HOOK_C, add_tag_hook, apply

root = Path(__file__).resolve().parents[1]
work = Path(os.environ.get('PROSPEROTUBE_NATIVE_WORK', Path.home()/'.cache/prosperotube/native'))
source = (work/'projects/evoplayer/media/src/evo_vdec_native.c').read_text()
def replace(_path, old, new):
    global source
    assert source.count(old) == 1, old
    source = source.replace(old, new)
if 'struct tube_luma_diag_record {' not in source:
    from luma_identity_diag_overlay import apply as apply_luma
    apply_luma(replace, 'projects/evoplayer/')
if 'struct tube_packet_diag_record {' not in source:
    from vp9_packet_identity_diag_overlay import apply as apply_packet
    apply_packet(replace, 'projects/evoplayer/')
if 'struct tube_picture_diag_record {' not in source:
    apply(replace, 'projects/evoplayer/')
elif TAG_HOOK_C not in source:
    add_tag_hook(replace, 'projects/evoplayer/media/src/evo_vdec_native.c')

def function(signature):
    start = source.index(signature)
    end = source.index('{', start) + 1
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]

decode = function('static int decode_one(')
assert decode.index(TAG_HOOK_C) < decode.index('sceVideodec2Decode(') < decode.index('n->dec_calls++;')
assert 'in.pts      = (uint64_t)pts;' in decode and 'in.dts      = UINT64_MAX;' in decode
output = function('static void tube_native_output(')
assert output.index('tube_native_surface_valid(') < output.index('tube_picture_diag_output(') < output.index('n->tube_vp9_head =') < output.index('if (!n->tube_vp9_pending[head].present)')
close = function('void evo_vdec_native_close(')
assert close.index('if (nwd_hung)') < close.index('tube_picture_diag_dump(v);') < close.index('ro_reset(v);')
flush = function('void evo_vdec_native_flush(')
assert flush.index('if (!v)') < flush.index('++v->tube_picture_epoch;') < flush.index('ro_reset(v);')
assert 'NtXRa3dRzU0' not in HELPERS_C and 'sceVideodec2GetPictureInfo(' not in source
program = r"""
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sce/sce_videodec2.h"
#define FRAME_POOL_SLOTS 8
#define PTS_POOL 64
#define NAT_VP92 4
""" + RECORD_C + r"""
struct descriptor { int is_vp9,idx; };
struct resident_slot { unsigned max_w,max_h; };
typedef struct evo_vdec_native {
    struct descriptor *desc;
    struct resident_slot *slot;
    uint8_t *frame_mem;
    size_t frame_size;
    unsigned dec_calls,tube_vp9_head,tube_vp9_count,frames_out;
    int pts_n,fatal,ro_count;
    struct {int64_t pts;int present;unsigned diagnostic_decode_call;int diagnostic_show_existing;} tube_vp9_pending[PTS_POOL];
    unsigned tube_luma_associated_decode_call;
    int tube_luma_associated_show_existing,tube_luma_trigger_present,tube_luma_trigger_show_existing;
""" + FIELDS_C + r"""
} evo_vdec_native;
static unsigned holds,releases,cache_calls,notes,presents;
static int refuse_lease;
static uint8_t *pool;
static int64_t presented_pts;
static uint64_t now;
static void note(const char *format,...) {(void)format;++notes;}
static uint64_t tube_luma_diag_now_us(void) {return ++now;}
static int tube_frame_lease_hold(const void *p) {assert(p==pool);if(refuse_lease)return -1;++holds;return 1;}
static void tube_frame_lease_release(const void *p) {assert(p==pool && holds>releases);++releases;}
static void evo_agc_runtime_cache_flush(const void *p,size_t n) {
    assert(holds==releases+1);
    assert((uintptr_t)p%64==0 && n==256);
    assert(p==pool+4096-256);++cache_calls;
    // Simulate fresh device metadata becoming CPU-visible only at invalidation.
    uint64_t fresh=83000;memcpy(pool+4096-0xf8+0x10,&fresh,8);
}
static void pts_push(evo_vdec_native *v,int64_t pts) {++v->pts_n;presented_pts=pts;}
static void ro_harvest(evo_vdec_native *v,const SceVideodec2OutputInfo *o) {
    (void)o;assert(v->pts_n==1);v->pts_n=0;++v->ro_count;++presents;
}
""" + HELPERS_C + function('static int tube_native_surface_valid(') + output + r"""
static uint64_t input_tag(evo_vdec_native *n,int present) {
    SceVideodec2InputData in={.pts=12345,.dts=UINT64_MAX};
""" + TAG_HOOK_C + r"""
    assert(in.pts==12345 && in.dts==UINT64_MAX);
    return in.attached;
}
static evo_vdec_native v;
static struct descriptor desc={1,0};
static struct resident_slot resident={64,64};
static void pending(int shown,int64_t pts) {
    unsigned h=v.tube_vp9_head;
    v.tube_vp9_pending[h].pts=pts;v.tube_vp9_pending[h].present=shown;
    v.tube_vp9_pending[h].diagnostic_decode_call=++v.dec_calls;
    v.tube_vp9_pending[h].diagnostic_show_existing=0;v.tube_vp9_count=1;
}
int main(void) {
    pool=aligned_alloc(256,4096*FRAME_POOL_SLOTS);assert(pool);memset(pool,0,4096*FRAME_POOL_SLOTS);
    v.frame_mem=pool;v.frame_size=4096;v.desc=&desc;v.slot=&resident;
    assert(input_tag(&v,0)==2 && input_tag(&v,1)==3 && input_tag(&v,-9)==3);
    v.dec_calls=1;assert(input_tag(&v,0)==4 && input_tag(&v,1)==5);
    v.dec_calls=UINT32_MAX;
    assert(input_tag(&v,0)==UINT64_C(8589934592) && input_tag(&v,1)==UINT64_C(8589934593));
    desc.is_vp9=0;assert(input_tag(&v,1)==0);desc.is_vp9=1;v.dec_calls=0;
    SceVideodec2OutputInfo out={.size=sizeof(out),.valid=1,.picture_count=1,
        .codec=SCE_VIDEODEC2_CODEC_VP9,.width=32,.height=32,.pitch=32,.pitch_bytes=32,
        .buffer=pool,.buffer_size=4096};
    uint8_t *m=pool+4096-0xf8;
    uint32_t five=5,one=1;uint64_t attached=0,pts=42000,dts=UINT64_MAX;
    memcpy(m,&five,4);memcpy(m+0x20,&one,4);memcpy(m+8,&attached,8);
    memcpy(m+0x10,&pts,8);memcpy(m+0x18,&dts,8);m[0x38]=0xa1;m[0x39]=0xb2;m[0x40]=0xc3;
    // Hidden completion is recorded before suppression; metadata is independent
    // of the current FIFO's guessed PTS. No timestamp correction occurs.
    pending(0,42000);v.tube_luma_trigger_present=1;
    tube_native_output(&v,&out);
    assert(v.tube_picture_count==1 && !v.tube_vp9_count && !presents && !v.fatal);
    const struct tube_picture_diag_record *r=&v.tube_picture_records[0];
    assert(!r->associated_present && r->associated_pts==42000 && r->raw_10==83000);
    assert(r->raw_08==0 && r->raw_18==UINT64_MAX && r->trailer_type==5 && r->trailer_valid==1);
    assert(r->raw_38==0xa1 && r->raw_39==0xb2 && r->raw_40==0xc3 && r->lease_held);
    assert(holds==1 && releases==1 && cache_calls==1 && !notes);
    // Same content, another timestamp, and a drain trigger remain distinct rows.
    pending(1,83000);v.tube_luma_trigger_present=v.tube_luma_trigger_show_existing=-1;
    ++v.tube_picture_epoch;tube_native_output(&v,&out);
    assert(v.tube_picture_count==2 && presents==1 && presented_pts==83000);
    assert(v.tube_picture_records[1].epoch==1 && v.tube_picture_records[1].trigger_present==-1);
    assert(v.tube_picture_records[1].raw_10==r->raw_10);
    // Invalid trailer marker remains observable, never changes playback state.
    memset(m,0,4);memset(m+0x20,0,4);pending(1,125000);tube_native_output(&v,&out);
    assert(v.tube_picture_count==3 && !v.tube_picture_records[2].trailer_valid && !v.fatal && presents==2);
    // Failed extra lease performs no cache reads and is explicitly recorded.
    refuse_lease=1;pending(1,167000);tube_native_output(&v,&out);refuse_lease=0;
    assert(v.tube_picture_count==4 && !v.tube_picture_records[3].lease_held && cache_calls==3);
    // Trailer bounds cannot expand the live decoded frame into another slot.
    uintptr_t begin;size_t span;
    assert(tube_picture_diag_bounds(&v,&out,&begin,&span) && span==256);
    uint64_t good=out.buffer_size;
    for(unsigned i=0;i<4;++i) {
        uint64_t sizes[]={0,0xf7,1024,4097};out.buffer_size=sizes[i];
        assert(!tube_picture_diag_bounds(&v,&out,&begin,&span));
    }
    pending(1,208000);tube_native_output(&v,&out);
    assert(v.tube_picture_count==5 && !v.tube_picture_records[4].bounds_valid && cache_calls==3);
    out.buffer_size=good;out.buffer=pool+1;assert(!tube_picture_diag_bounds(&v,&out,&begin,&span));
    out.buffer=pool+4096*FRAME_POOL_SLOTS;assert(!tube_picture_diag_bounds(&v,&out,&begin,&span));
    out.buffer=pool;out.picture_count=2;assert(!tube_picture_diag_bounds(&v,&out,&begin,&span));out.picture_count=1;
    out.size=0x58;assert(!tube_picture_diag_bounds(&v,&out,&begin,&span));out.size=sizeof(out);
    out.codec=1;assert(!tube_picture_diag_bounds(&v,&out,&begin,&span));out.codec=SCE_VIDEODEC2_CODEC_VP9;
    v.frame_mem=(void *)(UINTPTR_MAX-255u);out.buffer=v.frame_mem;
    assert(!tube_picture_diag_bounds(&v,&out,&begin,&span));v.frame_mem=pool;out.buffer=pool;
    // Actual failed surface validation does not enter the diagnostic at all.
    out.error=1;pending(1,250000);tube_native_output(&v,&out);assert(v.fatal && v.tube_picture_count==5);out.error=0;v.fatal=0;
    for(unsigned i=5;i<TUBE_PICTURE_DIAG_CAPACITY;++i){pending(i&1,i);tube_native_output(&v,&out);}
    unsigned before=cache_calls;pending(0,99999);tube_native_output(&v,&out);
    assert(v.tube_picture_count==TUBE_PICTURE_DIAG_CAPACITY && v.tube_picture_omitted==1 && cache_calls==before);
    assert(holds==releases && notes==1); // Only invalid-surface error, no hot-path file I/O.
    tube_picture_diag_dump(&v);assert(notes==2);tube_picture_diag_dump(&v);assert(notes==2);
    free(pool);
}
"""
with tempfile.TemporaryDirectory(prefix='tube-picture-diag-') as directory:
    temp=Path(directory);target=temp/'vp9_picture_info_diag.csv'
    program=program.replace('/data/prosperotube/vp9_picture_info_diag.csv',str(target))
    (temp/'test.c').write_text(program)
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(work/'projects/evoplayer/media/include'),str(temp/'test.c'),'-o',str(temp/'test')],check=True)
    subprocess.run([str(temp/'test')],check=True)
    rows=list(csv.DictReader(target.open()))
    assert len(rows)==2048 and all(None not in row for row in rows)
    assert rows[0]['associated_present']=='0' and rows[0]['raw_10']=='83000'
    assert rows[0]['associated_pts_us']=='42000' and rows[0]['raw_18']==str((1<<64)-1)
    assert rows[1]['epoch']=='1' and rows[1]['trigger_present']=='-1'
    assert rows[3]['lease_held']=='0' and rows[4]['bounds_valid']=='0'
print('PASS: diagnostic input tags (hidden/shown/UINT32_MAX), raw VP9 trailer, hidden/visible/drain hook, cache/lease order, bounds, bad marker, seek epoch, capacity and close-only CSV')
