"""Associate VP9 outputs by their returned identity, including delayed hidden AUs."""


ABI_C = r"""
/* GetPictureInfo VP9 ABI verified in local Videodec2 binaries and on hardware. */
struct tube_vp9_picture_info {
    uint64_t size;
    uint8_t valid, reserved[7];
    uint64_t pts, dts, attached;
    uint8_t codec_fields[0x58 - 0x28];
};
_Static_assert(sizeof(struct tube_vp9_picture_info) == 0x58, "VP9 picture-info size");
extern int32_t sceVideodec2GetPictureInfo(const SceVideodec2OutputInfo *, void *, void *);
/* Never reused by another decoder object or after seek. */
static uint64_t tube_vp9_identity_sequence;
"""

IDENTITY_C = r"""
static uint64_t tube_vp9_input_tag(int present)
{
    const uint64_t ordinal = __atomic_add_fetch(&tube_vp9_identity_sequence, 1u, __ATOMIC_RELAXED);
    if (!ordinal || ordinal > UINT64_MAX / 2u) return 0;
    return (ordinal << 1) | (uint64_t)!!present;
}

/* GetPictureInfo reads the VP9 trailer at buffer end-0xf8 through +0x53.
 * Invalidate its complete cache envelope while the validated surface is held. */
static int tube_vp9_output_tag(evo_vdec_native *n, const SceVideodec2OutputInfo *out, uint64_t *tag)
{
    if (out->codec != SCE_VIDEODEC2_CODEC_VP9 || out->picture_count != 1 ||
        (out->size != 0x30u && out->size != 0x38u) ||
        out->buffer_size <= 1024u || out->buffer_size > n->frame_size)
        return 0;
    const uintptr_t address = (uintptr_t)out->buffer;
    if ((address & 255u) || n->frame_size > UINTPTR_MAX - address) return 0;
    const uintptr_t end = address + (uintptr_t)out->buffer_size;
    if (end > UINTPTR_MAX - 63u) return 0;
    const uintptr_t begin = (end - 0xf8u) & ~(uintptr_t)63u;
    const uintptr_t rounded_end = (end + 63u) & ~(uintptr_t)63u;
    if (begin < address || rounded_end > address + n->frame_size ||
        tube_frame_lease_hold(out->buffer) != 1)
        return 0;
    struct tube_vp9_picture_info picture __attribute__((aligned(16))) = {0};
    picture.size = sizeof(picture);
    evo_agc_runtime_cache_flush((const void *)begin, (size_t)(rounded_end - begin));
    const int32_t rc = sceVideodec2GetPictureInfo(out, &picture, NULL);
    tube_frame_lease_release(out->buffer);
    if (rc || !picture.valid || !picture.attached) return 0;
    *tag = picture.attached;
    return 1;
}
"""

OUTPUT_C = r"""
/* Hidden completions can arrive 15/16 calls later, after newer shown pictures.
 * Exact returned identities, not coded FIFO positions, select display policy. */
static void tube_native_output(evo_vdec_native *n, const SceVideodec2OutputInfo *out)
{
    if (n->desc->is_vp9) {
        int ten_bit = n->desc->idx == NAT_VP92;
        uint32_t pitch = out->pitch_bytes ? out->pitch_bytes : out->pitch;
        if (ten_bit && pitch < out->pitch * 2u) pitch = out->pitch * 2u;
        uint64_t tag = 0;
        if (!n->tube_vp9_count || n->pts_n ||
            !tube_native_surface_valid(n, out, pitch, ten_bit) ||
            !tube_vp9_output_tag(n, out, &tag)) {
            note("EVO vdec native: invalid VP9 completion identity");
            n->fatal = 1;
            return;
        }
        unsigned match = 0;
        for (; match < n->tube_vp9_count; ++match)
            if (n->tube_vp9_pending[(n->tube_vp9_head + match) % PTS_POOL].tag == tag) break;
        if (match == n->tube_vp9_count) {
            note("EVO vdec native: unknown VP9 completion identity");
            n->fatal = 1;
            return;
        }
        unsigned head = (n->tube_vp9_head + match) % PTS_POOL;
        if (n->tube_vp9_pending[head].present) {
            for (unsigned i = 0; i < match; ++i) {
                if (n->tube_vp9_pending[(n->tube_vp9_head + i) % PTS_POOL].present) {
                    note("EVO vdec native: VP9 visible completion out of order");
                    n->fatal = 1;
                    return;
                }
            }
            pts_push(n, n->tube_vp9_pending[head].pts);
            int before = n->ro_count;
            ro_harvest(n, out);
            if (n->fatal || n->ro_count != before + 1 || n->pts_n) {
                n->fatal = 1;
                return;
            }
            n->frames_out++;
        }
        /* Remove only this completion; older hidden AUs remain matchable. */
        for (unsigned i = match; i + 1u < n->tube_vp9_count; ++i)
            n->tube_vp9_pending[(n->tube_vp9_head + i) % PTS_POOL] =
                n->tube_vp9_pending[(n->tube_vp9_head + i + 1u) % PTS_POOL];
        n->tube_vp9_count--;
        return;
    }
    n->frames_out++;
    ro_harvest(n, out);
}
"""

