"""Associate delayed VP9 outputs with their own coded-frame display policy."""


def apply(replace, player):
    native = player + 'media/src/evo_vdec_native.c'
    replace(native, '    int     pts_n;', '''    int     pts_n;
    struct { int64_t pts; int present; } tube_vp9_pending[PTS_POOL];
    unsigned tube_vp9_head, tube_vp9_count;
    int tube_vp9_bsf_pending;''')
    replace(native, 'static int decode_one(evo_vdec_native *n,', r'''/* VP9 produces one output per coded AU, including hidden alternate references. */
static void tube_native_output(evo_vdec_native *n, const SceVideodec2OutputInfo *out)
{
    if (n->desc->is_vp9) {
        int ten_bit = n->desc->idx == NAT_VP92;
        uint32_t pitch = out->pitch_bytes ? out->pitch_bytes : out->pitch;
        if (ten_bit && pitch < out->pitch * 2u) pitch = out->pitch * 2u;
        if (!n->tube_vp9_count || n->pts_n ||
            !tube_native_surface_valid(n, out, pitch, ten_bit)) {
            note("EVO vdec native: VP9 output lost coded-frame association");
            n->fatal = 1;
            return;
        }
        unsigned head = n->tube_vp9_head;
        n->tube_vp9_head = (head + 1u) % PTS_POOL;
        n->tube_vp9_count--;
        if (!n->tube_vp9_pending[head].present) return;
        pts_push(n, n->tube_vp9_pending[head].pts);
        int before = n->ro_count;
        ro_harvest(n, out);
        if (n->fatal || n->ro_count != before + 1 || n->pts_n) {
            n->fatal = 1;
            return;
        }
        n->frames_out++;
        return;
    }
    n->frames_out++;
    ro_harvest(n, out);
}

static int decode_one(evo_vdec_native *n,''')
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
    replace(native, '            n->tube_drain_finished = 1;\n            return;', '''            if (n->desc->is_vp9 && n->tube_vp9_count) {
                note("EVO vdec native: VP9 drain ended with pending coded frames");
                n->fatal = 1;
            } else n->tube_drain_finished = 1;
            return;''')
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
