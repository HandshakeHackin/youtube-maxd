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
dest.joinpath('seek_ui.inc').write_text(
    block(controller, 'void PlaybackController::seekTo(double targetSeconds)') +
    block(controller, 'bool PlaybackController::seekTo(double targetSeconds, bool keepPaused)'))
dest.joinpath('seek_request.inc').write_text(block(demux, 'int prospero_request_inplace_seek('))
dest.joinpath('seek_process.inc').write_text(block(demux, 'static int prospero_process_seek_request(void)'))
dest.joinpath('resume_seek.inc').write_text('int64_t tube_resume_target_us = 0;\nbool tube_resume_armed = false;\n' + block(controller, '    if (wantResume) {') + block(controller, '    if (tube_resume_armed) {'))
audio = Path(sys.argv[1]).parents[3] / 'media/src/evo_audio_out.c'
dest.joinpath('resume_anchor.inc').write_text(block(audio.read_text(), 'static void anchor_resume_base(void)'))
dest.joinpath('audio_accept.inc').write_text(block(audio.read_text(), 'static int tube_audio_accept_frame(AVFrame *af)'))
video = audio.with_name('evo_playback.c').read_text()
dest.joinpath('resume_video_anchor.inc').write_text(block(video, 'static void tube_anchor_video_resume(int64_t pts_us)'))
dest.joinpath('resume_video_clock.inc').write_text(block(video, '            if (pf.pts_us != INT64_MIN && !seek_discarding) {'))
