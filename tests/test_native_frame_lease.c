/* Actual native slot claim + pp publication/snapshot with the real fixed lease table.
 * Copyright (C) 2026 BlackBearReloaded. SPDX-License-Identifier: GPL-3.0-or-later */
#include "tube_frame_lease.h"
#include "pp_playback.h"
#include "evo_dovi.h"
#include "sce/sce_videodec2.h"
#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#include <pthread.h>
#include <unistd.h>
#define FRAME_POOL_SLOTS 8
#define NWD_DECODE_MS 10000
#include "native_constants.inc"
enum{NAT_HEVC10=2,NAT_VP92=4};
struct dec_slot{void*frame_mem;int owned,ready;uint32_t max_w,max_h;};
struct desc{int idx,is_vp9;};
struct nat_slot{uint8_t*data;size_t cap;int used,borrowed,nv12,is_10bit;int64_t pts;uint32_t w,h,coded_h,y_stride,c_stride;size_t u_off,v_off;};
typedef struct{void*frame_mem;int fatal;size_t frame_size;const void*tube_received;int flushing,tube_drain_finished,ro_count,color_trc,nv12_out;uint32_t disp_w,disp_h;
 struct dec_slot*slot;struct desc*desc;void*dovi;struct nat_slot ro[RO_SLOTS];}evo_vdec_native;
static int64_t fake_ms;
static const void *release_while_waiting;
static int64_t nwd_now_ms(void){return fake_ms;}
static int fake_usleep(unsigned ignored){(void)ignored;++fake_ms;if(release_while_waiting){tube_frame_lease_release(release_while_waiting);release_while_waiting=NULL;}return 0;}
static void note(const char*format,...){(void)format;}
void evo_boot_log(const char*format,...){(void)format;}
void pp_stage_bc_checkpoint(const char*stage,const char*detail){(void)stage;(void)detail;}
#define usleep fake_usleep
#include "native_claim.inc"
#include "native_idle.inc"
static int64_t pts_take(evo_vdec_native*n){(void)n;return 1000000;}
static void blend_fields(const SceVideodec2OutputInfo*out,uint32_t pitch){(void)out;(void)pitch;assert(0);}
static const evo_dovi_params*dovi_for_pts(evo_vdec_native*n,int64_t pts){(void)n;(void)pts;return NULL;}
static void drain_decoder(evo_vdec_native*n){(void)n;}
#include "native_surface.inc"
#include "native_harvest.inc"
#include "native_receive.inc"
#include "native_reset.inc"
#undef usleep
static int64_t media_clock(void*ctx){return *(int64_t*)ctx;}
static pp_frame frame(uint8_t*base,unsigned slot){pp_frame f={0};f.width=64;f.height=f.coded_height=16;f.format=PP_FRAME_NV12;
 f.planes[0]=base+slot*16384;f.planes[1]=f.planes[0]+64*16;f.strides[0]=f.strides[1]=64;f.pts_us=1000000;return f;}
