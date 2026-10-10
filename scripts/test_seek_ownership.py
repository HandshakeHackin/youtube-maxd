"""Readers must not mistake the pending-to-active handoff for idle seeking."""
import os
from pathlib import Path
import subprocess
import tempfile

work = Path(os.environ.get("PROSPEROTUBE_NATIVE_WORK", Path.home() / ".cache/prosperotube/native"))
source = (work / "projects/evoplayer/media/src/evo_demux.c").read_text()
start = source.index("static pthread_mutex_t prospero_seek_mutex =")
declarations = source[start:source.index(";", start) + 1]
for name in ("prospero_seek_pending", "prospero_seek_in_progress"):
    start = source.index("static _Atomic int " + name)
    declarations += "\n" + source[start:source.index(";", start) + 1]
start = source.index("int evo_demux_is_seeking(void)")
getter = source[start:source.index("\n}", start) + 2]
scaffold = r'''
#include <assert.h>
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
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
static void *handoff(void *unused) {
    (void)unused;
    assert(!pthread_mutex_lock(&prospero_seek_mutex));
    prospero_seek_pending = 0;
    prospero_seek_in_progress = 0; // Deliberate uncommitted intermediate state.
    atomic_store(&handoff_ready, 1);
    while (!atomic_load(&reader_entered)) sched_yield();
    prospero_seek_in_progress = 1;
    assert(!pthread_mutex_unlock(&prospero_seek_mutex));
    return NULL;
}
int main(void) {
    assert(!evo_demux_is_seeking());
    prospero_seek_pending = 1; assert(evo_demux_is_seeking());
    prospero_seek_pending = 0; prospero_seek_in_progress = 1;
    assert(evo_demux_is_seeking());
    prospero_seek_in_progress = 0; assert(!evo_demux_is_seeking());
    prospero_seek_pending = 1;
    pthread_t thread; assert(!pthread_create(&thread, NULL, handoff, NULL));
    while (!atomic_load(&handoff_ready)) sched_yield();
    track_reader = 1;
    assert(evo_demux_is_seeking()); // Locks and observes committed in-progress.
    assert(!pthread_join(thread, NULL));
    track_reader = 0;
    prospero_seek_in_progress = 0;
    assert(!evo_demux_is_seeking());
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix="tube-seek-ownership-") as directory:
    test = Path(directory) / "test.c"
    binary = Path(directory) / "test"
    test.write_text(scaffold + declarations + hooks + getter + checks)
    subprocess.run([os.environ.get("CC", "cc"), "-std=c11", "-Wall", "-Wextra", "-Werror",
                    "-fsanitize=address,undefined", "-pthread", str(test), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True, timeout=10)
print("PASS: shipping seek getter observes queued/active ownership and cannot expose an uncommitted handoff as idle")
