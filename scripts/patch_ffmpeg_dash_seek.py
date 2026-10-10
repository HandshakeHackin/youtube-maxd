"""Enable FFmpeg 7.1.1 byte seeking for complete single-file DASH VOD only."""
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
    for old,new in ((before,after),(old_seek,new_seek)):
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