struct concurrent{pp_playback*pb;uint8_t*memory;_Atomic int done;};
static void *produce(void *arg){struct concurrent*c=arg;evo_vdec_native n={.frame_mem=c->memory};
 for(unsigned i=0;i<1000;++i){int slot=tube_native_claim_frame(&n,i%8);assert(slot>=0);
  memset(c->memory+slot*16384,(int)(i%251),1536);pp_frame f=frame(c->memory,(unsigned)slot);
  assert(pp_playback_push_frame(c->pb,&f)==0);tube_frame_lease_release(f.planes[0]);}
 atomic_store_explicit(&c->done,1,memory_order_release);return NULL;
}
static void *present(void *arg){struct concurrent*c=arg;int observed=0;
 do{pp_video_frame f={0};if(pp_playback_get_video_frame(c->pb,&f)){unsigned before=f.y[0];usleep(10);
  assert(f.y[0]==before);pp_playback_release_video_frame(&f);++observed;}
 }while(!atomic_load_explicit(&c->done,memory_order_acquire));
 assert(observed>0);return NULL;
}
int main(void){
 uint8_t*memory=aligned_alloc(16384,8*16384);assert(memory);memset(memory,64,8*16384);
 assert(!tube_frame_pool_register(NULL,16384,8));assert(!tube_frame_pool_register(memory,0,8));
 assert(!tube_frame_pool_register(memory,16384,9));assert(!tube_frame_pool_register((void*)(UINTPTR_MAX-16),16384,8));
 assert(tube_frame_pool_register(memory,16384,8));assert(!tube_frame_pool_register(memory,16384,8));
 assert(tube_frame_lease_hold(memory)==-1);
 assert(tube_frame_lease_hold(memory+1)==-1);assert(tube_frame_lease_hold(NULL)==0);
 evo_vdec_native native={.frame_mem=memory};
 int gpu_slot=tube_native_claim_frame(&native,0);assert(gpu_slot==0);
 /* All other slots are consumed. The GPU's slot survives at least three pool wraps. */
 for(unsigned i=1;i<=24;++i){int slot=tube_native_claim_frame(&native,i%8);assert(slot>=0&&slot!=gpu_slot);tube_frame_lease_release(memory+slot*16384);}
 assert(!tube_frame_pool_unregister(memory));
 tube_frame_lease_release(memory);assert(tube_frame_pool_claim(memory,0)==0);tube_frame_lease_release(memory);
 /* Exhaustion is bounded and cannot grant reuse or teardown. No wall-clock sleep in this test. */
 for(int i=0;i<8;++i)assert(tube_frame_pool_claim(memory,i)==i);
 fake_ms=0;assert(tube_native_claim_frame(&native,0)<0&&native.fatal&&fake_ms==NWD_DECODE_MS);
 struct dec_slot slot={.frame_mem=memory,.ready=1,.max_w=64,.max_h=16};fake_ms=0;assert(!tube_native_pool_idle(&slot)&&slot.owned&&!slot.ready);
 assert(!tube_frame_pool_unregister(memory));
 release_while_waiting=memory+2*16384;native.fatal=0;assert(tube_native_claim_frame(&native,0)==2&&!native.fatal);
 for(int i=0;i<8;++i)tube_frame_lease_release(memory+i*16384);
 assert(tube_native_pool_idle(&slot));
 /* The real pp path: publication and GPU snapshot own independent references. */
 pp_playback pb;pp_playback_init(&pb);int64_t clock=1000000;pp_playback_set_clock_source(&pb,media_clock,&clock);pp_playback_on_file_open(&pb);
 /* A heap-backed publication has no pool lease and must take the staged GPU path. */
 uint8_t heap_frame[1536]={0};pp_frame soft=frame(heap_frame,0);assert(pp_playback_push_frame(&pb,&soft)==0);
 pp_video_frame heap_snapshot={0};assert(pp_playback_get_video_frame(&pb,&heap_snapshot));
 assert(heap_snapshot.y==heap_frame&&!heap_snapshot.tube_lease);pp_playback_release_video_frame(&heap_snapshot);
 pp_playback_on_file_open(&pb);
 assert(tube_native_claim_frame(&native,0)==0);pp_frame first=frame(memory,0);assert(pp_playback_push_frame(&pb,&first)==0);tube_frame_lease_release(memory);
 pp_video_frame gpu={0};assert(pp_playback_get_video_frame(&pb,&gpu)&&gpu.tube_lease==memory);
 assert(tube_native_claim_frame(&native,1)==1);pp_frame second=frame(memory,1);assert(pp_playback_push_frame(&pb,&second)==0);tube_frame_lease_release(memory+16384);
 assert(tube_frame_pool_busy(memory)==2);
 for(unsigned i=0;i<24;++i){int chosen=tube_native_claim_frame(&native,i%8);assert(chosen!=0&&chosen!=1);tube_frame_lease_release(memory+chosen*16384);}
 pp_playback_on_file_close(&pb);assert(tube_frame_pool_busy(memory)==1&&!tube_frame_pool_unregister(memory));
 /* Epoch retirement is bookkeeping, not permission to recycle an outstanding GPU snapshot. */
 pp_playback_note_retired(&pb,gpu.tube_epoch,gpu.tube_serial);assert(tube_frame_pool_busy(memory)==1);
 pp_playback_release_video_frame(&gpu);pp_playback_release_video_frame(&gpu);assert(tube_frame_pool_busy(memory)==0);
 pp_playback_on_file_open(&pb);
 /* Actual generated native harvest/receive: transfer the reorder reference only after pp publishes. */
 struct desc desc={.is_vp9=1};native=(evo_vdec_native){.frame_mem=memory,.frame_size=16384,.slot=&slot,.desc=&desc,.disp_w=64,.disp_h=16,.nv12_out=1};
 for(int i=0;i<=EVO_VDEC_REORDER_DEPTH;++i){int selected=tube_native_claim_frame(&native,(unsigned)i);assert(selected==i);
  SceVideodec2OutputInfo output={0};output.valid=output.picture_count=1;output.width=output.pitch=output.pitch_bytes=64;output.height=16;output.buffer=memory+i*16384;
  ro_harvest(&native,&output);tube_frame_lease_release(output.buffer);}
 pp_frame received={0};assert(evo_vdec_native_receive(&native,&received)==1);assert(pp_playback_push_frame(&pb,&received)==0);
 assert(pp_playback_get_video_frame(&pb,&gpu)&&gpu.tube_lease==memory);
 assert(evo_vdec_native_receive(&native,&received)==0); /* previous receive now belongs to publication/GPU only */
 ro_reset(&native);assert(tube_frame_pool_busy(memory)==1);
 pp_playback_on_file_close(&pb);assert(tube_frame_pool_busy(memory)==1);
 pp_playback_release_video_frame(&gpu);assert(!tube_frame_pool_busy(memory));
 pp_playback_on_file_open(&pb);
 struct concurrent concurrent={&pb,memory,0};pthread_t decoder_thread,presenter_thread;
 assert(!pthread_create(&presenter_thread,NULL,present,&concurrent));assert(!pthread_create(&decoder_thread,NULL,produce,&concurrent));
 pthread_join(decoder_thread,NULL);pthread_join(presenter_thread,NULL);
 pp_playback_shutdown(&pb);assert(!tube_frame_pool_busy(memory));assert(tube_frame_pool_unregister(memory));free(memory);
 puts("PASS actual native claims and pp handoff: pool wrap, bounded wait, independent publication/GPU references, close/reset and no timeout reuse");
}
