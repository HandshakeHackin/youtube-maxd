"""Opt-in identity of accepted VP9 packets, before superframe splitting."""
RECORD_C = r"""
#define TUBE_PACKET_DIAG_CAPACITY 2048u
#define TUBE_PACKET_DIAG_PATH "/data/prosperotube/vp9_packet_diag.csv"
struct tube_packet_diag_record {
    uint64_t packet, epoch_packet, hash_us;
    int64_t pts;
    unsigned epoch, size, valid;
    uint8_t sha256[32];
};
"""
FIELDS_C = r"""
    struct tube_packet_diag_record tube_packet_records[TUBE_PACKET_DIAG_CAPACITY];
    struct AVSHA *tube_packet_sha;
    unsigned tube_packet_count, tube_packet_omitted, tube_packet_epoch;
    uint64_t tube_packet_epoch_count;
    int tube_packet_dumped;
"""
HELPERS_C = r"""
static uint64_t tube_packet_diag_now_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000u + (uint64_t)ts.tv_nsec / 1000u;
}

/* Before the BSF takes ownership. No publication until it accepts the packet. */
static int tube_packet_diag_prepare(evo_vdec_native *v, const uint8_t *data,
                                    int size, int64_t pts, struct tube_packet_diag_record *r)
{
    if (__atomic_load_n(&v->tube_packet_count, __ATOMIC_RELAXED) >= TUBE_PACKET_DIAG_CAPACITY)
        return 0;
    memset(r, 0, sizeof(*r));
    r->pts = pts;
    r->size = (unsigned)size;
    const uint64_t started = tube_packet_diag_now_us();
    if (!v->tube_packet_sha) v->tube_packet_sha = av_sha_alloc();
    if (v->tube_packet_sha && av_sha_init(v->tube_packet_sha, 256) == 0) {
        av_sha_update(v->tube_packet_sha, data, (size_t)size);
        av_sha_final(v->tube_packet_sha, r->sha256);
        r->valid = 1;
    }
    r->hash_us = tube_packet_diag_now_us() - started;
    return 1;
}

static void tube_packet_diag_accept(evo_vdec_native *v, struct tube_packet_diag_record *r, int prepared)
{
    ++v->tube_packet_epoch_count;
    const unsigned count = __atomic_load_n(&v->tube_packet_count, __ATOMIC_RELAXED);
    if (!prepared || count >= TUBE_PACKET_DIAG_CAPACITY) {
        __atomic_fetch_add(&v->tube_packet_omitted, 1u, __ATOMIC_RELAXED);
        return;
    }
    r->packet = count + 1u;
    r->epoch = v->tube_packet_epoch;
    r->epoch_packet = v->tube_packet_epoch_count;
    v->tube_packet_records[count] = *r;
    __atomic_store_n(&v->tube_packet_count, count + 1u, __ATOMIC_RELEASE);
}

static void tube_packet_diag_seek(evo_vdec_native *v)
{
    ++v->tube_packet_epoch;
    v->tube_packet_epoch_count = 0;
}

static void tube_packet_diag_dump(evo_vdec_native *v)
{
    const unsigned count = __atomic_load_n(&v->tube_packet_count, __ATOMIC_ACQUIRE);
    if (v->tube_packet_dumped || !count) return;
    const unsigned omitted = __atomic_load_n(&v->tube_packet_omitted, __ATOMIC_RELAXED);
    v->tube_packet_dumped = 1;
    FILE *file = fopen(TUBE_PACKET_DIAG_PATH, "w");
    if (!file) { note("packet_diag: cannot write %s", TUBE_PACKET_DIAG_PATH); return; }
    int failed = fprintf(file, "packet,epoch,epoch_packet,pts_us,size,sha256_valid,sha256,hash_us\n") < 0;
    for (unsigned i = 0; i < count && !failed; ++i) {
        const struct tube_packet_diag_record *r = &v->tube_packet_records[i];
        char digest[65];
        for (unsigned j = 0; j < 32; ++j) snprintf(digest + j * 2, 3, "%02x", r->sha256[j]);
        failed = fprintf(file, "%llu,%u,%llu,%lld,%u,%u,%s,%llu\n",
            (unsigned long long)r->packet, r->epoch, (unsigned long long)r->epoch_packet,
            (long long)r->pts, r->size, r->valid, digest, (unsigned long long)r->hash_us) < 0;
    }
    if (fclose(file) != 0) failed = 1;
    note("packet_diag: accepted_rows=%u omitted=%u file=%s result=%s",
        count, omitted, TUBE_PACKET_DIAG_PATH, failed ? "failed" : "ok");
}
"""

def apply(replace, player):
    native = player + 'media/src/evo_vdec_native.c'
    replace(native, '#include "tube_frame_lease.h"',
            '#include "tube_frame_lease.h"\n#include <libavutil/sha.h>\n#include <libavutil/mem.h>')
    replace(native, 'struct evo_vdec_native {', RECORD_C + '\nstruct evo_vdec_native {')
    replace(native, '    int tube_vp9_bsf_pending;', '    int tube_vp9_bsf_pending;' + FIELDS_C)
    replace(native, 'static void pts_push(', HELPERS_C + '\nstatic void pts_push(')
    old = "        int sent = av_bsf_send_packet(v->bsf, v->in_pkt);\n        if (sent == AVERROR(EAGAIN)) return 1;\n        if (sent < 0) { v->fatal = 1; return -1; }\n"
    new = "        struct tube_packet_diag_record packet_identity;\n        int packet_prepared = tube_packet_diag_prepare(v, data, size, pts, &packet_identity);\n" + old + "        tube_packet_diag_accept(v, &packet_identity, packet_prepared);\n"
    replace(native, old, new)
    old = "void evo_vdec_native_flush(evo_vdec_native *v)   /* seek */\n{\n    if (!v)\n        return;\n"
    replace(native, old, old + '    tube_packet_diag_seek(v);\n')
    replace(native, '    ro_reset(v);\n    if (v->slot && !tube_native_pool_idle(v->slot)) return;',
            '    tube_packet_diag_dump(v);\n    ro_reset(v);\n    if (v->slot && !tube_native_pool_idle(v->slot)) return;')
    replace(native, '    free(v->dovi_ring);\n    free(v);',
            '    free(v->dovi_ring);\n    av_free(v->tube_packet_sha);\n    free(v);')
