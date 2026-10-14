/* ProsperoTube opt-in HLS DVR. The demux thread owns calibration/mutation;
 * readers copy only the published snapshot while holding dvr_mutex. */
static AVRational get_timebase(struct playlist *p);
static int parse_playlist(HLSContext *, const char *, struct playlist *, AVIOContext *);
int av_hls_get_dvr_window(AVFormatContext *s, int64_t out[7]);

static void tube_dvr_publish(HLSContext *c)
{
    int64_t value[7] = {0};
    int64_t (*points)[3] = NULL;
    int count = 0;
    struct playlist *p = c->dvr_anchored && c->dvr_video_playlist < c->n_playlists ?
                         c->playlists[c->dvr_video_playlist] : NULL;
    int64_t lower = 0, upper = INT64_MAX, target_duration = 0;
    if (c->dvr_refreshing) return;
    if (c->tube_dvr && c->dvr_anchored && !c->dvr_disabled && p &&
        (c->n_playlists == 1 || c->n_playlists == 2)) {
        for (int j = 0; j < c->n_playlists; ++j) {
            struct playlist *track = c->playlists[j];
            int64_t first, end;
            if (track->finished || track->dvr_bad || track->n_segments < 2 ||
                track->n_segments > 4096 || track->start_seq_no < 0 ||
                track->start_seq_no > INT64_MAX - track->n_segments ||
                track->target_duration <= 0 || track->target_duration > 60 * AV_TIME_BASE) break;
            first = end = track->segments[0]->dvr_utc;
            for (int i = 0; i < track->n_segments; ++i) {
                const struct segment *segment = track->segments[i];
                if (segment->duration <= 0 || segment->duration > 60 * AV_TIME_BASE ||
                    segment->dvr_utc != end || segment->key_type != KEY_NONE ||
                    end > INT64_MAX - segment->duration) { c->dvr_disabled = 1; break; }
                end += segment->duration;
            }
            if (c->dvr_disabled || first <= 0 || end <= first ||
                end - first > 12LL * 3600 * AV_TIME_BASE) break;
            lower = FFMAX(lower, first);
            upper = FFMIN(upper, end);
            target_duration = FFMAX(target_duration, track->target_duration);
            if (j != c->n_playlists - 1) continue;
            if (lower >= upper || lower < c->dvr_first_utc ||
                lower + c->dvr_offset < 0 ||
                upper + c->dvr_offset >= (1LL << 33) * AV_TIME_BASE / 90000) break;
            value[0] = 1;
            value[1] = p->start_seq_no;
            value[2] = p->start_seq_no + p->n_segments;
            value[3] = lower + c->dvr_offset;
            value[4] = upper + c->dvr_offset;
            value[5] = lower;
            value[6] = av_gettime_relative() + FFMAX(3 * target_duration, 5 * AV_TIME_BASE);
            c->first_timestamp = value[3];
        }
    }
    if (value[0]) {
        points = av_malloc_array(p->n_segments, sizeof(*points));
        if (!points) memset(value, 0, sizeof(value));
        else {
            count = p->n_segments;
            for (int i = 0; i < count; ++i) {
                points[i][0] = p->start_seq_no + i;
                points[i][1] = p->segments[i]->dvr_utc;
                points[i][2] = p->segments[i]->duration;
            }
        }
    }
    if (c->dvr_mutex_ready) {
        ff_mutex_lock(&c->dvr_mutex);
        av_freep(&c->dvr_points);
        c->dvr_points = points; c->dvr_point_count = count;
        memcpy(c->dvr_snapshot, value, sizeof(value));
        ff_mutex_unlock(&c->dvr_mutex);
    } else av_free(points);
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
    struct playlist *video = p;
    if (!c->tube_dvr || c->dvr_anchored || c->dvr_disabled ||
        (c->n_playlists != 1 && c->n_playlists != 2) || !p->pkt->data) return;
    if (!p->dvr_seen) {
        int stream = p->pkt->stream_index;
        int kind;
        int64_t index, utc, pts;
        if (p->finished || p->dvr_bad || p->cur_seq_no != p->dvr_selected_seq ||
            p->pkt->dts == AV_NOPTS_VALUE || p->pkt->pts != p->pkt->dts ||
            p->n_segments < 2 || p->n_segments > 4096 || p->start_seq_no < 0 ||
            p->start_seq_no > INT64_MAX - p->n_segments || stream < 0 ||
            (unsigned)stream >= p->ctx->nb_streams) return;
        kind = p->ctx->streams[stream]->codecpar->codec_type;
        if (kind != AVMEDIA_TYPE_VIDEO && !(c->n_playlists == 2 && kind == AVMEDIA_TYPE_AUDIO)) return;
        if (kind == AVMEDIA_TYPE_VIDEO && !(p->pkt->flags & AV_PKT_FLAG_KEY)) return;
        index = p->cur_seq_no - p->start_seq_no;
        if (index < 0 || index >= p->n_segments) return;
        utc = p->segments[index]->dvr_utc;
        pts = av_rescale_q(p->pkt->dts, get_timebase(p), AV_TIME_BASE_Q);
        if (utc <= 0 || pts < 0 || pts >= (1LL << 33) * AV_TIME_BASE / 90000) return;
        p->dvr_first_utc = utc;
        p->dvr_first_pts = pts;
        p->dvr_kind = kind;
        p->dvr_seen = 1;
    }
    if (c->n_playlists == 2) {
        struct playlist *other = c->playlists[1 - p->index];
        int64_t audio_offset, video_offset;
        if (!other->dvr_seen || other->dvr_kind == p->dvr_kind) return;
        video = p->dvr_kind == AVMEDIA_TYPE_VIDEO ? p : other;
        audio_offset = (p->dvr_kind == AVMEDIA_TYPE_AUDIO ? p : other)->dvr_first_pts -
                       (p->dvr_kind == AVMEDIA_TYPE_AUDIO ? p : other)->dvr_first_utc;
        video_offset = video->dvr_first_pts - video->dvr_first_utc;
        if (llabs(audio_offset - video_offset) > AV_TIME_BASE / 2) {
            c->dvr_disabled = 1;
            tube_dvr_publish(c);
            return;
        }
    }
    c->dvr_video_playlist = video->index;
    c->dvr_offset = video->dvr_first_pts - video->dvr_first_utc;
    c->dvr_first_utc = video->segments[0]->dvr_utc;
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
    if (p->index < 0 || p->index >= 2 || stream < 0 || stream >= 16) {
        c->dvr_disabled = 1; tube_dvr_publish(c); return;
    }
    pts = av_rescale_q(p->pkt->dts, get_timebase(p), AV_TIME_BASE_Q);
    if (p->pkt->pts != p->pkt->dts || pts < 0 || pts >= (1LL << 33) * AV_TIME_BASE / 90000 ||
        (p->seek_timestamp == AV_NOPTS_VALUE && c->dvr_last_pts[p->index][stream] > 0 &&
         pts < c->dvr_last_pts[p->index][stream] - AV_TIME_BASE / 10)) {
        c->dvr_disabled = 1;
        tube_dvr_publish(c);
    }
    c->dvr_last_pts[p->index][stream] = pts;
}

