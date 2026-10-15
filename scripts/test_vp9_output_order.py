#!/usr/bin/env python3
"""Replay delayed hidden completions with real identity/queue/decode/drain code."""
from pathlib import Path
import os
import subprocess
import sys
import tempfile
from vp9_output_order_overlay import ABI_C, IDENTITY_C, OUTPUT_C, TAG_HOOK_C, DRAIN_EOF_C

work=Path(os.environ.get('PROSPEROTUBE_NATIVE_WORK',Path.home()/'.cache/prosperotube/native'))
source=Path(sys.argv[1]).read_text() if len(sys.argv)>1 else (work/'projects/evoplayer/media/src/evo_vdec_native.c').read_text()
def function(signature):
    start=source.index(signature);end=source.index('{',start)+1;depth=1
    while depth:
        depth+=(source[end]=='{')-(source[end]=='}');end+=1
    return source[start:end]

decode=function('static int decode_one(')
drain=function('static void drain_decoder(')
# Diagnostic-only fields do not participate in production association.
def production(s):
    return '\n'.join(line for line in s.splitlines() if 'tube_luma_' not in line and '.diagnostic_' not in line and 'tube_picture_diag_output(' not in line)
decode=production(decode);drain=production(drain)
# Before shared native preparation, apply the exact candidate snippets privately.
if 'tube_vp9_input_tag(present)' not in decode:
    a=decode.index('    in.attached =');b=decode.index('    fb.size',a)
    decode=decode[:a]+TAG_HOOK_C+'\n'+decode[b:]
    anchor='        n->tube_vp9_pending[tail].present = present;'
    decode=decode.replace(anchor,anchor+'\n        n->tube_vp9_pending[tail].tag = in.attached;')
if 'pending coded frames' in drain:
    a=drain.index('            if (n->desc->is_vp9 && n->tube_vp9_count)');b=drain.index('            return;',a)+len('            return;')
    drain=drain[:a]+DRAIN_EOF_C+drain[b:]
