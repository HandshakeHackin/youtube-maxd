"""Opt-in visible VP9 output identity; no file I/O before decoder close."""

RECORD_C = r'''
#define TUBE_LUMA_DIAG_CAPACITY 8192u
#define TUBE_LUMA_DIAG_PATH "/data/prosperotube/decoder_luma_diag.csv"
struct tube_luma_diag_record {
    uint64_t sequence, decode_calls, output_y, source_y, hash, at_us, sample_us;
    int64_t pts;
    unsigned slot, width, height, pitch, ten_bit;
    unsigned output_valid, picture_count, associated_decode_call;
    int associated_show_existing, trigger_present, trigger_show_existing;
    int valid;
};
'''

HELPERS_C = r'''
/* The uncompressed header packs show_existing after the two profile bits
 * (and profile 3's reserved bit). Diagnostic only: never changes submission. */
static int tube_luma_diag_show_existing(const uint8_t *au, int size)
{
    if (!au || size < 1 || (au[0] >> 6) != 2) return -1;
    const unsigned profile = ((au[0] >> 5) & 1u) | (((au[0] >> 4) & 1u) << 1);
    return (au[0] >> (profile == 3 ? 2 : 3)) & 1u;
}

static uint64_t tube_luma_diag_now_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000u + (uint64_t)ts.tv_nsec / 1000u;
}

/* One writer release-publishes immutable rows; close takes an acquire snapshot. */
static void tube_luma_diag_output(evo_vdec_native *n, const struct nat_slot *s,
                                  const SceVideodec2OutputInfo *out)
{
    if (!n->desc->is_vp9 || !s->borrowed) return;
    const unsigned count = __atomic_load_n(&n->tube_luma_count, __ATOMIC_RELAXED);
    if (count >= TUBE_LUMA_DIAG_CAPACITY) {
        __atomic_fetch_add(&n->tube_luma_overflow, 1u, __ATOMIC_RELAXED);
        return;
    }
    struct tube_luma_diag_record *r = &n->tube_luma_records[count];
    r->sequence = count + 1u;
    r->decode_calls = n->dec_calls;
    r->pts = s->pts;
    r->output_valid = out->valid;
    r->picture_count = out->picture_count;
    r->associated_decode_call = n->tube_luma_associated_decode_call;
    r->associated_show_existing = n->tube_luma_associated_show_existing;
    r->trigger_present = n->tube_luma_trigger_present;
    r->trigger_show_existing = n->tube_luma_trigger_show_existing;
    r->output_y = (uint64_t)(uintptr_t)out->buffer;
    r->source_y = (uint64_t)(uintptr_t)s->data;
    r->slot = (unsigned)(((uintptr_t)out->buffer - (uintptr_t)n->frame_mem) / n->frame_size);
    r->width = s->w; r->height = s->h; r->pitch = s->y_stride; r->ten_bit = s->is_10bit;
    r->at_us = tube_luma_diag_now_us();
    r->valid = tube_luma_diag_sample(out->buffer, (int)s->y_stride,
        s->w, s->h, s->is_10bit, evo_agc_runtime_cache_flush, &r->hash);
    r->sample_us = tube_luma_diag_now_us() - r->at_us;
    __atomic_store_n(&n->tube_luma_count, count + 1u, __ATOMIC_RELEASE);
}

static void tube_luma_diag_dump(evo_vdec_native *n)
{
    const unsigned count = __atomic_load_n(&n->tube_luma_count, __ATOMIC_ACQUIRE);
    if (n->tube_luma_dumped || !count) return;
    const unsigned omitted = __atomic_load_n(&n->tube_luma_overflow, __ATOMIC_RELAXED);
    n->tube_luma_dumped = 1;
    FILE *file = fopen(TUBE_LUMA_DIAG_PATH, "w");
    if (!file) { note("luma_diag: cannot write %s", TUBE_LUMA_DIAG_PATH); return; }
    int failed = fprintf(file, "sequence,decode_calls,pts_us,output_y,source_y,slot,width,height,pitch_bytes,ten_bit,hash_valid,luma_hash,at_us,sample_us,output_valid,picture_count,associated_decode_call,associated_show_existing,trigger_present,trigger_show_existing\n") < 0;
    for (unsigned i = 0; i < count && !failed; ++i) {
        const struct tube_luma_diag_record *r = &n->tube_luma_records[i];
        failed = fprintf(file, "%llu,%llu,%lld,%llu,%llu,%u,%u,%u,%u,%u,%d,%llu,%llu,%llu,%u,%u,%u,%d,%d,%d\n",
            (unsigned long long)r->sequence, (unsigned long long)r->decode_calls,
            (long long)r->pts, (unsigned long long)r->output_y, (unsigned long long)r->source_y,
            r->slot, r->width, r->height, r->pitch, r->ten_bit, r->valid,
            (unsigned long long)r->hash, (unsigned long long)r->at_us,
            (unsigned long long)r->sample_us, r->output_valid, r->picture_count,
            r->associated_decode_call, r->associated_show_existing,
            r->trigger_present, r->trigger_show_existing) < 0;
    }
    if (fclose(file) != 0) failed = 1;
    note("luma_diag: rows=%u omitted=%u file=%s result=%s; sparse identity is not full-frame proof",
        count, omitted, TUBE_LUMA_DIAG_PATH, failed ? "failed" : "ok");
}
'''


