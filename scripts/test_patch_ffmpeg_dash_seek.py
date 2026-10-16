"""Check the DASH patch against a pinned FFmpeg dashdec.c source tree."""
from pathlib import Path
import argparse
import os
import subprocess
import tempfile
import re
import sys

from patch_ffmpeg_dash_seek import patch



def test_diagnostic(updated):
    helper = updated.split("static void prosperotube_dash_diagnostic", 1)[1].split("static int reopen_demux_for_component", 1)[0]
    helper = "static void prosperotube_dash_diagnostic" + helper
    # Compile the actual patched helper; null/absent IO state must be safe even
    # when the input failed before a fragment was allocated.
    harness = r"""
#include <assert.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#define AV_LOG_WARNING 24
#define AV_LOG_ERROR 16
#define AVERROR_EXIT -1414092869
struct fragment { int64_t url_offset, size; const char *url; };
struct input { int error; };
struct representation {
    unsigned prosperotube_probe_logs, prosperotube_seek_logs;
    void *parent; int stream_index; int64_t cur_seq_no; int n_fragments;
    struct fragment *cur_seg; unsigned init_sec_buf_read_offset, init_sec_data_len;
    struct input *input; int64_t cur_seg_offset;
    struct { struct { int error, eof_reached; } pub; } pb;
};
static int calls, level;
static char output[512];
static void av_log(void *parent, int severity, const char *fmt, ...) {
    (void)parent; va_list args; va_start(args, fmt);
    int n = vsnprintf(output, sizeof output, fmt, args); va_end(args);
    assert(n > 0 && n < (int)sizeof output); calls++; level = severity;
}
""" + helper + r"""
int main(void) {
    struct representation r = {0};
    for (int i=0; i<20; ++i) prosperotube_dash_diagnostic(&r, 0, AVERROR_EXIT, -1);
    assert(calls == 4 && level == AV_LOG_ERROR);
    assert(strstr(output,"offset=-1 size=-1") && strstr(output,"io_error=0 pb_error=0 eof=0 exit=1"));
    struct fragment f = {INT64_MAX, INT64_MAX,"https://secret.example/token"};
    struct input input = {-123}; r.cur_seg=&f; r.input=&input;
    r.cur_seq_no=INT64_MAX; r.n_fragments=2147483647; r.stream_index=2147483647;
    r.init_sec_buf_read_offset=UINT32_MAX; r.init_sec_data_len=UINT32_MAX;
    for (int i=0; i<20; ++i) prosperotube_dash_diagnostic(&r, 1, -123, INT64_MAX);
    assert(calls == 12 && level == AV_LOG_WARNING);
    assert(strstr(output,"io_error=-123 pb_error=0 eof=0 exit=0"));
    assert(!strstr(output,"https") && !strstr(output,"secret") && !strstr(output,"token"));
    assert(output[strlen(output)-1] == '\n');
}
"""
    with tempfile.TemporaryDirectory(prefix="tube-dash-diag-") as directory:
        path=Path(directory); (path/"test.c").write_text(harness)
        subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-fsanitize=address,undefined",
                        str(path/"test.c"), "-o", str(path/"test")], check=True)
        subprocess.run([str(path/"test")], check=True)



def test_real_dash(ffmpeg, tube_js):
    # Actual service manifests and FFmpeg demuxer, served only on loopback.
    sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "standalone/test"))
    import seek_lab as lab
    lab.FILES = {key: value for key, value in lab.FILES.items() if key.endswith((".mp4", ".m4a"))}
    server = lab.Server(lab.fixtures())
    try:
        shapes = lab.manifests(server, tube_js, "mp4", 30)
        for refuse in (None, 4):
            server.reset(refuse)
            result = subprocess.run([ffmpeg, "-hide_banner", "-nostdin", "-v", "warning",
                "-allowed_extensions", "ALL", "-ss", "365", "-i", server.base + "/" + shapes["ranged"][0],
                "-t", "12", "-c", "copy", "-f", "null", "-"], capture_output=True, text=True, timeout=20)
            lines = [line[line.index("ProsperoTube DASH phase="):] for line in result.stderr.splitlines()
                     if "ProsperoTube DASH phase=" in line]
            assert lines, "missing real DASH diagnostics"
            for line in lines:
                assert len(line) < 400 and re.fullmatch(r"ProsperoTube DASH(?: [a-z_]+=-?\d+(?:/\d+)?)+", line), line
            if refuse is None:
                assert result.returncode == 0 and len(lines) == 2
                assert all("phase=1 rc=0" in line and "target_ms=365000" in line for line in lines)
            else:
                assert any("phase=0 rc=-" in line for line in lines), "forced fragment failure did not reach probe"
                for stream in (0, 1):
                    assert sum(f"stream={stream} " in line and "phase=0 " in line for line in lines) <= 4
            print(f"Real DASH {'refused fragment' if refuse else 'seek'} numeric diagnostics: PASS")
    finally:
        server.httpd.shutdown()
        server.httpd.server_close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path, nargs="?")
    parser.add_argument("--ffmpeg", help="optional patched host FFmpeg for loopback integration")
    parser.add_argument("--tube-js", help="built standalone tube_js; required with --ffmpeg")
    args = parser.parse_args()
    if bool(args.ffmpeg) != bool(args.tube_js):
        parser.error("--ffmpeg and --tube-js must be provided together")
    if args.source is None:
        source = os.environ.get("PROSPEROTUBE_DASH_SOURCE")
        if not source:
            parser.error("provide dashdec.c or set PROSPEROTUBE_DASH_SOURCE")
        args.source = Path(source)
    original = args.source.read_text()
    updated = patch(original)
    assert patch(updated) == updated, "DASH patch is not idempotent"
    assert updated.count("ProsperoTube DASH HTTP 403 track=") == 1
    assert updated.count("prosperotube_forbidden_logged") == 3
    warning = updated.split("ProsperoTube DASH HTTP 403 track=", 1)[1].split(
        "v->prosperotube_forbidden_logged = 1;", 1
    )[0]
    assert "v->cur_seg->url," not in warning, "signed URL could reach diagnostic"
    assert "expiry_delta_s=" in warning and "v->cur_seg->url_offset" in warning
    test_diagnostic(updated)
    if args.ffmpeg:
        test_real_dash(args.ffmpeg, args.tube_js)
    print("DASH diagnostics: idempotence, null safety, bounds, privacy and severity PASS")


if __name__ == "__main__":
    main()
