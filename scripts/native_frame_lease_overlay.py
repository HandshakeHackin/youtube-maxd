"""Protect native output pools through pp publication and completed GPU presentation."""
import shutil

def apply(root,work,replace,player):
    for name in ('tube_frame_lease.c','tube_frame_lease.h'):
        shutil.copyfile(root/'native'/name,work/player/('addons/src' if name.endswith('.c') else 'addons/include')/name)
    replace(player+'Makefile','    addons/src/tube_backup.c','    addons/src/tube_backup.c \\\n    addons/src/tube_frame_lease.c')
    native=player+'media/src/evo_vdec_native.c'
    replace(native,'#include "evo_dovi.h"','#include "evo_dovi.h"\n#include "tube_frame_lease.h"')
    replace(native,'#define FRAME_POOL_SLOTS            8','#define FRAME_POOL_SLOTS            8\n_Static_assert(FRAME_POOL_SLOTS==TUBE_FRAME_POOL_SLOTS,"native lease pool size");')
    replace(native,'static void slot_teardown(struct dec_slot *s)',r"""static int tube_native_pool_idle(struct dec_slot *s) {
    const int64_t start=nwd_now_ms();
    while (tube_frame_pool_busy(s->frame_mem)) {
        if (nwd_now_ms()-start>=NWD_DECODE_MS) {
            note("EVO vdec native: unsafe pool teardown blocked by outstanding presenter lease; restart required pool=%p",s->frame_mem);
            s->owned=1;s->ready=0;return 0;
        }
        usleep(1000);
    }
    return 1;
}
static int slot_teardown(struct dec_slot *s)""")
    replace(native,'    uint32_t boot_w = s->boot_w;',r"""    if (!tube_native_pool_idle(s)) return -1;
    if (!tube_frame_pool_unregister(s->frame_mem)) {
        note("EVO vdec native: unsafe concurrent frame lease at teardown; restart required pool=%p",s->frame_mem);
        s->owned=1;s->ready=0;return -1;
    }
    uint32_t boot_w = s->boot_w;""")
    replace(native,'    if (s->decoder) { sceVideodec2DeleteDecoder(s->decoder); s->decoder = NULL; }',r"""    note("EVO vdec native: retiring maps input=%p+%zu frame=%p+%zu gpu=%p+%zu cpu_gpu=%p+%zu compute=%p+%zu queue=%p",
         s->input_mem,s->input_pool,s->frame_mem,s->frame_pool,s->gpu_mem,s->gpu_size,
         s->cpu_gpu_mem,s->cpu_gpu_size,s->compute_mem,s->compute_size,s->compute_queue);
    if (s->decoder) {
        int rc=sceVideodec2DeleteDecoder(s->decoder);
        if(rc!=0){note("EVO vdec native: DeleteDecoder failed rc=0x%08x; retaining mapped pools",(unsigned)rc);s->owned=1;s->ready=0;return -1;}
        s->decoder=NULL;
    }
    /* The compute queue may still reference every mapping below. */
    if (s->compute_queue) {
        int rc=sceVideodec2ReleaseComputeQueue(s->compute_queue);
        if(rc!=0){note("EVO vdec native: ReleaseComputeQueue failed rc=0x%08x; retaining mapped pools",(unsigned)rc);s->owned=1;s->ready=0;return -1;}
        s->compute_queue=NULL;
    }""")
    replace(native,'    if (s->compute_queue) { sceVideodec2ReleaseComputeQueue(s->compute_queue); s->compute_queue = NULL; }\n','')
    replace(native,'    s->boot_h = boot_h;\n}', '    s->boot_h = boot_h;\n    return 0;\n}')
    replace(native,'    STAGE("CreateDecoder");',r"""    note("EVO vdec native: mapped pools input=%p+%zu frame=%p+%zu gpu=%p+%zu cpu_gpu=%p+%zu compute=%p+%zu queue=%p",
         s->input_mem,s->input_pool,s->frame_mem,s->frame_pool,s->gpu_mem,s->gpu_size,
         s->cpu_gpu_mem,s->cpu_gpu_size,s->compute_mem,s->compute_size,s->compute_queue);
    if (!tube_frame_pool_register(s->frame_mem,s->frame_size,FRAME_POOL_SLOTS)) return -1;
    STAGE("CreateDecoder");""")
    replace(native,'    probe_slot(NAT_VP9,  EVO_VDEC_NATIVE_BOOT_W, EVO_VDEC_NATIVE_BOOT_H, 0, sm);',r"""    /* Keep one 4K-capable VP9 slot across 1080p and 2160p sessions. */
    probe_slot(NAT_VP9, EVO_VDEC_NATIVE_VP9_MAX_W, EVO_VDEC_NATIVE_VP9_MAX_H, 0, sm);""")
    replace(native,'if (rc != 0 && (w > 1920 || h > 1088)) {','if (rc != 0 && d->idx != NAT_VP9 && (w > 1920 || h > 1088)) {')
    replace(native,'    unsigned au_ring;','    unsigned au_ring;\n    const void *tube_received;')
    replace(native,'static void ro_reset(evo_vdec_native *n)',r"""static int tube_native_claim_frame(evo_vdec_native *n,unsigned first) {
    const int64_t start=nwd_now_ms();
    for (;;) {
        int slot=tube_frame_pool_claim(n->frame_mem,first);
        if (slot>=0) return slot;
        if (slot==-2 || nwd_now_ms()-start>=NWD_DECODE_MS) {
            note("EVO vdec native: no free caller frame while presenter owns pool; restart may be required pool=%p",n->frame_mem);
            n->fatal=1;return -1;
        }
        usleep(1000); /* Outside pp/pool mutex; the presenter can retire independently. */
    }
}
static void ro_reset(evo_vdec_native *n)""")
    replace(native,'    for (int i = 0; i < RO_SLOTS; i++)\n        n->ro[i].used = 0;',r"""    tube_frame_lease_release(n->tube_received);n->tube_received=NULL;
    for (int i = 0; i < RO_SLOTS; i++) {
        if (n->ro[i].used && n->ro[i].borrowed) tube_frame_lease_release(n->ro[i].data);
        n->ro[i].used = 0;
    }""")
    replace(native,'        s->data     = (uint8_t *)out->buffer;',r"""        if (tube_frame_lease_hold(out->buffer)!=1) { n->fatal=1;return; }
        s->data     = (uint8_t *)out->buffer;""")
    replace(native,'    unsigned fslot = n->au_ring % FRAME_POOL_SLOTS;','    int fslot = tube_native_claim_frame(n,(n->au_ring-1u) % FRAME_POOL_SLOTS);\n    if (fslot<0) return -1;')
    # Acquire after AU cleanup/IRAP guards so discarded encoded input never owns a frame slot.
    source=(work/native).read_text();a=source.index('    int fslot = tube_native_claim_frame');b=source.index('    n->au_ring++;',a)
    claim=source[a:b];replace(native,claim,'')
    replace(native,'    SceVideodec2InputData  in;',claim+'    SceVideodec2InputData  in;')
    # The temporary decode-write claim survives until harvest owns the returned output.
    source=(work/native).read_text();a=source.index('    n->dec_calls++;');b=source.index('\nstatic void drain_decoder',a)
    chunk=source[a:b];chunk=chunk.replace('            return 0;','            tube_frame_lease_release(fb.buffer);\n            return 0;').replace('        return -1;','        tube_frame_lease_release(fb.buffer);\n        return -1;').replace('    return 0;\n}', '    tube_frame_lease_release(fb.buffer);\n    return 0;\n}')
    replace(native,source[a:b],chunk)
    replace(native,'        unsigned slot = n->au_ring % FRAME_POOL_SLOTS;', '        int slot=tube_native_claim_frame(n,n->au_ring % FRAME_POOL_SLOTS);\n        if(slot<0)return;')
    replace(native,'        if (frc != 0) { n->fatal = 1; return; }','        if (frc != 0) { tube_frame_lease_release(fb.buffer);n->fatal = 1; return; }')
    replace(native,'            n->tube_drain_finished = 1;\n            return;', '            tube_frame_lease_release(fb.buffer);\n            n->tube_drain_finished = 1;\n            return;')
    replace(native,'        ro_harvest(n, &out);\n        if (n->ro_count', '        ro_harvest(n, &out);\n        tube_frame_lease_release(fb.buffer);\n        if (n->ro_count')
    replace(native,'    if (v->flushing && !v->tube_drain_finished && !v->ro_count) {', '    tube_frame_lease_release(v->tube_received);v->tube_received=NULL;\n    if (v->flushing && !v->tube_drain_finished && !v->ro_count) {')
    replace(native,'    best->used = 0;', '    if(best->borrowed)v->tube_received=best->data;\n    best->used = 0;')
    replace(native,'void evo_vdec_native_close(evo_vdec_native *v)\n{',r"""/* Buffered decode work may still own AU and decoder maps after the last visible frame. */
static int tube_native_drain_for_close(evo_vdec_native *n) {
    if (!n->dec && n->dec_calls) goto unsafe;
    if (!n->dec_calls || n->tube_drain_finished) return 1;
    const int64_t start=nwd_now_ms();
    for (int i=0;i<64 && !nwd_hung && nwd_now_ms()-start<NWD_DECODE_MS;i++) {
        int slot=tube_native_claim_frame(n,n->au_ring % FRAME_POOL_SLOTS);
        if(slot<0)break;
        n->au_ring++;
        SceVideodec2FrameBuffer fb={0};
        SceVideodec2OutputInfo out={0};
        fb.size=sizeof fb;
        fb.buffer=n->frame_mem+(size_t)slot*n->frame_size;
        fb.buffer_size=n->frame_size;
        out.size=sizeof out;
        nwd_enter("Flush (close)",NWD_DECODE_MS);
        int rc=sceVideodec2Flush(n->dec,&fb,&out);
        nwd_leave();
        tube_frame_lease_release(fb.buffer);
        if(rc!=0 || nwd_hung || (out.valid && out.error))break;
        if(!out.valid){note("EVO vdec native: close drain complete outputs=%d pool=%p",i,n->frame_mem);n->tube_drain_finished=1;return 1;}
    }
unsafe:
    note("EVO vdec native: close drain incomplete; retaining decoder mappings pool=%p",n->frame_mem);
    if(n->slot){n->slot->owned=1;n->slot->ready=0;}
    return 0;
}
void evo_vdec_native_close(evo_vdec_native *v)
{""")
    replace(native,'    if (v->annexb_extradata) {','    ro_reset(v);\n    if (v->slot && !tube_native_pool_idle(v->slot)) return;\n    if (!tube_native_drain_for_close(v)) return;\n    if (v->annexb_extradata) {')
    replace(native,'{ nwd_enter("Reset (close)", NWD_RESET_MS); sceVideodec2Reset(v->dec); nwd_leave(); }',r"""{
                    nwd_enter("Reset (close)",NWD_RESET_MS);
                    int rc=sceVideodec2Reset(v->dec);
                    nwd_leave();
                    if(rc!=0 || nwd_hung){
                        note("EVO vdec native: close Reset failed rc=0x%08x; retaining decoder mappings",(unsigned)rc);
                        slot->owned=1;slot->ready=0;return;
                    }
                }""")

    replace(native,'        slot_teardown(s);\n        rc = slot_bringup(s, d, 1920, 1088, &stage);','        if(slot_teardown(s)!=0)return;\n        rc = slot_bringup(s, d, 1920, 1088, &stage);')
    replace(native,'            slot_teardown(slot);\n            s_depth_override = 0;', '            if(slot_teardown(slot)!=0){s_depth_override=0;return NULL;}\n            s_depth_override = 0;')
    replace(native,'            slot_teardown(slot);\n            stage = "?";', '            if(slot_teardown(slot)!=0)return NULL;\n            stage = "?";')
    replace(native,'        slot_teardown(slot);\n        const char *stage', '        if(slot_teardown(slot)!=0)return NULL;\n        const char *stage')
    replace(native,'            slot_teardown(slot);\n            const char *stage', '            if(slot_teardown(slot)!=0)return;\n            const char *stage')
    # Cheap once per open; raw addresses are diagnostic-local, never public telemetry.
    replace(native,'            n->first_valid_logged = 1;',r"""            n->first_valid_logged = 1;
            const uintptr_t surface=(uintptr_t)out.buffer,base=(uintptr_t)n->frame_mem;
            const size_t index=surface>=base && n->frame_size ? (size_t)((surface-base)/n->frame_size) : SIZE_MAX;
            note("EVO vdec native: first surface ptr=%p pool=%p slot=%zu slot_bytes=%zu coded=%ux%u visible=%ux%u pitch=%u pitch_bytes=%u profile=%u accepted=%u pictures=%u error=%u",
                 out.buffer,n->frame_mem,index,n->frame_size,out.width,out.height,n->disp_w,n->disp_h,
                 out.pitch,out.pitch_bytes,n->slot->cfg.profile,fb.accepted,out.picture_count,out.error);""")

    pp=player+'pp/src/pp_playback.c';header=player+'pp/include/pp_playback.h'
    replace(pp,'#include <unistd.h>','#include <unistd.h>\n#include "tube_frame_lease.h"')
    replace(header,'typedef struct pp_video_frame {','typedef struct pp_video_frame {\n    const void *tube_lease;')
    replace(header,'int pp_playback_get_video_frame(pp_playback *pb, pp_video_frame *f);',
        'int pp_playback_get_video_frame(pp_playback *pb, pp_video_frame *f);\nvoid pp_playback_release_video_frame(pp_video_frame *f);')
    replace(pp,'    pb->gl_src_y  = src->planes[0];',r"""    if (tube_frame_lease_hold(src->planes[0])<0) {
        if (pb->lock) pthread_mutex_unlock(mtx(pb));
        return -1;
    }
    tube_frame_lease_release(pb->gl_src_y);
    pb->gl_src_y  = src->planes[0];""")
    replace(pp,'    /* Don\'t show the previous file\'s last frame, or hold it through its seek. */',r"""    if (pb->lock) pthread_mutex_lock(mtx(pb));
    tube_frame_lease_release(pb->gl_src_y);pb->gl_src_y=NULL;
    if (pb->lock) pthread_mutex_unlock(mtx(pb));
    /* Don't show the previous file's last frame, or hold it through its seek. */""")
    replace(pp,'    pb->gl_src_y = pb->gl_src_uv = pb->gl_src_u = pb->gl_src_v = NULL;',
        '    tube_frame_lease_release(pb->gl_src_y);\n    pb->gl_src_y = pb->gl_src_uv = pb->gl_src_u = pb->gl_src_v = NULL;',count=2)
    source=(work/pp).read_text();a=source.index('int pp_playback_get_video_frame(');b=source.index('\nint pp_playback_get_interp_phase',a)
    chunk=source[a:b];anchor='    if (pb->lock)\n        pthread_mutex_unlock(mtx(pb));'
    assert chunk.count(anchor)==1
    chunk=chunk.replace(anchor,r"""    if (got && !f->held) {
        int lease=tube_frame_lease_hold(f->y);
        if (lease<0) { memset(f,0,sizeof *f);got=0; }
        else if(lease) f->tube_lease=f->y;
    }
"""+anchor)
    chunk += r"""
void pp_playback_release_video_frame(pp_video_frame *f) {
    if (!f) return;
    tube_frame_lease_release(f->tube_lease);f->tube_lease=NULL;
}
"""
    replace(pp,source[a:b],chunk)
    controller=player+'core/src/services/PlaybackController.cpp'
    replace(controller,'    pp_playback_on_file_close(&g_pp_pb);','')
    replace(controller,'    if (g_vdec) {\n        evo_vdec_close(g_vdec);',r"""    // Retire pp publication after the decode thread joins, before its native pool can close.
    pp_playback_on_file_close(&g_pp_pb);
    if (g_vdec) {
        evo_vdec_close(g_vdec);""")
    app=player+'core/src/Application.cpp'
    # A native backend alone does not prove that pp handed us a GPU-mapped frame.
    replace(app,'int is_direct = (evo_pb_active_backend() == EVO_VDEC_BACKEND_NATIVE && !f.held && f.uv != nullptr) ? 1 : 0;',
        'int is_direct = (evo_pb_active_backend() == EVO_VDEC_BACKEND_NATIVE && !f.held && f.uv != nullptr && f.tube_lease) ? 1 : 0;')
    replace(app,'        uint64_t tube_hidden_epoch=0,tube_hidden_serial=0;',
        '        pp_video_frame tube_present_source{};\n        uint64_t tube_hidden_epoch=0,tube_hidden_serial=0;')
    replace(app,'                    tube_hidden_epoch=hiddenFrame.tube_epoch;tube_hidden_serial=hiddenFrame.tube_serial;',r"""                    tube_hidden_epoch=hiddenFrame.tube_epoch;tube_hidden_serial=hiddenFrame.tube_serial;
                    tube_present_source=hiddenFrame;hiddenFrame.tube_lease=nullptr;""")
    replace(app,'                    tube_frame_epoch = f.tube_epoch; tube_frame_serial = f.tube_serial;',r"""                    tube_frame_epoch = f.tube_epoch; tube_frame_serial = f.tube_serial;
                    tube_present_source=f;f.tube_lease=nullptr;""")
    source=(work/app).read_text();a=source.index('        if (isPlayer) {',source.index('// 4. Video Quad blit'));brace=source.index('{',a);end=brace+1;depth=1
    while depth:
        depth+=(source[end]=='{')-(source[end]=='}');end+=1
    # f remains in this exact block; a snapshot that was not used by a draw is immediately released.
    chunk=source[a:end];chunk=chunk[:-1]+'    pp_playback_release_video_frame(&f);\n        }'
    replace(app,source[a:end],chunk)
    replace(app,'        // 5. Present if swap requested',r"""        if (!(swap && uiActive)) pp_playback_release_video_frame(&tube_present_source);
        // 5. Present if swap requested""")
    replace(app,'            // End ProsperoTube hidden discard.',r"""            // End ProsperoTube hidden discard.
            if (tube_present_source.tube_lease) {
                if (evo_agc_runtime_last_present_retired()) pp_playback_release_video_frame(&tube_present_source);
                else {
                    // A timeout never authorizes decoder reuse or pool teardown.
                    evo_boot_log("native source lease retained after failed present; restart required");
                }
            }""")

    replace(native,'''         * that pointer instead of copying it into an owned buffer - the reorder
         * window (<= RO_SLOTS frames) is far shorter than the FRAME_POOL_SLOTS
         * cycle before the decoder reuses the slot, so it stays valid until the
         * present consumes it. Saves ~62 MiB of heap (RO_SLOTS owned frames)''','''         * that pointer instead of copying it into an owned buffer. Explicit
         * references cover reorder, pp publication and GPU presentation until
         * successful retirement; a cyclic slot number or pacing is not proof
         * that the caller frame is free. Saves ~62 MiB of heap (RO_SLOTS owned frames)''')
    replace(pp,'''     * Stash the borrowed decoder planes — no copy anywhere. push_frame is on
     * the decode thread and has just pace-slept to this frame's PTS, so the
     * decoder won't recycle the pool slot before the render loop's
     * pp_playback_get_video_frame() + upload runs (~1 frame; the pool is 12 slots).
     * The one case that breaks is a seek's discard window, which has no pacing
     * — hold_snapshot() covers it.''','''     * Publish borrowed planes without a pixel copy. Native output owns a pool
     * reference here; get_video_frame takes an independent snapshot reference
     * before unlocking, and presentation releases it only after completion.
     * Non-native buffers keep their existing pacing/held-copy behavior.''')
    replace(header,'''     * pool (or FFmpeg's AVFrame) — valid because push_frame pace-sleeps on this
     * frame's PTS before returning, so the decoder can't recycle the slot until
     * the render thread has had its turn. pp_playback_get_video_frame() hands these
     * straight out (under the lock, no pixel copy); the GL uploader reads them.''','''     * pool (or FFmpeg's AVFrame). Native pools carry publication and GPU snapshot
     * references through completed presentation. FFmpeg keeps its existing pacing.
     * get_video_frame hands the planes out under the lock without a pixel copy;
     * every native snapshot must be released after its actual GPU use completes.''')