int av_hls_dvr_prepare_seek(AVFormatContext *s, int64_t pts_us);
/* Demux-owner only: perform this before the native decoder/queue flush.
 * hls_read_seek still revalidates, since a window can move again meanwhile. */
int av_hls_dvr_prepare_seek(AVFormatContext *s, int64_t pts_us)
{
    HLSContext *c;
    int64_t window[7];
    if (!s || !s->iformat || strcmp(s->iformat->name, "hls") ||
        pts_us < 0 || pts_us >= (1LL << 33) * AV_TIME_BASE / 90000) return 0;
    c = s->priv_data;
    if (!c || !c->tube_dvr || !c->dvr_anchored || c->dvr_disabled ||
        (c->n_playlists != 1 && c->n_playlists != 2)) return 0;
    c->dvr_refreshing = 1;
    for (int i = 0; i < c->n_playlists; ++i) {
        struct playlist *track = c->playlists[i];
        if (track->finished || parse_playlist(c, track->url, track, NULL) < 0) {
            c->dvr_disabled = 1;
            break;
        }
    }
    c->dvr_refreshing = 0;
    tube_dvr_publish(c);
    return av_hls_get_dvr_window(s, window) && pts_us >= window[3] && pts_us < window[4];
}

int av_hls_dvr_point_identity(AVFormatContext *s, int64_t utc, int64_t out[3]);
int av_hls_dvr_point_identity(AVFormatContext *s, int64_t utc, int64_t out[3])
{
    HLSContext *c;
    int lo = 0, hi, ok = 0;
    if (!out) return 0;
    memset(out, 0, 3 * sizeof(*out));
    if (!s || !s->iformat || strcmp(s->iformat->name, "hls")) return 0;
    c = s->priv_data;
    if (!c || !c->dvr_mutex_ready) return 0;
    ff_mutex_lock(&c->dvr_mutex);
    hi = c->dvr_point_count;
    if (c->dvr_snapshot[0] && av_gettime_relative() < c->dvr_snapshot[6] &&
        utc >= c->dvr_snapshot[5] &&
        utc - c->dvr_snapshot[5] < c->dvr_snapshot[4] - c->dvr_snapshot[3]) {
        while (lo < hi) {
            int mid = lo + (hi - lo) / 2;
            const int64_t *point = c->dvr_points[mid];
            if (utc >= point[1] + point[2]) lo = mid + 1;
            else hi = mid;
        }
        if (lo < c->dvr_point_count && utc >= c->dvr_points[lo][1] &&
            utc - c->dvr_points[lo][1] < c->dvr_points[lo][2]) {
            memcpy(out, c->dvr_points[lo], 3 * sizeof(*out)); ok = 1;
        }
    }
    ff_mutex_unlock(&c->dvr_mutex);
    return ok;
}
