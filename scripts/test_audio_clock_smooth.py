#!/usr/bin/env python3
"""Exercise the generated audio-master interpolation and reset gates."""
from pathlib import Path
import os
import subprocess
import tempfile


work = Path(os.environ.get("PROSPEROTUBE_NATIVE_WORK", str(Path.home() / ".cache/prosperotube/native")))
player = work / "projects/evoplayer"
audio = (player / "media/src/evo_audio_out.c").read_text()
playback = (player / "media/src/evo_playback.c").read_text()
demux = (player / "media/src/evo_demux.c").read_text()
controller = (player / "core/src/services/PlaybackController.cpp").read_text()

start = audio.index("static pthread_mutex_t tube_audio_clock_mutex")
fn = audio.index("double evo_audio_clock_smooth_seconds(void)", start)
brace = audio.index("{", fn)
depth, end = 1, brace + 1
while depth:
    depth += (audio[end] == "{") - (audio[end] == "}")
    end += 1
source = audio[start:end]

assert "evo_audio_clock_smooth_seconds() - evo_pb_video_hold_s()" in playback
assert playback.count("audio_rel = evo_audio_clock_smooth_seconds() - evo_pb_video_hold_s();") == 2
assert "evo_audio_clock_smooth_reset();" in demux and "evo_audio_clock_smooth_reset();" in controller
assert "tube_audio_clock_hold();\n                const int tube_delay_pending" in audio
assert "tube_audio_clock_hold();\n            /* Reset throttle tracker" in audio
assert "tube_audio_clock_advance(audio_queue_samples[audio_queue_read]" in audio

program = r'''#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <math.h>
#include <pthread.h>
#include <stdint.h>
#include <time.h>
#define AUDIO_BLOCK_SAMPLES 2048
static int tube_drain, seeking, drained;
static uint64_t epoch = 1;
static uint64_t tube_audio_epoch(void *p) { (void)p; return epoch; }
static int evo_demux_is_seeking(void) { return seeking; }
static int evo_audio_is_drained(void) { return drained; }
volatile long long audio_samples_played;
volatile double audio_clock_seconds;
volatile int audio_thread_running = 1;
static int player_paused, pb_scrub_hold;
''' + source + r'''
int main(void) {
    const double block = 2048.0 / 48000.0;
    assert(fabs(tube_audio_clock_interpolate(10, 1000000, 1, block, 1020000) - 10.020) < 1e-9);
    assert(fabs(tube_audio_clock_interpolate(10, 1000000, 1.5, 1.5 * block, 1020000) - 10.030) < 1e-9);
    assert(fabs(tube_audio_clock_interpolate(10, 1000000, 1, block, 1100000) - (10 + block)) < 1e-9);
    assert(fabs(tube_audio_clock_interpolate(10, 1000000, 1, block / 2, 1100000) - (10 + block / 2)) < 1e-9);
    assert(tube_audio_clock_interpolate(10, 1000000, 1, block, 999999) == 10);
    tube_audio_clock_advance(2048, 1.5, epoch);
    assert(fabs(audio_clock_seconds - 1.5 * block) < 1e-9);
    player_paused = 1;
    assert(evo_audio_clock_smooth_seconds() == audio_clock_seconds);
    player_paused = 0;
    pb_scrub_hold = 1;
    assert(evo_audio_clock_smooth_seconds() == audio_clock_seconds);
    pb_scrub_hold = 0;
    tube_audio_clock_hold();
    assert(evo_audio_clock_smooth_seconds() == audio_clock_seconds);
    seeking = 1;
    tube_audio_clock_advance(2048, 1, epoch);
    assert(fabs(audio_clock_seconds - 1.5 * block) < 1e-9);
    seeking = 0;
    ++epoch;
    tube_audio_clock_advance(2048, 1, epoch - 1);
    assert(fabs(audio_clock_seconds - 1.5 * block) < 1e-9);
    evo_audio_clock_smooth_reset();
    assert(audio_clock_seconds == 0 && audio_samples_played == 0);
    tube_audio_clock_advance(2048, 1, epoch);
    assert(fabs(audio_clock_seconds - block) < 1e-9);
    drained = 1;
    assert(evo_audio_clock_smooth_seconds() == audio_clock_seconds);
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix="audio-clock-smooth-") as directory:
    path = Path(directory) / "test.c"
    exe = Path(directory) / "test"
    path.write_text(program)
    subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", str(path), "-pthread", "-lm", "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
print("smooth audio-master clock: PASS")
