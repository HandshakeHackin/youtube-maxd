"""Test the exact generated host callback, launch worker and completion guard."""
from pathlib import Path
import sys

source = Path(sys.argv[1]).read_text()
dest = Path(sys.argv[2]); dest.mkdir(parents=True, exist_ok=True)
def block(signature):
    if source.count(signature) != 1:
        raise RuntimeError('Handoff boundary mismatch: ' + signature)
    start = source.index(signature)
    stop = source.index('{', start) + 1
    depth = 1
    while depth:
        if source[stop] == '{': depth += 1
        elif source[stop] == '}': depth -= 1
        stop += 1
    return source[start:stop]
dest.joinpath('recovery_pending.inc').write_text(block('struct PendingPlay {') + ';\n')
functions = [block(s) for s in (
    'static void* start_playback_worker(void* arg)',
    'int launch_choice(int idx)',
    'void on_resolved(int ok, const evo_stream_choice_t* choices, int count, void* ud)',
    'void on_tube_resolved(int ok, const evo_stream_choice_t* choices, int count, void* ud)',
)]
dest.joinpath('recovery_handoff.inc').write_text('\n'.join(functions) + '\n')
start = '\n        if (g_tube_recovery_launch) {'
if source.count(start) != 1:
    raise RuntimeError('Worker completion boundary mismatch')
completion = source.split(start, 1)[1].split('\n        if (g_start_success) {', 1)[0]
dest.joinpath('recovery_complete.inc').write_text(start + completion)
