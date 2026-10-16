"""Fix FFmpeg 7.1.1 DASH byte seeking and ranged-fragment EOF handling."""
from pathlib import Path
import argparse


def patch(source):
    before = '''    pls->ctx->pb = &pls->pb.pub;
    pls->ctx->io_open  = nested_io_open;'''
    after = '''    /* ProsperoTube: a complete single-file VOD representation retains its
     * underlying byte index. Fragment sequences and separate initialization
     * sections are virtual concatenations and must remain non-seekable. */
    if (!c->is_live && pls->n_fragments == 1 && !pls->init_section &&
        pls->fragments[0]->url_offset == 0 && pls->fragments[0]->size < 0 &&
        pls->input && (pls->input->seekable & AVIO_SEEKABLE_NORMAL))
        pls->pb.pub.seekable = AVIO_SEEKABLE_NORMAL;

    pls->ctx->pb = &pls->pb.pub;
    pls->ctx->io_open  = nested_io_open;'''
    old_seek = '''    if (v->n_fragments && !v->init_sec_data_len) {
        return avio_seek(v->input, offset, whence);
    }'''
    new_seek = '''    if (v->n_fragments && !v->init_sec_data_len) {
        if (v->pb.pub.seekable & AVIO_SEEKABLE_NORMAL) {
            int64_t result;
            if (whence == AVSEEK_SIZE)
                return avio_size(v->input);
            result = avio_seek(v->input, offset, whence);
            if (result >= 0)
                v->cur_seg_offset = result;
            return result;
        }
        return avio_seek(v->input, offset, whence);
    }'''
    old_read = '''    ret = read_from_url(v, v->cur_seg, buf, buf_size);
    if (ret > 0)'''
    new_read = '''    ret = read_from_url(v, v->cur_seg, buf, buf_size);
    /* A bounded range has ended: let the demuxer retire this fragment.
     * Returning zero makes the WebM parser retry the same exhausted input. */
    if (!ret)
        ret = AVERROR_EOF;
    if (ret > 0)'''
    old_field = """    int is_restart_needed;
};"""
    new_field = """    int is_restart_needed;
    int prosperotube_forbidden_logged;
};"""
    old_warning = """            av_log(v->parent, AV_LOG_WARNING, "Failed to open fragment of playlist\\n");"""
    new_warning = """            /* Log one URL-free diagnostic per representation on HTTP 403.
             * The media URL contains a signed token and must never reach logs. */
            if (ret == AVERROR_HTTP_FORBIDDEN) {
                if (!v->prosperotube_forbidden_logged) {
                    const char *kind = "other";
                    const char *expiry = strstr(v->cur_seg->url, "expire=");
                    int64_t expiry_delta = -1;
                    int i;
                    for (i = 0; i < c->n_videos; i++)
                        if (c->videos[i] == v) kind = "video";
                    for (i = 0; i < c->n_audios; i++)
                        if (c->audios[i] == v) kind = "audio";
                    if (expiry && (expiry == v->cur_seg->url ||
                                   expiry[-1] == '?' || expiry[-1] == '&')) {
                        char *end;
                        int64_t expiry_time = strtoll(expiry + 7, &end, 10);
                        if (end > expiry + 7 && end - (expiry + 7) <= 12 &&
                            (*end == '&' || *end == '\\0'))
                            expiry_delta = expiry_time - time(NULL);
                    }
                    av_log(v->parent, AV_LOG_WARNING,
                           "ProsperoTube DASH HTTP 403 track=%s segment=%"PRId64
                           " range=%"PRId64"-%"PRId64" expiry_delta_s=%"PRId64"\\n",
                           kind, v->cur_seq_no - v->first_seq_no,
                           v->cur_seg->url_offset,
                           v->cur_seg->size < 0 ? -1 :
                           v->cur_seg->url_offset + v->cur_seg->size - 1,
                           expiry_delta);
                    v->prosperotube_forbidden_logged = 1;
                }
            } else {
                av_log(v->parent, AV_LOG_WARNING, "Failed to open fragment of playlist\\n");
            }"""
    diagnostic_field = "    int prosperotube_forbidden_logged;"
    diagnostic_fields = diagnostic_field + "\n    unsigned prosperotube_probe_logs, prosperotube_seek_logs;"
    diagnostic_anchor = "static int reopen_demux_for_component(AVFormatContext *s, struct representation *pls)"
    diagnostic_helper = '''/* Bounded numeric diagnostics: phase 0 = failed probe, 1 = seek result.
 * Never include signed media addresses. */
static void prosperotube_dash_diagnostic(struct representation *pls, int phase,
                                       int ret, int64_t target_ms)
{
    unsigned *count = phase ? &pls->prosperotube_seek_logs : &pls->prosperotube_probe_logs;
    if (*count >= (phase ? 8 : 4))
        return;
    ++*count;
    av_log(pls->parent, phase ? AV_LOG_WARNING : AV_LOG_ERROR,
           "ProsperoTube DASH phase=%d rc=%d stream=%d target_ms=%"PRId64
           " seq=%"PRId64" count=%d offset=%"PRId64" size=%"PRId64
           " read=%"PRId64" init=%u/%u io_error=%d pb_error=%d eof=%d exit=%d\\n",
           phase, ret, pls->stream_index, target_ms, pls->cur_seq_no, pls->n_fragments,
           pls->cur_seg ? pls->cur_seg->url_offset : (int64_t)-1,
           pls->cur_seg ? pls->cur_seg->size : (int64_t)-1,
           pls->cur_seg_offset, pls->init_sec_buf_read_offset, pls->init_sec_data_len,
           pls->input ? pls->input->error : 0, pls->pb.pub.error,
           pls->pb.pub.eof_reached, ret == AVERROR_EXIT);
}

''' + diagnostic_anchor
    probe_old = '''    ret = av_probe_input_buffer(&pls->pb.pub, &in_fmt, "", NULL, 0, 0);
    if (ret < 0) {'''
    probe_new = probe_old + "\n        prosperotube_dash_diagnostic(pls, 0, ret, -1);"
    single_old = "        return av_seek_frame(pls->ctx, -1, seek_pos_msec * 1000, flags);"
    single_new = '''        ret = av_seek_frame(pls->ctx, -1, seek_pos_msec * 1000, flags);
        prosperotube_dash_diagnostic(pls, 1, ret, seek_pos_msec);
        return ret;'''
    multi_old = "    ret = dry_run ? 0 : reopen_demux_for_component(s, pls);"
    multi_new = multi_old + "\n    if (!dry_run) prosperotube_dash_diagnostic(pls, 1, ret, seek_pos_msec);"
    for old,new in ((before,after),(old_seek,new_seek),(old_read,new_read),(old_field,new_field),(old_warning,new_warning),
                    (diagnostic_field,diagnostic_fields),(diagnostic_anchor,diagnostic_helper),
                    (probe_old,probe_new),(single_old,single_new),(multi_old,multi_new)):
        if new in source or (old == old_field and diagnostic_fields in source): continue
        if source.count(old)!=1: raise RuntimeError('FFmpeg DASH source does not match the pinned overlay')
        source=source.replace(old,new,1)
    return source


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source',type=Path)
    args=parser.parse_args()
    original=args.source.read_text()
    updated=patch(original)
    if updated!=original: args.source.write_text(updated)

if __name__=='__main__': main()
