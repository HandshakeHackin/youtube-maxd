#!/usr/bin/env bash
# Host tests of the JavaScript service's native bindings under ASan and UBSan.
# The HTTP tests talk to a server inside the test process on 127.0.0.1; the
# certificate for its TLS port is made here and trusted only by the test.
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
out=$here/build/host
mkdir -p "$out"
CC=${CC:-clang}
PYTHON=${PT_PY:-python3}
SAN=(-fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer)
WARN=(-std=gnu11 -g -O1 -Wall -Wextra -Werror)
export ASAN_OPTIONS=detect_leaks=1:abort_on_error=0 UBSAN_OPTIONS=print_stacktrace=1

# A compressible text and its gzip form, for the decoding test.
"$PYTHON" - "$out/test_gzip.h" <<'PY'
import gzip, sys
text = ("The quick brown fox jumps over the lazy dog. " * 40).encode()
packed = gzip.compress(text, mtime=0)
with open(sys.argv[1], "w") as f:
    f.write('static const char gzip_text[] = "%s";\n' % text.decode())
    f.write("static const unsigned char gzip_bytes[] = {%s};\n" % ",".join(str(b) for b in packed))
PY

# A certificate for the address 127.0.0.1 only, valid for two days.
if [ ! -s "$out/test-cert.pem" ] || [ ! -s "$out/test-other.pem" ] || ! openssl x509 -checkend 3600 -noout -in "$out/test-cert.pem" >/dev/null 2>&1; then
  openssl req -x509 -newkey rsa:2048 -nodes -days 2 -subj "/CN=tube-js test" \
    -addext "subjectAltName=IP:127.0.0.1" -keyout "$out/test-key.pem" -out "$out/test-cert.pem" 2>/dev/null
  # And one that has nothing to do with the server, to be the wrong root.
  openssl req -x509 -newkey rsa:2048 -nodes -days 2 -subj "/CN=tube-js unrelated" \
    -keyout "$out/test-other-key.pem" -out "$out/test-other.pem" 2>/dev/null
fi

"$CC" "${WARN[@]}" "${SAN[@]}" -I"$here" -I"$out" \
  "$here/test_bindings.c" "$here/tube_js_http.c" "$here/tube_js_platform.c" \
  -o "$out/test_bindings" -lcurl -lssl -lcrypto -lpthread
"$out/test_bindings" "$out/test-cert.pem" "$out/test-key.pem" "$out/test-other.pem"

"$CC" "${WARN[@]}" "${SAN[@]}" -DTUBE_JS_CONSOLE_RANDOM=1 -I"$here" \
  "$here/test_random_console.c" -o "$out/test_random_console"
"$out/test_random_console"

# The link probe, on the host: the same engine tag and the same C as the PS5
# build, run for real. The engine itself is third-party code and is built
# without the sanitizers.
qjs=$("$here/fetch_quickjs.sh")
if [ ! -f "$out/quickjs.o" ] || [ "$qjs/quickjs-amalgam.c" -nt "$out/quickjs.o" ]; then
  "$CC" -O2 -w -c "$qjs/quickjs-amalgam.c" -o "$out/quickjs.o"
fi
"$CC" "${WARN[@]}" "${SAN[@]}" -I"$here" -I"$qjs" \
  "$here/link_probe.c" "$here/tube_js_http.c" "$here/tube_js_platform.c" "$out/quickjs.o" \
  -o "$out/link_probe" -lcurl -lpthread -lm
"$out/link_probe" | tee "$out/link_probe.txt"
grep -qF 'engine 0.17.0: 0 {"sum":42,"re":"34","big":"1180591620717411303424","date":"1970-01-01T00:00:00.000Z","map":["k"]}' "$out/link_probe.txt"
grep -q '^bindings: 0 libcurl/' "$out/link_probe.txt"
echo "test_bindings: ok"
