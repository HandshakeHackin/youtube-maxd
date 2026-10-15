"""Check shipping seek ownership, read interruption and EOF completion gating."""
import os
from pathlib import Path
import re
import subprocess
import tempfile

work = Path(os.environ.get("PROSPEROTUBE_NATIVE_WORK", Path.home() / ".cache/prosperotube/native"))
media = work / "projects/evoplayer/media/src"
source = (media / "evo_demux.c").read_text()
stream = (media / "evo_stream_io.c").read_text()

def declaration(text, name):
    # Pending is volatile storage shared with the stream callback via GCC atomics;
    # active ownership remains C11 atomic. Keep the shipping declarations intact.
    matches = re.findall(r"^static [^;{}\n]*\b" + re.escape(name) + r"\s*(?:=[^;]*)?;", text, re.M)
    assert len(matches) == 1, (name, matches)
    return matches[0] + "\n"

def block(text, signature):
    assert text.count(signature) == 1, signature
    start = text.index(signature)
    end = text.index("{", start) + 1
    depth = 1
    while depth:
        depth += (text[end] == "{") - (text[end] == "}")
        end += 1
    return text[start:end] + "\n"

declarations = "".join(declaration(source, name) for name in
                       ("prospero_seek_mutex", "prospero_seek_pending", "prospero_seek_in_progress"))
getter = block(source, "int evo_demux_is_seeking(void)")
completion = block((media / "evo_playback.c").read_text(), "int evo_pb_is_complete(void)")
interrupts = declaration(stream, "sio_start_cancelled")
for signature in ("void evo_stream_io_reset_start_cancel(void)",
                  "void evo_stream_io_cancel_pending_start(void)",
                  "int evo_stream_io_start_cancelled(void)",
                  "static int sio_interrupt_cb(void *opaque)",
                  "void evo_stream_io_interrupt_on_seek(",
                  "int evo_stream_io_take_seek_interrupt("):
    interrupts += block(stream, signature)

scaffold = r'''
#include <assert.h>
#include <stdint.h>
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
typedef struct {
    double deadline_at;
    int deadline_hit, aborted;
    const volatile int *seek_pending;
    volatile int seek_interrupted;
} evo_stream_io_ctx_t;
typedef struct { struct { int (*callback)(void *); void *opaque; } interrupt_callback; } AVFormatContext;
static double sio_now_seconds(void) { return 100.0; }
static int player_paused, pb_scrub_hold, g_pb_decode_fatal, video_stream_index=-1, audio_stream_index=0;
static _Atomic int tube_video_failed, tube_video_decoded;
static double video_fps=24;
static int g_pp_pb;
static int evo_pb_input_complete(void) { return 1; }
static int evo_pb_audio_drained(void) { return 1; }
static int pp_playback_display_retired(int *pb, int64_t dwell) {
    assert(pb == &g_pp_pb && dwell > 0); return 1;
}
'''
hooks = r'''
static _Atomic int handoff_ready, reader_entered;
static int track_reader;
static int seek_lock_hook(pthread_mutex_t *mutex) {
    assert(mutex == &prospero_seek_mutex);
    if (track_reader) atomic_store(&reader_entered, 1);
    return pthread_mutex_lock(mutex);
}
#define pthread_mutex_lock seek_lock_hook
'''
checks = r'''
#undef pthread_mutex_lock
static void pending(int value) {
    __atomic_store_n(&prospero_seek_pending, value, __ATOMIC_RELEASE);
}
static void *handoff(void *unused) {
    (void)unused;
    assert(!pthread_mutex_lock(&prospero_seek_mutex));
    pending(0);
    prospero_seek_in_progress = 0; // Deliberate uncommitted intermediate state.
    atomic_store(&handoff_ready, 1);
    while (!atomic_load(&reader_entered)) sched_yield();
    prospero_seek_in_progress = 1;
    assert(!pthread_mutex_unlock(&prospero_seek_mutex));
    return NULL;
}
int main(void) {
    assert(!evo_demux_is_seeking() && evo_pb_is_complete());
    pending(1); assert(evo_demux_is_seeking() && !evo_pb_is_complete());
    pending(0); prospero_seek_in_progress = 1;
    assert(evo_demux_is_seeking() && !evo_pb_is_complete());
    prospero_seek_in_progress = 0;
    assert(!evo_demux_is_seeking() && evo_pb_is_complete());

    pending(1);
    pthread_t thread; assert(!pthread_create(&thread, NULL, handoff, NULL));
    while (!atomic_load(&handoff_ready)) sched_yield();
    track_reader = 1;
    assert(evo_demux_is_seeking()); // Blocks until committed in-progress.
    assert(!pthread_join(thread, NULL));
    track_reader = 0;
    assert(!evo_pb_is_complete());
    prospero_seek_in_progress = 0;
    assert(!evo_demux_is_seeking() && evo_pb_is_complete());

    evo_stream_io_ctx_t io = {0};
    AVFormatContext fmt = {{sio_interrupt_cb, &io}}, foreign = {{0, &io}};
    evo_stream_io_interrupt_on_seek(&fmt, &prospero_seek_pending);
    assert(io.seek_pending == &prospero_seek_pending && !sio_interrupt_cb(&io));
    pending(1);
    assert(sio_interrupt_cb(&io)); // Queued seek wakes the blocked read.
    assert(evo_stream_io_take_seek_interrupt(&fmt) == 1);
    assert(evo_stream_io_take_seek_interrupt(&fmt) == 0); // Consumed once.
    pending(0); prospero_seek_in_progress = 1;
    assert(!sio_interrupt_cb(&io)); // Its own active seek must be able to read.
    assert(evo_demux_is_seeking() && !evo_pb_is_complete());

    pending(1); assert(sio_interrupt_cb(&io));
    evo_stream_io_cancel_pending_start();
    assert(sio_interrupt_cb(&io));
    assert(!evo_stream_io_take_seek_interrupt(&fmt)); // Stop cannot masquerade as a seek retry.
    evo_stream_io_reset_start_cancel();
    io.aborted = 1;
    assert(sio_interrupt_cb(&io) && !evo_stream_io_take_seek_interrupt(&fmt));
    io.aborted = 0; pending(0);
    evo_stream_io_interrupt_on_seek(&fmt, NULL); // Clears a stale wake on reassignment.
    assert(!io.seek_pending && !evo_stream_io_take_seek_interrupt(&fmt));
    assert(!sio_interrupt_cb(&io));
    io.deadline_at = 99.0;
    assert(sio_interrupt_cb(&io) && io.deadline_hit);
    assert(!evo_stream_io_take_seek_interrupt(&fmt)); // Deadline is not seek cancellation.
    evo_stream_io_interrupt_on_seek(&foreign, &prospero_seek_pending);
    assert(!io.seek_pending && !evo_stream_io_take_seek_interrupt(&foreign));
    assert(!evo_stream_io_take_seek_interrupt(NULL));
    prospero_seek_in_progress = 0;
    assert(evo_pb_is_complete());
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix="tube-seek-ownership-") as directory:
    test = Path(directory) / "test.c"
    binary = Path(directory) / "test"
    test.write_text(scaffold + declarations + hooks + getter + completion + interrupts + checks)
    subprocess.run([os.environ.get("CC", "cc"), "-std=c11", "-Wall", "-Wextra", "-Werror",
                    "-fsanitize=address,undefined", "-pthread", str(test), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True, timeout=10)
print("PASS: shipping seek ownership is atomic across handoff, gates completion, and distinguishes seek wake from stop/timeout")
