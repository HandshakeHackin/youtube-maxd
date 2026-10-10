"""Extract shipping seek request/UI methods for observable handoff regressions."""
from pathlib import Path
import sys


def block(source, signature):
    if source.count(signature) != 1:
        raise RuntimeError('Seek boundary mismatch: ' + signature)
    start = source.index(signature)
    stop = source.index('{', start) + 1
    depth = 1
    while depth:
        if source[stop] == '{': depth += 1
        elif source[stop] == '}': depth -= 1
        stop += 1
    return source[start:stop] + '\n'


controller = Path(sys.argv[1]).read_text()
demux = Path(sys.argv[2]).read_text()
dest = Path(sys.argv[3]); dest.mkdir(parents=True, exist_ok=True)
dest.joinpath('seek_ui.inc').write_text(block(controller, 'void PlaybackController::seekTo(double targetSeconds)'))
dest.joinpath('seek_request.inc').write_text(block(demux, 'int prospero_request_inplace_seek('))