TAG_HOOK_C = r"""    in.attached = n->desc->is_vp9 ? tube_vp9_input_tag(present) : 0;
    if (n->desc->is_vp9 && !in.attached) {
        n->fatal = 1;
        tube_frame_lease_release(n->frame_mem + (size_t)fslot * n->frame_size);
        return -1;
    }"""


DRAIN_EOF_C = r"""            if (n->desc->is_vp9) {
                for (unsigned i = 0; i < n->tube_vp9_count; ++i) {
                    if (n->tube_vp9_pending[(n->tube_vp9_head + i) % PTS_POOL].present) {
                        note("EVO vdec native: VP9 drain ended with pending visible frames");
                        n->fatal = 1;
                        return;
                    }
                }
                n->tube_vp9_head = n->tube_vp9_count = 0;
            }
            n->tube_drain_finished = 1;
            return;"""

def apply(replace, player):
    native = player + 'media/src/evo_vdec_native.c'
    replace(native, """ * FRAME ORDER. SceVideodec2OutputInfo carries no PTS and no picture-detail
 * hook is bound. This backend assumes DISPLAY-order emission + min-PTS pairing,
 * with a small PTS-sorted reorder window as a safety net. If B-frame content
 * misbehaves, build with -DEVO_VDEC_NATIVE_DECODE_ORDER=1 and a deeper
 * -DEVO_VDEC_REORDER_DEPTH.
""", """ * FRAME ORDER. VP9 matches GetPictureInfo's returned attached identity to its
 * original AU, including delayed hidden completions, and preserves shown order.
 * AVC/HEVC retain min-PTS pairing and the bounded PTS-sorted reorder window.
""")
    replace(native, '#include "tube_frame_lease.h"', '#include "tube_frame_lease.h"\n#include "evo_agc_runtime.h"')
    replace(native, 'struct evo_vdec_native {', ABI_C + '\nstruct evo_vdec_native {')
    replace(native, '    in.attached = 0;', TAG_HOOK_C)
    replace(native, '    int     pts_n;', '''    int     pts_n;
    struct { int64_t pts; int present; uint64_t tag; } tube_vp9_pending[PTS_POOL];
    unsigned tube_vp9_head, tube_vp9_count;
    int tube_vp9_bsf_pending;''')
    replace(native, 'static int decode_one(evo_vdec_native *n,', IDENTITY_C + OUTPUT_C + '\nstatic int decode_one(evo_vdec_native *n,')
    replace(native, '    if (size <= 0 || (size_t)size > INPUT_SLOT_BYTES)\n        return -1;', '''    if (size <= 0 || (size_t)size > INPUT_SLOT_BYTES)
        return -1;
    if (n->desc->is_vp9 && n->tube_vp9_count >= PTS_POOL) {
        note("EVO vdec native: VP9 pending coded-frame queue full");
        n->fatal = 1;
        return -1;
    }''')
    replace(native, '        if (n->since_flush_out == 0 &&', '''        if (n->desc->is_vp9 && n->tube_vp9_count) {
            /* Failed submission makes delayed-output association uncertain. */
            n->fatal = 1;
            tube_frame_lease_release(fb.buffer);
            return -1;
        }
        if (n->since_flush_out == 0 &&''')
    replace(native, '''    if (present) {
        if (n->dovi)
            dovi_note_au(n, au, au_size_orig, pts);
        pts_push(n, pts);
    }
    if (out.valid && out.picture_count) {
        n->since_flush_out++;
        if (present) {
            n->frames_out++;
            ro_harvest(n, &out);
        }
        /* hidden VP9 frame produced an output: consume it silently */
    }''', '''    if (n->desc->is_vp9) {
        unsigned tail = (n->tube_vp9_head + n->tube_vp9_count) % PTS_POOL;
        n->tube_vp9_pending[tail].pts = pts;
        n->tube_vp9_pending[tail].present = present;
        n->tube_vp9_pending[tail].tag = in.attached;
        n->tube_vp9_count++;
    } else if (present) {
        if (n->dovi)
            dovi_note_au(n, au, au_size_orig, pts);
        pts_push(n, pts);
    }
    if (out.valid && (out.picture_count || n->desc->is_vp9)) {
        n->since_flush_out++;
        if (n->desc->is_vp9 && !fb.accepted) n->fatal = 1;
        else if (n->desc->is_vp9 || present) tube_native_output(n, &out);
    }''')
    replace(native, '''    tube_frame_lease_release(fb.buffer);
    return 0;
}

static void drain_decoder''', '''    tube_frame_lease_release(fb.buffer);
    return n->fatal ? -1 : 0;
}

static void drain_decoder''')
    replace(native, '        if (!(out.valid && out.picture_count)) {',
            '        if (!out.valid || (!n->desc->is_vp9 && !out.picture_count)) {')
    replace(native, '            n->tube_drain_finished = 1;\n            return;', DRAIN_EOF_C)
    replace(native, '        ro_harvest(n, &out);\n        tube_frame_lease_release(fb.buffer);', '''        if (n->desc->is_vp9) tube_native_output(n, &out);
        else ro_harvest(n, &out);
        tube_frame_lease_release(fb.buffer);
        if (n->fatal) return;''')
    replace(native, '    v->pts_n          = 0;', '''    v->pts_n          = 0;
    v->tube_vp9_head = v->tube_vp9_count = 0;''')

    replace(native, 'int evo_vdec_native_send(evo_vdec_native *v, const uint8_t *data, int size,', r'''/* A superframe can return several older visible pictures; stop before reorder fills. */
static void tube_vp9_pump_bsf(evo_vdec_native *v)
{
    while (v->tube_vp9_bsf_pending && v->ro_count < RO_SLOTS && !v->fatal) {
        int received = av_bsf_receive_packet(v->bsf, v->filt_pkt);
        if (received < 0) {
            v->tube_vp9_bsf_pending = 0;
            if (received != (v->flushing ? AVERROR_EOF : AVERROR(EAGAIN)))
                v->fatal = 1;
            return;
        }
        int64_t pts = v->filt_pkt->pts == AV_NOPTS_VALUE ? INT64_MIN : v->filt_pkt->pts;
        int decoded = decode_one(v, v->filt_pkt->data, v->filt_pkt->size, pts,
                                 pkt_present(v, v->filt_pkt));
        av_packet_unref(v->filt_pkt);
        if (decoded < 0) v->fatal = 1;
    }
}

static int tube_vp9_send(evo_vdec_native *v, const uint8_t *data, int size, int64_t pts)
{
    if (!v->bsf) { v->fatal = 1; return -1; }
    tube_vp9_pump_bsf(v);
    if (v->fatal) return -1;
    if (v->tube_vp9_bsf_pending) return 1;
    if (!data || size <= 0) {
        if (v->flushing) return 0;
        int sent = av_bsf_send_packet(v->bsf, NULL);
        if (sent == AVERROR(EAGAIN)) return 1;
        if (sent < 0 && sent != AVERROR_EOF) { v->fatal = 1; return -1; }
        v->flushing = 1;
    } else {
        if (v->flushing) { v->fatal = 1; return -1; }
        if (v->ro_count > EVO_VDEC_REORDER_DEPTH) return 1;
        av_packet_unref(v->in_pkt);
        if (av_new_packet(v->in_pkt, size) < 0) { v->fatal = 1; return -1; }
        memcpy(v->in_pkt->data, data, (size_t)size);
        v->in_pkt->pts = pts == INT64_MIN ? AV_NOPTS_VALUE : pts;
        v->in_pkt->dts = AV_NOPTS_VALUE;
        int sent = av_bsf_send_packet(v->bsf, v->in_pkt);
        if (sent == AVERROR(EAGAIN)) return 1;
        if (sent < 0) { v->fatal = 1; return -1; }
    }
    v->tube_vp9_bsf_pending = 1;
    tube_vp9_pump_bsf(v);
    if (v->flushing && !v->tube_vp9_bsf_pending) drain_decoder(v);
    return v->fatal ? -1 : 0;
}

int evo_vdec_native_send(evo_vdec_native *v, const uint8_t *data, int size,''')
    replace(native, '''    if (!v || v->fatal)
        return -1;

    if (!data || size <= 0) {''', '''    if (!v || v->fatal)
        return -1;
    if (v->desc->is_vp9) return tube_vp9_send(v, data, size, pts_us);

    if (!data || size <= 0) {''')
    replace(native, '''    if (v->flushing && !v->tube_drain_finished && !v->ro_count) {''', '''    if (v->desc->is_vp9) {
        tube_vp9_pump_bsf(v);
        if (v->fatal) return -1;
    }
    if (v->flushing && !v->tube_vp9_bsf_pending && !v->tube_drain_finished && !v->ro_count) {''')
    replace(native, '    v->tube_vp9_head = v->tube_vp9_count = 0;',
            '    v->tube_vp9_head = v->tube_vp9_count = 0;\n    v->tube_vp9_bsf_pending = 0;')

    replace(native, '        if (!best) { best = &v->ro[i]; continue; }',
            '        if (!best) { best = &v->ro[i]; if (v->desc->is_vp9) break; continue; }')
    replace(native, '''    best->used = 0;
    v->ro_count--;''', '''    if (v->desc->is_vp9) {
        /* VP9 pictures are FIFO even when split subframes share a timestamp. */
        struct nat_slot retired = *best;
        retired.used = 0;
        size_t following = (size_t)(&v->ro[RO_SLOTS - 1] - best);
        memmove(best, best + 1, following * sizeof(*best));
        v->ro[RO_SLOTS - 1] = retired;
    } else best->used = 0;
    v->ro_count--;''')
