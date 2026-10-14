#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

enum { NWD_DECODE_MS=10000, FRAME_POOL_SLOTS=8 };
typedef struct { size_t size; void *buffer; size_t buffer_size; } SceVideodec2FrameBuffer;
typedef struct { size_t size; unsigned valid, error, picture_count; } SceVideodec2OutputInfo;
struct dec_slot { int owned, ready; };
typedef struct {
    void *dec;
    unsigned dec_calls, au_ring;
    int tube_drain_finished;
    uint8_t *frame_mem;
    size_t frame_size;
    struct dec_slot *slot;
} evo_vdec_native;
static int nwd_hung, calls, releases, claims, fail_call, bad_output, no_slot;
static unsigned first_seen[64];
static int64_t tick, tick_step;
static int64_t nwd_now_ms(void) { tick += tick_step; return tick; }
static int tube_native_claim_frame(evo_vdec_native *n, unsigned first) {
    (void)n; first_seen[claims++]=first; return no_slot ? -1 : (int)first;
}
static void nwd_enter(const char *what, int limit) { (void)what; (void)limit; }
static void nwd_leave(void) {}
static void tube_frame_lease_release(const void *p) { assert(p); releases++; }
static int sceVideodec2Flush(void *dec, SceVideodec2FrameBuffer *fb, SceVideodec2OutputInfo *out) {
    assert(dec && fb->buffer && fb->buffer_size && out->size == sizeof *out);
    calls++;
    if (fail_call && calls == fail_call) return -1;
    out->valid = calls <= 3;
    out->picture_count = 1;
    out->error = bad_output ? 1 : 0;
    return 0;
}
static void note(const char *fmt, ...) { (void)fmt; }
#include "native_close_drain.inc"
static void reset_state(void) {
    nwd_hung = calls = releases = claims = fail_call = bad_output = no_slot = 0;
    tick = tick_step = 0;
}
int main(void) {
    uint8_t frame[FRAME_POOL_SLOTS*16384];
    struct dec_slot slot={0,1};
    evo_vdec_native n={.dec=(void*)1,.dec_calls=4,.frame_mem=frame,.frame_size=16384,.slot=&slot};
    assert(tube_native_drain_for_close(&n)==1);
    assert(calls==4 && claims==4 && releases==4 && n.tube_drain_finished);
    for(int i=0;i<4;i++)assert(first_seen[i]==(unsigned)i);
    reset_state(); n.tube_drain_finished=0; n.au_ring=0; slot=(struct dec_slot){0,1}; fail_call=2;
    assert(tube_native_drain_for_close(&n)==0);
    assert(calls==2 && releases==2 && slot.owned && !slot.ready && !n.tube_drain_finished);
    reset_state(); slot=(struct dec_slot){0,1}; bad_output=1;
    assert(tube_native_drain_for_close(&n)==0);
    assert(calls==1 && releases==1 && slot.owned && !slot.ready);
    reset_state(); slot=(struct dec_slot){0,1}; no_slot=1;
    assert(tube_native_drain_for_close(&n)==0);
    assert(calls==0 && releases==0 && slot.owned && !slot.ready);
    reset_state(); slot=(struct dec_slot){0,1}; tick_step=NWD_DECODE_MS;
    assert(tube_native_drain_for_close(&n)==0);
    assert(calls==0 && slot.owned && !slot.ready);
    reset_state(); n.tube_drain_finished=1; slot=(struct dec_slot){0,1};
    assert(tube_native_drain_for_close(&n)==1 && calls==0);
    reset_state(); n.tube_drain_finished=0; n.dec=NULL; slot=(struct dec_slot){0,1};
    assert(tube_native_drain_for_close(&n)==0);
    assert(calls==0 && slot.owned && !slot.ready);
    puts("PASS generated native close drain: complete, error, invalid, no slot and timeout");
}
