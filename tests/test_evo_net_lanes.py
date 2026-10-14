"""Exercise the prepared EVO HTTP worker against a held artwork response."""

import http.server
import os
from pathlib import Path
import subprocess
import tempfile
import threading
import time
import unittest


ROOT = Path(__file__).resolve().parents[1]
WORK = Path(os.environ.get("PROSPEROTUBE_NATIVE_WORK", Path.home() / ".cache/prosperotube/native"))
EVO = WORK / "projects/evoplayer"

HARNESS = r'''
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "evo_net.h"

static int image_done, control_done, control_ok;
void evo_error_url_host(const char *url, char *out, size_t cap)
{ (void)url; if (cap) out[0] = 0; }
int evo_error_take(char *out, size_t cap)
{ (void)out; (void)cap; return 0; }
static void image_cb(int ok, int status, const char *body, size_t len, void *ctx)
{ (void)ok; (void)status; (void)body; (void)len; (void)ctx; image_done++; }
static void control_cb(int ok, int status, const char *body, size_t len, void *ctx)
{ (void)body; (void)len; (void)ctx; control_done = 1; control_ok = ok && status == 200; }
static double now(void)
{ struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec / 1e9; }
static void sleep_ms(long ms)
{ struct timespec t = {ms / 1000, (ms % 1000) * 1000000}; nanosleep(&t, NULL); }
int main(int argc, char **argv)
{
    if (argc != 3 || evo_net_init() != 0) return 2;
    char image[128], control[128];
    if (strcmp(argv[2], "capacity") == 0) {
        for (int i = 0; i < 32; ++i) {
            snprintf(image, sizeof image, "http://127.0.0.1:%s/fast.png?i=%d", argv[1], i);
            if (evo_net_request_async("GET", image, NULL, NULL, 0, image_cb, NULL) != 0) return 6;
        }
        snprintf(image, sizeof image, "http://127.0.0.1:%s/fast.png?i=33", argv[1]);
        if (evo_net_request_async("GET", image, NULL, NULL, 0, image_cb, NULL) != EVO_NET_ASYNC_ERR_FULL) return 7;
        double start = now();
        while (image_done < 32 && now() - start < 3.0) { evo_net_poll(); sleep_ms(10); }
        if (image_done != 32) return 8;
        if (evo_net_request_async("GET", image, NULL, NULL, 0, image_cb, NULL) != 0) return 9;
        start = now();
        while (image_done < 33 && now() - start < 1.0) { evo_net_poll(); sleep_ms(10); }
        printf("accepted=32 rejected=1 callbacks=%d reaccepted=%d\\n", image_done, image_done == 33);
        evo_net_shutdown();
        return image_done == 33 ? 0 : 10;
    }
    snprintf(image, sizeof image, "http://127.0.0.1:%s/%s", argv[1], argv[2]);
    snprintf(control, sizeof control, "http://127.0.0.1:%s/control", argv[1]);
    if (evo_net_request_async("GET", image, NULL, NULL, 0, image_cb, NULL) != 0) return 3;
    sleep_ms(100);
    double start = now();
    if (evo_net_request_async("GET", control, NULL, NULL, 0, control_cb, NULL) != 0) return 4;
    while (!control_done && now() - start < 1.0) { evo_net_poll(); sleep_ms(10); }
    int pass = control_done && control_ok && !image_done;
    printf("control_done=%d control_ok=%d image_done=%d elapsed=%.3f\\n",
           control_done, control_ok, image_done, now() - start);
    evo_net_shutdown();
    return pass ? 0 : 5;
}
'''


class Handler(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"
    image_entered = threading.Event()
    release_image = threading.Event()

    def do_GET(self):
        if self.path == "/held.png" or self.path.startswith("/Items/42/Images/Primary?"):
            self.image_entered.set()
            self.release_image.wait(3)
        body = b"ok"
        self.send_response(200)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, *args):
        pass


class NetLanesTest(unittest.TestCase):
    def check_held_image(self, image_path):
        self.assertTrue((EVO / "addons/src/evo_net.c").exists(), "run prepare_native first")
        Handler.image_entered.clear()
        Handler.release_image.clear()
        server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        try:
            with tempfile.TemporaryDirectory() as tmp:
                source = Path(tmp) / "lanes.c"
                binary = Path(tmp) / "lanes"
                source.write_text(HARNESS)
                subprocess.run([
                    "cc", "-std=c11", "-DNO_OPENSSL", "-pthread", "-Wall", "-Wextra", "-Werror",
                    "-I", str(EVO / "addons/include"), "-I", str(EVO / "include"),
                    str(source), str(EVO / "addons/src/evo_net.c"), "-o", str(binary),
                ], check=True, capture_output=True, text=True)
                result = subprocess.run([str(binary), str(server.server_port), image_path],
                                        capture_output=True, text=True, timeout=6)
                if image_path != 'capacity':
                    self.assertTrue(Handler.image_entered.is_set(), "image did not occupy its worker")
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        finally:
            Handler.release_image.set()
            server.shutdown()
            server.server_close()

    def test_extension_image_cannot_block_control(self):
        self.check_held_image('held.png')

    def test_suffixless_emby_artwork_cannot_block_control(self):
        self.check_held_image('Items/42/Images/Primary?maxHeight=256&format=jpg')

    def test_each_accepted_request_has_completion_capacity(self):
        self.check_held_image('capacity')


if __name__ == "__main__":
    unittest.main()
