/* ProsperoTube opt-in HLS DVR. The demux thread owns calibration/mutation;
 * readers copy only the published snapshot while holding dvr_mutex. */
static AVRational get_timebase(struct playlist *p);
int av_hls_get_dvr_window(AVFormatContext *s, int64_t out[7]);

static void tube_dvr_publish(HLSContext *c)
{
    int64_t value[7] = {0};
    struct playlist *p = c->n_playlists == 1 ? c->playlists[0] : NULL;
    if (c->tube_dvr && c->dvr_anchored && !c->dvr_disabled && p &&
        !p->finished && !p->dvr_bad && p->n_segments >= 2 && p->n_segments <= 4096 &&
        p->start_seq_no >= 0 && p->start_seq_no <= INT64_MAX - p->n_segments &&
        p->target_duration > 0 && p->target_duration <= 60 * AV_TIME_BASE) {
        int64_t utc = p->segments[0]->dvr_utc;
        int64_t end = utc;
        for (int i = 0; i < p->n_segments; ++i) {
            const struct segment *segment = p->segments[i];
            if (segment->duration <= 0 || segment->duration > 60 * AV_TIME_BASE ||
                segment->dvr_utc != end || segment->key_type != KEY_NONE ||
                end > INT64_MAX - segment->duration) { c->dvr_disabled = 1; break; }
            end += segment->duration;
        }
        if (!c->dvr_disabled && utc > 0 && end > utc && end - utc <= 12LL * 3600 * AV_TIME_BASE &&
            utc >= c->dvr_first_utc && utc + c->dvr_offset >= 0 &&
            end + c->dvr_offset < (1LL << 33) * AV_TIME_BASE / 90000) {
            value[0] = 1;
            value[1] = p->start_seq_no;
            value[2] = p->start_seq_no + p->n_segments;
            value[3] = utc + c->dvr_offset;
            value[4] = end + c->dvr_offset;
            value[5] = utc;
            value[6] = av_gettime_relative() + FFMAX(3 * p->target_duration, 5 * AV_TIME_BASE);
            c->first_timestamp = value[3];
        }
    }
    if (c->dvr_mutex_ready) {
        ff_mutex_lock(&c->dvr_mutex);
        memcpy(c->dvr_snapshot, value, sizeof(value));
        ff_mutex_unlock(&c->dvr_mutex);
    }
    if (c->tube_dvr && c->dvr_anchored) {
        if (value[0]) c->ctx->ctx_flags &= ~(unsigned)AVFMTCTX_UNSEEKABLE;
        else if (p && !p->finished) c->ctx->ctx_flags |= AVFMTCTX_UNSEEKABLE;
    }
}

int av_hls_get_dvr_window(AVFormatContext *s, int64_t out[7])
{
    HLSContext *c;
    if (!out) return 0;
    memset(out, 0, 7 * sizeof(*out));
    if (!s || !s->iformat || strcmp(s->iformat->name, "hls")) return 0;
    c = s->priv_data;
    if (!c || !c->dvr_mutex_ready) return 0;
    ff_mutex_lock(&c->dvr_mutex);
    memcpy(out, c->dvr_snapshot, sizeof(c->dvr_snapshot));
    ff_mutex_unlock(&c->dvr_mutex);
    if (!out[0] || av_gettime_relative() >= out[6]) { memset(out, 0, sizeof(c->dvr_snapshot)); return 0; }
    return 1;
}

int av_hls_dvr_pts_to_wall(AVFormatContext *s, int64_t pts, int64_t *utc);
int av_hls_dvr_wall_to_pts(AVFormatContext *s, int64_t utc, int64_t *pts);

int av_hls_dvr_pts_to_wall(AVFormatContext *s, int64_t pts, int64_t *utc)
{
    int64_t window[7];
    if (!utc || !av_hls_get_dvr_window(s, window) || pts < window[3] || pts >= window[4]) return 0;
    *utc = window[5] + (pts - window[3]);
    return 1;
}

int av_hls_dvr_wall_to_pts(AVFormatContext *s, int64_t utc, int64_t *pts)
{
    int64_t window[7];
    if (!pts || !av_hls_get_dvr_window(s, window) || utc < window[5] ||
        utc - window[5] >= window[4] - window[3]) return 0;
    *pts = window[3] + (utc - window[5]);
    return 1;
}

static void tube_dvr_anchor(HLSContext *c, struct playlist *p)
{
    int64_t offset, utc, pts;
    if (!c->tube_dvr || c->dvr_anchored || c->dvr_disabled || c->n_playlists != 1 ||
        p->finished || p->dvr_bad || p->pkt->dts == AV_NOPTS_VALUE ||
        p->pkt->pts != p->pkt->dts || !(p->pkt->flags & AV_PKT_FLAG_KEY) ||
        p->n_segments < 2 || p->n_segments > 4096 || p->start_seq_no < 0 ||
        p->start_seq_no > INT64_MAX - p->n_segments) return;
    offset = p->cur_seq_no - p->start_seq_no;
    if (offset < 0 || offset >= p->n_segments) return;
    utc = p->segments[offset]->dvr_utc;
    if (utc <= 0) return;
    pts = av_rescale_q(p->pkt->dts, get_timebase(p), AV_TIME_BASE_Q);
    if (pts < 0 || pts >= (1LL << 33) * AV_TIME_BASE / 90000) return;
    c->dvr_offset = pts - utc;
    c->dvr_first_utc = p->segments[0]->dvr_utc;
    c->dvr_anchored = 1;
    tube_dvr_publish(c);
}

/* Conservatively retire calibration on a transport reset or reordered
 * timestamps. In-place seeks have their own explicit discard timestamp. */
static void tube_dvr_observe(HLSContext *c, struct playlist *p)
{
    int64_t pts;
    int stream = p->pkt->stream_index;
    tube_dvr_anchor(c, p);
    if (!c->dvr_anchored || c->dvr_disabled || p->pkt->dts == AV_NOPTS_VALUE) return;
    if (stream < 0 || stream >= 16) { c->dvr_disabled = 1; tube_dvr_publish(c); return; }
    pts = av_rescale_q(p->pkt->dts, get_timebase(p), AV_TIME_BASE_Q);
    if (p->pkt->pts != p->pkt->dts || pts < 0 || pts >= (1LL << 33) * AV_TIME_BASE / 90000 ||
        (p->seek_timestamp == AV_NOPTS_VALUE && c->dvr_last_pts[stream] > 0 && pts < c->dvr_last_pts[stream] - AV_TIME_BASE / 10)) {
        c->dvr_disabled = 1;
        tube_dvr_publish(c);
    }
    c->dvr_last_pts[stream] = pts;
}
