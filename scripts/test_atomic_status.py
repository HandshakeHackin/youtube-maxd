"""Exercise the shipping status publisher while readers observe partial writes."""
import os
from pathlib import Path
import subprocess
import tempfile

work = Path(os.environ.get("PROSPEROTUBE_NATIVE_WORK", Path.home() / ".cache/prosperotube/native"))
source = (work / "projects/evoplayer/src/evo_usb_remote.c").read_text()
start = source.index("static const char* tube_dev_path(")
paths = source[start:source.index("#define CMD_PATH", start)]
start = source.index("static void write_status(")
body = source[start:source.index("\n}\n", start) + 3]
scaffold = r'''
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static const char *directory;
static const char *evo_data_dir(void) { return directory; }
'''
hooks = r'''
#define STATUS_PATH tube_dev_path(1)
#define STATUS_TMP_PATH tube_dev_path(4)
#define EVO_BUILD_ID "test-build"
static int screen = 2;
static int evo_pb_active_backend(void) { return 1; }
static double evo_player_position_s(void) { return 12.25; }
static double evo_pb_duration_s(void) { return 100; }
static double evo_pb_video_fps(void) { return 60; }
static int evo_pb_decode_fatal(void) { return 0; }
static int evo_pb_is_eof(void) { return 1; }
static int evo_pb_is_complete(void) { return 1; }
static int evo_pb_is_active(void) { return 1; }
static int fail_open, fail_write, fail_close, fail_rename;
static const char *prior = "prior-complete-state\n";
static void assert_prior(void) {
    char buffer[256] = {0};
    FILE *file = fopen(STATUS_PATH, "r"); assert(file);
    assert(fread(buffer, 1, sizeof buffer - 1, file) == strlen(prior));
    assert(!fclose(file) && !strcmp(buffer, prior));
}
static FILE *open_hook(const char *path, const char *mode) {
    assert(!strcmp(path, STATUS_TMP_PATH));
    assert(strcmp(STATUS_TMP_PATH, STATUS_PATH));
    if (fail_open) return NULL;
    FILE *file = fopen(path, mode); assert(file);
    assert_prior(); // An empty staging file never empties the published file.
    return file;
}
static int write_hook(FILE *file, const char *format, ...) {
    assert(fputs("partial", file) >= 0 && !fflush(file));
    assert_prior(); // Reader sees the complete old snapshot during a partial write.
    if (fail_write) return -1;
    rewind(file);
    va_list args; va_start(args, format);
    int result = vfprintf(file, format, args); va_end(args);
    return result;
}
static int close_hook(FILE *file) {
    int result = fclose(file);
    return fail_close ? EOF : result;
}
static int rename_hook(const char *from, const char *to) {
    assert(!strcmp(from, STATUS_TMP_PATH) && !strcmp(to, STATUS_PATH));
    assert_prior();
    return fail_rename ? -1 : rename(from, to);
}
#define fopen open_hook
#define fprintf write_hook
#define fclose close_hook
#define rename rename_hook
'''
checks = r'''
#undef fopen
#undef fprintf
#undef fclose
#undef rename
static void seed(void) {
    fail_open = fail_write = fail_close = fail_rename = 0;
    FILE *file = fopen(STATUS_PATH, "w"); assert(file);
    assert(fputs(prior, file) >= 0 && !fclose(file));
}
int main(int argc, char **argv) {
    assert(argc == 2); directory = argv[1];
    for (int failure = 0; failure < 4; ++failure) {
        seed();
        switch (failure) {
            case 0: fail_open = 1; break;
            case 1: fail_write = 1; break;
            case 2: fail_close = 1; break;
            case 3: fail_rename = 1; break;
        }
        write_status(1000000, 1);
        assert_prior();
        assert(access(STATUS_TMP_PATH, F_OK) != 0);
    }
    for (int parked = 0; parked < 2; ++parked) {
        seed(); write_status(1000000, parked);
        char buffer[256] = {0};
        FILE *file = fopen(STATUS_PATH, "r"); assert(file);
        assert(fread(buffer, 1, sizeof buffer - 1, file) > 0 && !fclose(file));
        char expected[256];
        snprintf(expected, sizeof expected,
            "build=test-build t=1000 scr=2 be=1 pos=12.25 dur=100.0 fps=60.0 "
            "fatal=0 eof=1 complete=1 active=1 parked=%d\n", parked);
        assert(!strcmp(buffer, expected));
        assert(access(STATUS_TMP_PATH, F_OK) != 0);
    }
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix="tube-atomic-status-") as directory:
    test = Path(directory) / "test.c"
    binary = Path(directory) / "test"
    test.write_text(scaffold + paths + hooks + body + checks)
    subprocess.run([os.environ.get("CC", "cc"), "-std=c11", "-Wall", "-Wextra", "-Werror",
                    "-fsanitize=address,undefined", str(test), "-o", str(binary)], check=True)
    subprocess.run([str(binary), directory], check=True)
print("PASS: shipping status publisher keeps prior snapshots during partial writes and I/O failures; active/parked snapshots publish completely")
