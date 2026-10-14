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
    for old,new in ((before,after),(old_seek,new_seek),(old_read,new_read),(old_field,new_field),(old_warning,new_warning)):
        if new in source: continue
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