assert decode.index('tube_vp9_input_tag(present)')<decode.index('sceVideodec2Decode(')
assert decode.index('.tag = in.attached')<decode.index('tube_native_output(n, &out)')
identity = (function('static uint64_t tube_vp9_input_tag(') + '\n' + function('static int tube_vp9_output_tag(')) if 'static uint64_t tube_vp9_input_tag(' in source else IDENTITY_C
output = production(function('static void tube_native_output(')) if 'static uint64_t tube_vp9_input_tag(' in source else OUTPUT_C
reset=function('void evo_vdec_native_flush(')
a=reset.index('    v->pts_n');b=reset.index('    v->au_ring',a);reset=reset[a:b]
assert 'v->tube_vp9_head = v->tube_vp9_count = 0;' in reset
assert 'tube_vp9_identity_sequence' not in reset
program=r"""
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sce/sce_videodec2.h"
#define PTS_POOL 64
#define INPUT_SLOT_BYTES 64
#define PIPELINE_BUFFER_COUNT 4
#define FRAME_POOL_SLOTS 8
#define RO_SLOTS 64
#define HEVC_NAL_IRAP_HI 23
#define NAT_HEVC10 4
#define NAT_VP92 5
#define NWD_DECODE_MS 100
#define NWD_RESET_MS 100
#define POST_FLUSH_ERR_TOLERANCE 5
"""+ABI_C+r"""
typedef struct {int is_vp9,codec_type,idx;} nat_codec_desc;
typedef struct {struct {unsigned profile;} cfg;} dec_slot;
typedef struct evo_vdec_native {
 const nat_codec_desc *desc;dec_slot *slot;
 unsigned au_ring,dec_calls,dec_fails,first_valid_logged,first_err_logged,first_pic_err_logged;
 unsigned since_flush_out,post_flush_errs,frames_out,disp_w,disp_h,tube_drain_calls;
 int fatal,need_irap,dv_strip_logged,tube_drain_finished,ro_count;
 void *dovi,*dec;uint8_t *input_mem,*frame_mem;size_t frame_size;
 int64_t pts_pool[PTS_POOL];int pts_n;
 struct {int64_t pts;int present;uint64_t tag;} tube_vp9_pending[PTS_POOL];
 unsigned tube_vp9_head,tube_vp9_count;int tube_vp9_bsf_pending;
} evo_vdec_native;
static struct {int id,shown,ready;uint64_t tag,pts;} hw[256];
static int queued,tick,published,extra_refs,cache_calls,release_calls,hardware_calls;
static int fail_decode,fail_flush,no_output,omit_hidden,bad_surface,bad_info,api_fail,bad_bounds,lease_fail,choose_last;
static uint64_t override_tag;
static int ids[256];static int64_t timestamps[256];
static void note(const char *s,...) {(void)s;}
#define nwd_start_once() ((void)0)
#define nwd_enter(a,b) ((void)0)
#define nwd_test_hang(a) ((void)0)
#define nwd_leave() ((void)0)
#define drop_undecodable_leading(n,a,s) 0
#define hevc_copy_base_layer(dst,src,size,stripped,dups) (memcpy(dst,src,size),(size))
#define hevc_au_nal_type(a,s) 19
#define tube_native_claim_frame(n,i) ((int)((i)%FRAME_POOL_SLOTS))
#define dovi_note_au(n,a,s,p) ((void)(s))
static int tube_frame_lease_hold(const void *p) {(void)p;if(lease_fail)return -1;++extra_refs;return 1;}
static void tube_frame_lease_release(const void *p) {(void)p;++release_calls;if(extra_refs)--extra_refs;}
static void evo_agc_runtime_cache_flush(const void *p,size_t bytes) {
 assert(extra_refs==1 && ((uintptr_t)p&63u)==0 && bytes==256);++cache_calls;
}
int32_t sceVideodec2GetPictureInfo(const SceVideodec2OutputInfo *out,void *first,void *second) {
 assert(!second && extra_refs==1 && ((uintptr_t)first&15u)==0);
 struct tube_vp9_picture_info *pi=first;assert(pi->size==0x58 && !pi->valid);
 if(api_fail)return -1;
 const uint8_t *m=(const uint8_t *)out->buffer+out->buffer_size-0xf8;
 pi->valid=!bad_info;memcpy(&pi->pts,m+0x10,8);memcpy(&pi->dts,m+0x18,8);memcpy(&pi->attached,m+8,8);
 return 0;
}
static void emit(SceVideodec2FrameBuffer *fb,SceVideodec2OutputInfo *out,int draining) {
 if(no_output)return;
 int found=-1;
 for(int i=0;i<queued;++i)if(draining || hw[i].ready<=tick) {
  if(found<0 || hw[i].ready<hw[found].ready || choose_last)found=i;
 }
 if(found<0)return;
 out->valid=out->picture_count=1;out->width=out->pitch=out->pitch_bytes=32;out->height=32;
 out->codec=SCE_VIDEODEC2_CODEC_VP9;out->buffer=fb->buffer;out->buffer_size=bad_bounds?1024:fb->buffer_size;fb->accepted=1;
 uint8_t *pixels=fb->buffer,*m=pixels+fb->buffer_size-0xf8;
 // A hidden completion leaves recycled pixel content unchanged, exactly the hitch.
 if(hw[found].shown)pixels[0]=(uint8_t)hw[found].id;
 uint64_t tag=override_tag?override_tag:hw[found].tag,dts=UINT64_MAX;
 memcpy(m+8,&tag,8);memcpy(m+0x10,&hw[found].pts,8);memcpy(m+0x18,&dts,8);
 memmove(hw+found,hw+found+1,(size_t)(--queued-found)*sizeof(hw[0]));
}
int32_t sceVideodec2Decode(void *decoder,SceVideodec2InputData *in,SceVideodec2FrameBuffer *fb,SceVideodec2OutputInfo *out) {
 (void)decoder;++tick;++hardware_calls;if(fail_decode)return -1;
 int shown=!in->attached || (in->attached&1);
 if(shown || !omit_hidden) {
  hw[queued].id=*(uint8_t *)in->au;hw[queued].shown=shown;
  hw[queued].ready=tick+(shown?3:16);hw[queued].tag=in->attached;hw[queued].pts=in->pts;++queued;
 }
 emit(fb,out,0);return 0;
}
int32_t sceVideodec2Flush(void *decoder,SceVideodec2FrameBuffer *fb,SceVideodec2OutputInfo *out) {
 (void)decoder;if(fail_flush)return -1;emit(fb,out,1);return 0;
}
"""+function('static void pts_push(')+'\n'+function('static int64_t pts_take(')+r"""
static int tube_native_surface_valid(const evo_vdec_native *n,const SceVideodec2OutputInfo *o,uint32_t p,int ten) {
 (void)p;(void)ten;return !bad_surface && o->valid && o->picture_count==1 && !o->error &&
 (uintptr_t)o->buffer>=(uintptr_t)n->frame_mem &&
 (uintptr_t)o->buffer-(uintptr_t)n->frame_mem<n->frame_size*FRAME_POOL_SLOTS;
}
static void ro_harvest(evo_vdec_native *n,const SceVideodec2OutputInfo *o) {
 ids[published]=*(uint8_t *)o->buffer;timestamps[published++]=pts_take(n);++n->ro_count;
}
"""+identity+output+decode+drain+'\nstatic void seek_metadata(evo_vdec_native *v){\n'+reset+'\n}\n'+r"""
static nat_codec_desc vp9={1,SCE_VIDEODEC2_CODEC_VP9,0},avc={0,SCE_VIDEODEC2_CODEC_AVC,0};
static dec_slot slot;
static uint8_t input[INPUT_SLOT_BYTES*PIPELINE_BUFFER_COUNT];
static uint8_t frames[4096*FRAME_POOL_SLOTS] __attribute__((aligned(256)));
static evo_vdec_native fresh(const nat_codec_desc *desc) {
 evo_vdec_native n={0};n.desc=desc;n.slot=&slot;n.input_mem=input;n.frame_mem=frames;n.frame_size=4096;
 queued=tick=published=extra_refs=cache_calls=release_calls=hardware_calls=0;
 fail_decode=fail_flush=no_output=omit_hidden=bad_surface=bad_info=api_fail=bad_bounds=lease_fail=choose_last=0;override_tag=0;
 memset(frames,0,sizeof(frames));return n;
}
static void submit(evo_vdec_native *n,int id,int shown,int64_t pts) {
 uint8_t byte=(uint8_t)id;assert(decode_one(n,&byte,1,pts,shown)==0);n->ro_count=0;
}
static void assert_bad(int which) {
 evo_vdec_native n=fresh(&vp9);submit(&n,1,1,42);
 if(which==0)bad_info=1;if(which==1)api_fail=1;if(which==2)bad_bounds=1;
 if(which==3)lease_fail=1;if(which==4)override_tag=UINT64_C(123456789);
 drain_decoder(&n);assert(n.fatal && !published && !extra_refs);
}
int main(void) {
 evo_vdec_native n=fresh(&vp9);int expected=0;
 // Exact measured ordering: hidden ordinal2 arrives after visible ordinal14.
 for(int id=1;id<=45;++id) {
  int shown=id!=2 && id!=15 && id!=28;
  submit(&n,id,shown,shown?id*1000:INT64_MIN);
  if(id==17)assert(n.tube_vp9_pending[n.tube_vp9_head].present==0);
 }
 drain_decoder(&n);assert(!n.fatal && n.tube_drain_finished && !n.tube_vp9_count);
 for(int id=1;id<=45;++id)if(id!=2 && id!=15 && id!=28) {
  assert(ids[expected]==id && timestamps[expected]==id*1000);++expected;
 }
 assert(published==expected && !extra_refs);
 // Original equal/NOPTS timestamps survive identity matching; show-existing is shown.
 n=fresh(&vp9);submit(&n,1,1,INT64_MIN);submit(&n,2,0,42);submit(&n,3,1,42);submit(&n,4,1,42);drain_decoder(&n);
 assert(!n.fatal && published==3 && ids[0]==1 && ids[1]==3 && ids[2]==4);
 assert(timestamps[0]==INT64_MIN && timestamps[1]==42 && timestamps[2]==42);
 // Explicit EOF may forget only hidden completions that never materialized.
 n=fresh(&vp9);omit_hidden=1;submit(&n,1,1,0);submit(&n,2,0,INT64_MIN);submit(&n,3,1,1);drain_decoder(&n);
 assert(!n.fatal && n.tube_drain_finished && published==2 && !n.tube_vp9_count);
 n=fresh(&vp9);submit(&n,1,1,0);no_output=1;drain_decoder(&n);assert(n.fatal && !n.tube_drain_finished);
 // Seek never reuses tags. A previous epoch's visible or hidden identity is rejected.
 for(int shown=0;shown<2;++shown) {
  n=fresh(&vp9);submit(&n,1,shown,0);uint64_t old=hw[0].tag;
  seek_metadata(&n);queued=0;submit(&n,2,1,42);assert(hw[0].tag!=old);
  override_tag=old;drain_decoder(&n);assert(n.fatal && !published);
 }
 // Object reuse also cannot accept an old identity.
 n=fresh(&vp9);submit(&n,1,1,0);uint64_t old=hw[0].tag;
 n=fresh(&vp9);submit(&n,2,1,42);override_tag=old;drain_decoder(&n);assert(n.fatal && !published);
 for(int i=0;i<5;++i)assert_bad(i);
 // Unknown hidden identity is rejected, not silently treated as a valid hidden AU.
 n=fresh(&vp9);submit(&n,1,0,0);override_tag=UINT64_C(123456788);drain_decoder(&n);assert(n.fatal && !published);
 n=fresh(&vp9);submit(&n,1,1,1);submit(&n,2,1,2);choose_last=1;drain_decoder(&n);assert(n.fatal && !published);
 // Refusal with pending frames fails closed; bound checked before hardware submission.
 uint8_t byte=1;n=fresh(&vp9);submit(&n,1,1,1);fail_decode=1;
 assert(decode_one(&n,&byte,1,2,1)==-1 && n.fatal);
 n=fresh(&vp9);no_output=1;for(int i=0;i<PTS_POOL;++i)submit(&n,i,1,i);
 assert(decode_one(&n,&byte,1,99,1)==-1 && hardware_calls==PTS_POOL);
 n=fresh(&vp9);submit(&n,1,1,1);fail_flush=1;drain_decoder(&n);assert(n.fatal && !extra_refs);
 // Exhaustion cannot wrap a visible tag back into a previous identity.
 n=fresh(&vp9);tube_vp9_identity_sequence=UINT64_MAX/2;
 assert(decode_one(&n,&byte,1,1,1)==-1 && n.fatal && !hardware_calls && release_calls==1);
 tube_vp9_identity_sequence=UINT64_C(4294967295);
 assert(tube_vp9_input_tag(0)==UINT64_C(8589934592));assert(tube_vp9_input_tag(1)==UINT64_C(8589934595));
 // AVC keeps its existing PTS queue and never queries picture identity.
 n=fresh(&avc);submit(&n,1,1,10);submit(&n,2,1,20);drain_decoder(&n);
 assert(published==2 && timestamps[0]==10 && timestamps[1]==20 && !cache_calls);
 puts("PASS: delayed hidden identities, recycled surfaces, exact visible order, equal/NOPTS, seek/reopen, API faults, bounds and EOF");
}
"""
with tempfile.TemporaryDirectory(prefix='vp9-identity-') as directory:
    temp=Path(directory);(temp/'test.c').write_text(program)
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-misleading-indentation','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(work/'projects/evoplayer/media/include'),str(temp/'test.c'),'-o',str(temp/'test')],check=True)
    subprocess.run([str(temp/'test')],check=True)
