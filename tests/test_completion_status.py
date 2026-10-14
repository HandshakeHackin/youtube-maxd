"""Compile the generated dev_status writer with distinct gate values."""

from pathlib import Path
import subprocess
import sys
import tempfile


def status_writer(source: str) -> str:
    start = source.index("static void write_status(long long now, int parked)")
    opening = source.index("{", start)
    depth = 0
    for index in range(opening, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[start : index + 1]
    raise AssertionError("unterminated write_status")


def main() -> None:
    remote = Path(sys.argv[1]).read_text()
    writer = status_writer(remote)
    harness = r'''
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define STATUS_PATH "status"
#define STATUS_TMP_PATH "status.tmp"
#define EVO_BUILD_ID "host-test"
int screen = 2;
typedef struct evo_pb_completion_gates {
    int natural_eof, input_failed, audio_drained, video_decoded;
    int display_retired, video_failed, paused, scrub, seeking;
} evo_pb_completion_gates;
void evo_pb_get_completion_gates(evo_pb_completion_gates *g) {
    *g = (evo_pb_completion_gates){1, 2, 3, 4, 5, 6, 7, 8, 9};
}
int evo_pb_active_backend(void) { return 4; }
double evo_player_position_s(void) { return 1.25; }
double evo_pb_duration_s(void) { return 8.0; }
double evo_pb_video_fps(void) { return 30.0; }
int evo_pb_decode_fatal(void) { return 11; }
int evo_pb_is_eof(void) { return 12; }
int evo_pb_is_complete(void) { return 13; }
int evo_pb_is_active(void) { return 14; }
'''
    harness += "\n" + writer + r'''
int main(void) {
    write_status(12345000, 15);
    FILE *f = fopen(STATUS_PATH, "r");
    if (!f) return 1;
    char line[512] = {0};
    if (!fgets(line, sizeof(line), f)) return 2;
    fclose(f);
    const char *expected = "fatal=11 eof=12 complete=13 active=14 parked=15 "
        "nat=1 ifail=2 aud=3 vdec=4 disp=5 vfail=6 pause=7 scrub=8 seek=9\n";
    if (!strstr(line, expected) || strncmp(line, "build=host-test t=12345 ", 24)) return 3;
    if (strchr(line, '\n') != strrchr(line, '\n')) return 4;
    puts("PASS: generated dev_status reports each completion gate once in order");
    return 0;
}
'''
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)
        (path / "status_test.c").write_text(harness)
        subprocess.run(
            ["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", "status_test.c", "-o", "status_test"],
            cwd=path,
            check=True,
        )
        subprocess.run([str(path / "status_test")], cwd=path, check=True)


if __name__ == "__main__":
    main()