def apply(replace, player):
    native = player + 'media/src/evo_vdec_native.c'
    replace(native, '#include "tube_frame_lease.h"',
            '#include "tube_frame_lease.h"\n#include "evo_agc_runtime.h"\n#include "tube_luma_diag.h"')
    replace(native, 'struct evo_vdec_native {', RECORD_C + '\nstruct evo_vdec_native {')
    replace(native, '    int tube_vp9_bsf_pending;', '''    int tube_vp9_bsf_pending;
    struct tube_luma_diag_record tube_luma_records[TUBE_LUMA_DIAG_CAPACITY];
    unsigned tube_luma_count, tube_luma_overflow;
    int tube_luma_dumped;
    unsigned tube_luma_associated_decode_call;
    int tube_luma_associated_show_existing, tube_luma_trigger_present, tube_luma_trigger_show_existing;''')
    replace(native, 'static void pts_push(', HELPERS_C + '\nstatic void pts_push(')
    replace(native, '    s->pts      = pts_take(n);',
            '    s->pts      = pts_take(n);\n    tube_luma_diag_output(n, s, out);')
    # Skip hung decoders; the immutable snapshot also tolerates an abandoned live writer.
    replace(native, '    ro_reset(v);\n    if (v->slot && !tube_native_pool_idle(v->slot)) return;',
            '    tube_luma_diag_dump(v);\n    ro_reset(v);\n    if (v->slot && !tube_native_pool_idle(v->slot)) return;')

    add_metadata_hooks(replace, native)


def add_metadata_hooks(replace, native):
    replace(native, 'struct { int64_t pts; int present; uint64_t tag; } tube_vp9_pending[PTS_POOL];',
            'struct { int64_t pts; int present; uint64_t tag; unsigned diagnostic_decode_call; int diagnostic_show_existing; } tube_vp9_pending[PTS_POOL];')
    replace(native, '        n->tube_vp9_pending[tail].present = present;',
            '        n->tube_vp9_pending[tail].present = present;\n'
            '        n->tube_luma_trigger_present = present;\n'
            '        n->tube_luma_trigger_show_existing = tube_luma_diag_show_existing(au, au_size_orig);\n'
            '        n->tube_vp9_pending[tail].diagnostic_decode_call = n->dec_calls;\n'
            '        n->tube_vp9_pending[tail].diagnostic_show_existing = n->tube_luma_trigger_show_existing;')
    replace(native, '        pts_push(n, n->tube_vp9_pending[head].pts);',
            '        n->tube_luma_associated_decode_call = n->tube_vp9_pending[head].diagnostic_decode_call;\n'
            '        n->tube_luma_associated_show_existing = n->tube_vp9_pending[head].diagnostic_show_existing;\n'
            '        pts_push(n, n->tube_vp9_pending[head].pts);')
    replace(native, '        if (n->desc->is_vp9) tube_native_output(n, &out);',
            '        if (n->desc->is_vp9) {\n'
            '            n->tube_luma_trigger_present = n->tube_luma_trigger_show_existing = -1;\n'
            '            tube_native_output(n, &out);\n'
            '        }')
