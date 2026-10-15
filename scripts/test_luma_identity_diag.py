#!/usr/bin/env python3
"""Exercise the sparse sampler and actual decoder recorder without hardware."""
from pathlib import Path
import csv
import os
import subprocess
import tempfile
from luma_identity_diag_overlay import RECORD_C, HELPERS_C, apply

root = Path(__file__).resolve().parents[1]
work = Path(os.environ.get('PROSPEROTUBE_NATIVE_WORK', Path.home()/'.cache/prosperotube/native'))
source = (work/'projects/evoplayer/media/src/evo_vdec_native.c').read_text()
if 'struct tube_luma_diag_record {' not in source:
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

harvest = function('static void ro_harvest(')
assert harvest.index('tube_native_surface_valid(') < harvest.index('tube_frame_lease_hold(') < harvest.index('s->pts      = pts_take(n);') < harvest.index('tube_luma_diag_output(n, s, out);') < harvest.index('s->used     = 1;')
close = function('void evo_vdec_native_close(')
assert close.index('if (nwd_hung)') < close.index('tube_luma_diag_dump(v);') < close.index('ro_reset(v);')

program = r'''
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <sched.h>
#include "tube_luma_diag.h"
''' + RECORD_C + r'''
struct nat_slot { uint8_t *data; unsigned w,h,y_stride; int borrowed,is_10bit; int64_t pts; };
typedef struct { const void *buffer; } SceVideodec2OutputInfo;
struct desc { int is_vp9; };
typedef struct evo_vdec_native {
    const struct desc *desc;
    unsigned dec_calls;
    uint8_t *frame_mem; size_t frame_size;
    struct tube_luma_diag_record tube_luma_records[TUBE_LUMA_DIAG_CAPACITY];
    unsigned tube_luma_count,tube_luma_overflow;
    int tube_luma_dumped;
} evo_vdec_native;
static _Alignas(64) uint8_t pool[3][16384];
static const uint8_t *checked_base,*first_sample;
static unsigned checked_pitch,checked_width,checked_height,flushes,logs;
static void note(const char *format,...) { (void)format;++logs; }
static void evo_agc_runtime_cache_flush(const void *ptr,size_t bytes) {
    const uintptr_t address=(uintptr_t)ptr,base=(uintptr_t)checked_base;
    assert(bytes==64 && !(address&63u) && address>=base);
    const size_t offset=address-base;
    assert(offset/checked_pitch<checked_height && offset%checked_pitch+bytes<=checked_width);
    if(!flushes)first_sample=ptr;
    ++flushes;
}
''' + HELPERS_C + r'''
static void shape(unsigned pitch,unsigned width,unsigned height,int ten_bit) {
    checked_base=pool[1];checked_pitch=pitch;checked_width=width*(ten_bit?2:1);checked_height=height;flushes=0;
}
static uint64_t fingerprint(unsigned pitch,unsigned width,unsigned height,int ten_bit) {
    uint64_t hash=0;shape(pitch,width,height,ten_bit);
    assert(tube_luma_diag_sample(pool[1],(int)pitch,width,height,ten_bit,evo_agc_runtime_cache_flush,&hash));
    assert(flushes==16*9);return hash;
}
static evo_vdec_native concurrent;
static void *produce(void *unused) {
    (void)unused;
    struct nat_slot s={pool[1],256,37,320,1,0,0};SceVideodec2OutputInfo out={pool[1]};
    for(unsigned i=0;i<200;++i){s.pts=i;concurrent.dec_calls=i+1;tube_luma_diag_output(&concurrent,&s,&out);}
    return NULL;
}
int main(void) {
    for(unsigned i=0;i<sizeof pool;++i)((uint8_t*)pool)[i]=(uint8_t)(i*17u+(i>>8));
    uint64_t a=fingerprint(320,256,37,0),hash=0;
    assert(fingerprint(320,256,37,0)==a);
    uint8_t *changed=(uint8_t*)first_sample;*changed^=1;
    assert(fingerprint(320,256,37,0)!=a);*changed^=1;
    /* Padding and coded rows outside visible height do not affect the sample. */
    pool[1][319]^=1;pool[1][37*320]^=1;
    assert(fingerprint(320,256,37,0)==a);
    a=fingerprint(384,160,37,1);changed=(uint8_t*)first_sample;changed[1]^=1;
    assert(fingerprint(384,160,37,1)!=a);changed[1]^=1;
    shape(320,256,37,0);
    assert(!tube_luma_diag_sample(NULL,320,256,37,0,evo_agc_runtime_cache_flush,&hash));
    assert(!tube_luma_diag_sample(pool[1],-1,256,37,0,evo_agc_runtime_cache_flush,&hash));
    assert(!tube_luma_diag_sample(pool[1],320,321,37,0,evo_agc_runtime_cache_flush,&hash));
    assert(!tube_luma_diag_sample(pool[1],320,31,37,0,evo_agc_runtime_cache_flush,&hash));
    assert(!tube_luma_diag_sample(pool[1]+1,64,64,1,0,evo_agc_runtime_cache_flush,&hash));
    assert(!tube_luma_diag_sample((void*)(UINTPTR_MAX-63),320,256,37,0,evo_agc_runtime_cache_flush,&hash));
    assert(!flushes);
    static evo_vdec_native n;
    static const struct desc vp9={1};n.desc=&vp9;n.frame_mem=pool[0];n.frame_size=sizeof pool[0];
    struct nat_slot s={pool[1],256,37,320,1,0,1000};SceVideodec2OutputInfo out={pool[1]};
    shape(320,256,37,0);n.dec_calls=4;tube_luma_diag_output(&n,&s,&out);
    assert(n.tube_luma_count==1 && flushes==144 && !logs);
    assert(n.tube_luma_records[0].slot==1 && n.tube_luma_records[0].valid);
    assert(n.tube_luma_records[0].hash==fingerprint(320,256,37,0));
    /* Equal content at distinct timestamps stays distinct metadata, not a false mutation. */
    s.pts=2000;n.dec_calls=5;shape(320,256,37,0);tube_luma_diag_output(&n,&s,&out);
    assert(n.tube_luma_count==2 && n.tube_luma_records[1].pts==2000);
    assert(n.tube_luma_records[0].hash==n.tube_luma_records[1].hash);
    changed=(uint8_t*)first_sample;*changed^=1;
    assert(fingerprint(320,256,37,0)!=n.tube_luma_records[1].hash);*changed^=1;
    unsigned before=flushes;n.tube_luma_count=TUBE_LUMA_DIAG_CAPACITY;
    tube_luma_diag_output(&n,&s,&out);
    assert(n.tube_luma_count==TUBE_LUMA_DIAG_CAPACITY && n.tube_luma_overflow==1 && flushes==before);
    n.tube_luma_count=2;s.borrowed=0;tube_luma_diag_output(&n,&s,&out);
    assert(n.tube_luma_count==2 && flushes==before);
    assert(!fopen(TUBE_LUMA_DIAG_PATH,"r")); /* Recording has done no file I/O. */
    tube_luma_diag_dump(&n);assert(logs==1);tube_luma_diag_dump(&n);assert(logs==1);
    assert(!rename(TUBE_LUMA_DIAG_PATH,TUBE_LUMA_DIAG_PATH ".first"));
    /* Close may snapshot while an abandoned worker finishes later rows. */
    concurrent.desc=&vp9;concurrent.frame_mem=pool[0];concurrent.frame_size=sizeof pool[0];
    shape(320,256,37,0);pthread_t worker;assert(!pthread_create(&worker,NULL,produce,NULL));
    while(__atomic_load_n(&concurrent.tube_luma_count,__ATOMIC_ACQUIRE)<10)sched_yield();
    tube_luma_diag_dump(&concurrent);
    assert(!pthread_join(worker,NULL));
    assert(__atomic_load_n(&concurrent.tube_luma_count,__ATOMIC_ACQUIRE)==200 && logs==2);
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='tube-luma-identity-') as temporary:
    temp=Path(temporary);output=temp/'decoder_luma_diag.csv'
    test=temp/'test.c';exe=temp/'test'
    test.write_text(program.replace('/data/prosperotube/decoder_luma_diag.csv',str(output)))
    subprocess.run(['cc','-std=c11','-pthread','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',
        '-I'+str(root/'native'),str(test),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
    with output.with_suffix('.csv.first').open() as stream:rows=list(csv.DictReader(stream))
    assert len(rows)==2 and [int(r['pts_us']) for r in rows]==[1000,2000]
    assert rows[0]['luma_hash']==rows[1]['luma_hash'] and rows[0]['sequence']!=rows[1]['sequence']
    with output.open() as stream:snapshot=list(csv.DictReader(stream))
    assert 10<=len(snapshot)<=200
    for index,row in enumerate(snapshot):
        assert int(row['sequence'])==index+1 and int(row['pts_us'])==index
        assert row['luma_hash']==rows[0]['luma_hash'] and row['hash_valid']=='1'
print('PASS: coherent bounded sparse luma, crop/pitch/depth bounds, mutation detection, equal-content timestamps and close-only CSV')
