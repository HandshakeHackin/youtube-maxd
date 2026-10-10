"""Exercise the shipping generated flush body with observable startup sinks."""
from pathlib import Path
import os
import subprocess
import tempfile

work = Path(os.environ.get("PROSPEROTUBE_NATIVE_WORK", Path.home() / ".cache/prosperotube/native"))
source = (work / "projects/evoplayer/src/evo_boot_log.c").read_text()
start = source.index("void evo_boot_log_flush(void)\n{")
end = source.index("\nvoid evo_boot_log_kick(void)", start)
body = source[start:end]
scaffold = r'''
#include <assert.h>
#include <stdio.h>
#include <pthread.h>
#include <time.h>
static int ready, opens, writers, servers, drains, headers;
static FILE *g_fp;
static long g_written;
static int g_dropped, g_boot_len, g_writer_up;
static char g_boot[32];
static pthread_mutex_t g_io_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_q_lock = PTHREAD_MUTEX_INITIALIZER;
#define EVO_LOG_PATH "unused"
#define BL_ROTATE_BYTES 100000
static int evo_jailbreak_is_open(void) { return ready; }
static FILE *test_open(const char *path, const char *mode) {
    (void)path; (void)mode; ++opens; return tmpfile();
}
static FILE *bl_rotate(FILE *fp) { return fp; }
static void bl_session_header(FILE *fp, const char *why) {
    (void)fp; (void)why; ++headers;
}
static void *bl_writer(void *arg) { return arg; }
static int test_create(pthread_t *t, const pthread_attr_t *attr,
                       void *(*fn)(void *), void *arg) {
    (void)t; (void)attr; (void)fn; (void)arg; ++writers; return 1;
}
static void evo_log_server_start(void) { ++servers; }
static void bl_drain(int sync) { assert(sync == 1); ++drains; }
#define fopen test_open
#define pthread_create test_create
'''
checks = r'''
int main(void) {
    g_boot_len = 4;
    for (int i = 0; i < 10; ++i) evo_boot_log_flush();
    assert(!opens && !writers && !servers && !drains && !headers);
    assert(g_boot_len == 4 && !g_fp);
    ready = 1;
    evo_boot_log_flush();
    assert(opens == 1 && writers == 1 && servers == 1 && drains == 1);
    assert(headers == 1 && !g_boot_len && g_fp);
    evo_boot_log_flush();
    assert(opens == 1 && writers == 1 && servers == 1 && drains == 2);
    fclose(g_fp);
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix="tube-startup-log-") as directory:
    test = Path(directory) / "test.c"
    binary = Path(directory) / "test"
    test.write_text(scaffold + body + checks)
    subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-pthread", str(test), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
print("Generated startup flush: no disk/thread/server before proof; buffered log drains after proof.")
