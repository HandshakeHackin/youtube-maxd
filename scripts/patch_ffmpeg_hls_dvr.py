# Add opt-in calibrated HLS DVR to isolated pinned FFmpeg 7.1.1.
from pathlib import Path
import argparse
import hashlib


def patch(source):
    digest = hashlib.sha256(Path(__file__).read_bytes() + Path(__file__).with_name('ffmpeg_hls_dvr.inc').read_bytes()).hexdigest()
    stamp = '/* ProsperoTube HLS DVR overlay: ' + digest + ' */'
    if '/* ProsperoTube HLS DVR overlay:' in source:
        if stamp in source: return source
        raise RuntimeError('DVR overlay changed; restore pristine pinned hls.c before applying')
    changes = [
        ('#include "libavutil/time.h"', '#include "libavutil/time.h"\n#include "libavutil/parseutils.h"\n#include "libavutil/thread.h"'),
        ('struct segment {\n', 'struct segment {\n    int64_t dvr_utc;\n'),
        ('    int finished;\n', '    int dvr_bad;\n    int finished;\n'),
        ('    int live_start_index;\n', '    int live_start_index;\n    int tube_dvr, dvr_anchored, dvr_disabled, dvr_mutex_ready;\n    int64_t dvr_offset, dvr_first_utc, dvr_last_pts[16], dvr_snapshot[7];\n    AVMutex dvr_mutex;\n'),
        ('    int64_t duration = 0;\n    enum KeyType', '    int64_t duration = 0;\n    int64_t dvr_utc = AV_NOPTS_VALUE;\n    enum KeyType'),
        ('        pls->finished = 0;\n', '        pls->finished = 0;\n        pls->dvr_bad = 0;\n'),
        ('        } else if (av_strstart(line, "#EXTINF:", &ptr)) {', '''        } else if (av_strstart(line, "#EXT-X-PROGRAM-DATE-TIME:", &ptr)) {
            int64_t parsed;
            if (strlen(ptr) < 20 || strlen(ptr) > 32 || ptr[strlen(ptr) - 1] != 'Z' ||
                !strchr(ptr, 'T') || av_parse_time(&parsed, ptr, 0) < 0 ||
                parsed < 946684800000000LL || parsed > 4102444800000000LL ||
                (dvr_utc != AV_NOPTS_VALUE && parsed != dvr_utc)) {
                if (pls) pls->dvr_bad = 1;
                c->dvr_disabled = c->dvr_anchored;
                dvr_utc = AV_NOPTS_VALUE;
            } else dvr_utc = parsed;
        } else if (av_strstart(line, "#EXT-X-DISCONTINUITY", &ptr) ||
                   av_strstart(line, "#EXT-X-GAP", &ptr)) {
            if (pls) pls->dvr_bad = 1;
            c->dvr_disabled = 1;
        } else if (av_strstart(line, "#EXTINF:", &ptr)) {'''),
        ('                seg = av_malloc(sizeof(struct segment));', '                seg = av_mallocz(sizeof(struct segment));'),
        ('                if (has_iv) {\n                    memcpy(seg->iv, iv, sizeof(iv));', '''                seg->dvr_utc = dvr_utc;
                if (dvr_utc != AV_NOPTS_VALUE && duration > 0 && dvr_utc <= INT64_MAX - duration)
                    dvr_utc += duration;
                else { pls->dvr_bad = 1; dvr_utc = AV_NOPTS_VALUE; }
                if (has_iv) {
                    memcpy(seg->iv, iv, sizeof(iv));'''),
        ('    if (prev_segments) {\n', '''    if (prev_segments && c->tube_dvr && c->dvr_anchored) {
        int overlap = 0;
        if (pls->start_seq_no < prev_start_seq_no || pls->start_seq_no < 0 ||
            pls->n_segments > 4096 || pls->start_seq_no > INT64_MAX - pls->n_segments)
            c->dvr_disabled = 1;
        for (int i = 0; !c->dvr_disabled && i < pls->n_segments; ++i) {
            int64_t old = pls->start_seq_no + i - prev_start_seq_no;
            if (old >= 0 && old < prev_n_segments) {
                overlap = 1;
                if (pls->segments[i]->dvr_utc != prev_segments[old]->dvr_utc ||
                    pls->segments[i]->duration != prev_segments[old]->duration) c->dvr_disabled = 1;
            }
        }
        if (!overlap) c->dvr_disabled = 1;
    }
    if (prev_segments) {'''),
        ('    return ret;\n}\n\nstatic struct segment *current_segment', '''    if (ret < 0 && c->dvr_anchored) c->dvr_disabled = 1;
    tube_dvr_publish(c);
    return ret;
}

static struct segment *current_segment'''),
        ('    free_playlist_list(c);\n', '''    if (c->dvr_mutex_ready) {
        ff_mutex_destroy(&c->dvr_mutex);
        c->dvr_mutex_ready = 0;
    }
    free_playlist_list(c);
'''),
        ('    c->ctx                = s;', '''    if (ff_mutex_init(&c->dvr_mutex, NULL)) return AVERROR(ENOMEM);
    c->dvr_mutex_ready = 1;
    c->ctx                = s;'''),
        ('                seg = current_segment(pls);\n', '                tube_dvr_observe(c, pls);\n                seg = current_segment(pls);\n'),
        ('    if ((flags & AVSEEK_FLAG_BYTE) || (c->ctx->ctx_flags & AVFMTCTX_UNSEEKABLE))', '''    /* Refresh/revalidate on the demux owner before any flush or clock change. */
    if (c->tube_dvr && c->dvr_anchored && c->n_playlists == 1 && !c->playlists[0]->finished) {
        if (parse_playlist(c, c->playlists[0]->url, c->playlists[0], NULL) < 0) {
            c->dvr_disabled = 1;
            tube_dvr_publish(c);
            return AVERROR(EIO);
        }
        if (!c->dvr_snapshot[0]) return AVERROR(ENOSYS);
    }
    if ((flags & AVSEEK_FLAG_BYTE) || (c->ctx->ctx_flags & AVFMTCTX_UNSEEKABLE))'''),
        ('    /* set segment now so we do not need to search again below */', '    memset(c->dvr_last_pts, 0, sizeof(c->dvr_last_pts));\n    /* set segment now so we do not need to search again below */'),
        ('    {"live_start_index",', '''    {"prosperotube_dvr", "enable checked program-date-time DVR for a continuous single playlist",
        OFFSET(tube_dvr), AV_OPT_TYPE_BOOL, {.i64 = 0}, 0, 1, FLAGS},
    {"live_start_index",'''),
    ]
    # The helpers need get_timebase, which is defined just before parse_playlist.
    marker = 'static int parse_playlist(HLSContext *c, const char *url,'
    changes.append((marker, '#include "prosperotube_hls_dvr.inc"\n\n' + marker))
    for before, after in changes:
        if after in source: continue
        if source.count(before) != 1: raise RuntimeError('Pinned HLS overlay mismatch: ' + before[:100])
        source = source.replace(before, after, 1)
    return stamp + '\n' + source


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    args = parser.parse_args()
    source = args.source.read_text()
    result = patch(source)
    args.source.with_name('prosperotube_hls_dvr.inc').write_text(Path(__file__).with_name('ffmpeg_hls_dvr.inc').read_text())
    if result != source: args.source.write_text(result)


if __name__ == '__main__': main()
