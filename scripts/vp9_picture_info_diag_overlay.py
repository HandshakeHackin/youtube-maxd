"""Opt-in raw VP9 completion metadata; no change to decoding or presentation."""
RECORD_C = r"""
#define TUBE_PICTURE_DIAG_CAPACITY 2048u
#define TUBE_PICTURE_DIAG_PATH "/data/youtube-maxd/vp9_picture_info_diag.csv"
struct tube_picture_diag_record {
    uint64_t sequence, decode_calls, output_y, buffer_size, sample_us;
    uint64_t raw_08, raw_10, raw_18, raw_28, raw_30;
    int64_t associated_pts;
    unsigned epoch, associated_decode_call, slot, output_valid, picture_count;
    unsigned bounds_valid, lease_held, trailer_type, trailer_valid, raw_38, raw_39, raw_3c, raw_40;
    int associated_present, associated_show_existing, trigger_present, trigger_show_existing;
};
"""
FIELDS_C = r"""
    struct tube_picture_diag_record tube_picture_records[TUBE_PICTURE_DIAG_CAPACITY];
    unsigned tube_picture_count, tube_picture_omitted, tube_picture_epoch;
    int tube_picture_dumped;
"""
HELPERS_C = r"""
/* Reject bad sizes before any trailer read or cache operation. The public parser
 * additionally requires >1024 bytes for its single VP9 picture. */
static int tube_picture_diag_bounds(const evo_vdec_native *n,
                                    const SceVideodec2OutputInfo *out,
                                    uintptr_t *flush_begin, size_t *flush_size)
{
    if (!out || !n->desc->is_vp9 || out->codec != SCE_VIDEODEC2_CODEC_VP9 ||
        out->picture_count != 1 || (out->size != 0x30u && out->size != 0x38u) ||
        !n->frame_mem || !n->frame_size || n->frame_size > SIZE_MAX / FRAME_POOL_SLOTS ||
        out->buffer_size <= 1024u || out->buffer_size > n->frame_size)
        return 0;
    const uintptr_t base = (uintptr_t)n->frame_mem, address = (uintptr_t)out->buffer;
    if (address < base || address - base >= n->frame_size * FRAME_POOL_SLOTS ||
        (address - base) % n->frame_size || (address & 255u) ||
        n->frame_size > UINTPTR_MAX - address)
        return 0;
    const uintptr_t end = address + (uintptr_t)out->buffer_size;
    const uintptr_t begin = (end - 0xf8u) & ~(uintptr_t)63u;
    if (end > UINTPTR_MAX - 63u) return 0;
    const uintptr_t rounded_end = (end + 63u) & ~(uintptr_t)63u;
    if (begin < address || rounded_end > address + n->frame_size) return 0;
    *flush_begin = begin;
    *flush_size = (size_t)(rounded_end - begin);
    return 1;
}

static uint64_t tube_picture_diag_read64(const uint8_t *p)
{
    uint64_t value; memcpy(&value, p, sizeof(value)); return value;
}
static uint32_t tube_picture_diag_read32(const uint8_t *p)
{
    uint32_t value; memcpy(&value, p, sizeof(value)); return value;
}

/* Called after surface validation, before pending metadata or hidden filtering.
 * The decoder owns its completion until this function returns; explicitly hold
 * the registered surface while invalidating and reading its metadata trailer. */
static void tube_picture_diag_output(evo_vdec_native *n, const SceVideodec2OutputInfo *out, unsigned head)
{
    const unsigned count = __atomic_load_n(&n->tube_picture_count, __ATOMIC_RELAXED);
    if (count >= TUBE_PICTURE_DIAG_CAPACITY) {
        __atomic_fetch_add(&n->tube_picture_omitted, 1u, __ATOMIC_RELAXED);
        return;
    }
    struct tube_picture_diag_record *r = &n->tube_picture_records[count];
    memset(r, 0, sizeof(*r));
    r->sequence = count + 1u; r->epoch = n->tube_picture_epoch;
    r->decode_calls = n->dec_calls;
    r->associated_decode_call = n->tube_vp9_pending[head].diagnostic_decode_call;
    r->associated_pts = n->tube_vp9_pending[head].pts;
    r->associated_present = n->tube_vp9_pending[head].present;
    r->associated_show_existing = n->tube_vp9_pending[head].diagnostic_show_existing;
    r->trigger_present = n->tube_luma_trigger_present;
    r->trigger_show_existing = n->tube_luma_trigger_show_existing;
    r->output_y = (uint64_t)(uintptr_t)out->buffer; r->buffer_size = out->buffer_size;
    r->output_valid = out->valid; r->picture_count = out->picture_count;
    r->slot = (unsigned)(((uintptr_t)out->buffer - (uintptr_t)n->frame_mem) / n->frame_size);
    uintptr_t flush_begin; size_t flush_size;
    r->bounds_valid = tube_picture_diag_bounds(n, out, &flush_begin, &flush_size);
    if (r->bounds_valid && tube_frame_lease_hold(out->buffer) == 1) {
        r->lease_held = 1;
        const uint8_t *metadata = (const uint8_t *)out->buffer + out->buffer_size - 0xf8u;
        const uint64_t started = tube_luma_diag_now_us();
        evo_agc_runtime_cache_flush((const void *)flush_begin, flush_size);
        /* Firmware12.70 GetPictureInfo VP9 trailer layout. Raw evidence only:
         * do not assume PTS, attached identity, or presentation semantics. */
        r->trailer_type = tube_picture_diag_read32(metadata);
        r->trailer_valid = tube_picture_diag_read32(metadata + 0x20);
        r->raw_08 = tube_picture_diag_read64(metadata + 0x08);
        r->raw_10 = tube_picture_diag_read64(metadata + 0x10);
        r->raw_18 = tube_picture_diag_read64(metadata + 0x18);
        r->raw_28 = tube_picture_diag_read64(metadata + 0x28);
        r->raw_30 = tube_picture_diag_read64(metadata + 0x30);
        r->raw_38 = metadata[0x38]; r->raw_39 = metadata[0x39];
        r->raw_3c = tube_picture_diag_read32(metadata + 0x3c);
        r->raw_40 = metadata[0x40];
        r->sample_us = tube_luma_diag_now_us() - started;
        tube_frame_lease_release(out->buffer);
    }
    __atomic_store_n(&n->tube_picture_count, count + 1u, __ATOMIC_RELEASE);
}

static void tube_picture_diag_dump(evo_vdec_native *n)
{
    const unsigned count = __atomic_load_n(&n->tube_picture_count, __ATOMIC_ACQUIRE);
    if (n->tube_picture_dumped || !count) return;
    const unsigned omitted = __atomic_load_n(&n->tube_picture_omitted, __ATOMIC_RELAXED);
    n->tube_picture_dumped = 1;
    FILE *file = fopen(TUBE_PICTURE_DIAG_PATH, "w");
    if (!file) { note("picture_diag: cannot write %s", TUBE_PICTURE_DIAG_PATH); return; }
    int failed = fprintf(file, "sequence,epoch,decode_calls,associated_decode_call,associated_pts_us,associated_present,associated_show_existing,trigger_present,trigger_show_existing,output_valid,picture_count,output_y,buffer_size,slot,bounds_valid,lease_held,trailer_type,trailer_valid,raw_08,raw_10,raw_18,raw_28,raw_30,raw_38,raw_39,raw_3c,raw_40,sample_us\n") < 0;
    for (unsigned i = 0; i < count && !failed; ++i) {
        const struct tube_picture_diag_record *r = &n->tube_picture_records[i];
        failed = fprintf(file, "%llu,%u,%llu,%u,%lld,%d,%d,%d,%d,%u,%u,%llu,%llu,%u,%u,%u,%u,%u,%llu,%llu,%llu,%llu,%llu,%u,%u,%u,%u,%llu\n",
            (unsigned long long)r->sequence, r->epoch, (unsigned long long)r->decode_calls,
            r->associated_decode_call, (long long)r->associated_pts, r->associated_present,
            r->associated_show_existing, r->trigger_present, r->trigger_show_existing,
            r->output_valid, r->picture_count, (unsigned long long)r->output_y,
            (unsigned long long)r->buffer_size, r->slot, r->bounds_valid, r->lease_held,
            r->trailer_type, r->trailer_valid, (unsigned long long)r->raw_08,
            (unsigned long long)r->raw_10, (unsigned long long)r->raw_18,
            (unsigned long long)r->raw_28, (unsigned long long)r->raw_30,
            r->raw_38, r->raw_39, r->raw_3c, r->raw_40, (unsigned long long)r->sample_us) < 0;
    }
    if (fclose(file) != 0) failed = 1;
    note("picture_diag: rows=%u omitted=%u file=%s result=%s; raw fields have no assumed semantics",
        count, omitted, TUBE_PICTURE_DIAG_PATH, failed ? "failed" : "ok");
}
"""

def apply(replace, player):
    native = player + 'media/src/evo_vdec_native.c'
    replace(native, 'struct evo_vdec_native {', RECORD_C + '\nstruct evo_vdec_native {')
    replace(native, '    int tube_vp9_bsf_pending;', '    int tube_vp9_bsf_pending;' + FIELDS_C)
    replace(native, 'static void pts_push(', HELPERS_C + '\nstatic void pts_push(')
    replace(native, '        unsigned head = (n->tube_vp9_head + match) % PTS_POOL;',
            '        unsigned head = (n->tube_vp9_head + match) % PTS_POOL;\n        tube_picture_diag_output(n, out, head);')
    replace(native, '    tube_packet_diag_seek(v);',
            '    tube_packet_diag_seek(v);\n    ++v->tube_picture_epoch;')
    replace(native, '    ro_reset(v);\n    if (v->slot && !tube_native_pool_idle(v->slot)) return;',
            '    tube_picture_diag_dump(v);\n    ro_reset(v);\n    if (v->slot && !tube_native_pool_idle(v->slot)) return;')
