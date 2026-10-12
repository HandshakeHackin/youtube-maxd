# The embedded JavaScript engine on the PS5: build, link proof and native bindings

This directory is the PS5-toolchain half of making ProsperoTube self-sufficient:
QuickJS-NG compiled with the PS5 SDK, proof that it links into the app the way
EVO's final link works, and the native primitives a JavaScript `fetch` shim
needs (HTTPS, clock, timers, random bytes, hashes). Everything here was built
and tested **offline**: compile, link and host tests only. Nothing in this
directory has run on a console; the section "What only a console can settle"
lists what that leaves open.

It does not touch `native/tube_provider.c`, the overlays or the other
`standalone/` files. Two things here duplicate what the host half
(`standalone/fetch_quickjs.sh`) has and should be unified later:
`fetch_quickjs.sh` (same tag, same archive; this copy pins the checksum) and
the engine object built for the host by `test_bindings.sh`.

| Command | What it does |
|---|---|
| `./test_bindings.sh` | Host tests under ASan + UBSan; server on 127.0.0.1 only |
| `./build_ps5.sh` | PS5 static library, bindings objects, probe ELF, relink of the app, `build/report.txt` |
| `TUBE_JS_BUNDLE=<bundle.js> ./build_ps5.sh` | The same, with the service bundle embedded, for the size figure |

`build_ps5.sh` reads (never writes) a prepared and built EVO work tree:
`TUBE_PS5_EVO_TREE`, or the first of `~/.cache/prosperotube/native*` whose last
build left every object. The SDK is `~/.cache/prosperotube/sdk`
(`PS5_PAYLOAD_SDK`); `/usr/lib/llvm-18/bin` is put on `PATH`.

## 1. Engine

* **QuickJS-NG `v0.17.0`**, MIT. The release's `quickjs-amalgam.zip`
  (SHA-256 `a0955463c74809173a253ff87365095e6972e8b171cfb6aa8a1e781adf35cfeb`),
  fetched by `fetch_quickjs.sh` into the git-ignored `.cache/`. The same tag as
  the host half, which matters if bytecode is ever shared.
* **Flags**: exactly EVO's for a C source -
  `-g -O2 -fno-plt -fno-stack-protector -ffunction-sections -fdata-sections
  -DEVO_APP_MODULE=1 -DEVO_TARGET_PS5=1` (`projects/evoplayer/Makefile` `CFLAGS`,
  `scripts/package-app.sh:119` `TFLAGS`, the compile line of `tube_provider.c` in
  `output/app/.build/compile.log`) - plus `-w`, because the engine is
  third-party code and EVO compiles with `-Wall`.
* **Source patches: none. Extra `-D`: none. Shim headers: none.** The
  amalgamation takes its FreeBSD paths (the SDK compiler defines `__FreeBSD__`
  9 and `__PROSPERO__`). What the task expected to be trouble, and what it was:

  | Topic | Result |
  |---|---|
  | `quickjs-libc` (`std`/`os`) | Not compiled: it is behind `QJS_BUILD_LIBC`, which is left undefined. No `environ`, `dlopen`, `fork`, signals, workers |
  | `malloc_usable_size` | `<malloc_np.h>` exists in the SDK. In the app the call binds to EVO's allocator, which defines it (`tools/native-app/stubs/malloc_shim.c:407`), so the engine's memory accounting is exact |
  | `alloca`, `fenv`, `sys/time.h` | Compile as they are |
  | Threads (`pthread_once`, mutex, condition) | Compile; `pthread_condattr_init/destroy/setclock` are new imports (libkernel has them), used only by `Atomics.wait` |
  | Atomics | `CONFIG_ATOMICS` on, lock-free builtins, no libatomic needed |
  | Stack-limit detection | `__builtin_frame_address`; needs `JS_UpdateStackTop()` on the thread that runs the engine (section 4) |
  | `localtime_r`, `__assert` | Come from the SDK's static `libc.a` (`localtime.o` wraps `localtime_s`, which the app already imports; `assert.o`). EVO's link already pulls both members |
  | JIT | None exists in QuickJS |

* Output: `build/libquickjs.a` (one member, `quickjs.o`; text 1,040,160 bytes,
  data 29,680).

## 2. Link proof

EVO's real final link is not the `$(ELF)` rule of its Makefile but
`scripts/package-app.sh` sections 6-8: its own CRT and allocator objects,
`prospero-lld -T tools/native-app/ps5-pie.ld --version-script app-symbols.map
--exclude-libs=ALL` (line 601), the static port archives in one group (line
417: `... libssl libcrypto ... libz libbz2 liblzma libzstd libm`), the SDK
stub libraries `--as-needed`, the SDK's static `libc.a` last, and then the
converter `ps5-native-tool link` that rewrites the LLVM PIE into the PS5 module
that is signed into `eboot.bin`. `build_ps5.sh` repeats that, with the tail
objects compiled from the EVO tree's own sources, twice:

1. **The probe alone.** `link_probe.c` creates a runtime (memory limit, stack
   limit), compiles a script to bytecode, writes it, reads it back, runs it,
   drains the job queue (an `async` function with `Promise.all`, a regular
   expression, a BigInt, a `Date`, a `Map`) and returns the result through
   `tube_js_link_probe()`; `tube_js_link_probe_bindings()` calls each binding.
   Result: `build/probe-pie.elf` links with **no undefined symbol**, and the
   converter accepts it (`build/probe.elf`, 9.0 MB, most of it OpenSSL). The
   same C file runs on the host in `test_bindings.sh` and prints
   `{"sum":42,"re":"34","big":"1180591620717411303424",...}`.
2. **The app itself.** EVO's 218 objects from the last build, relinked once as
   they are and once with `quickjs.o`, the bindings, libcurl and the embedded
   data added. Both link and convert. The baseline relink imports exactly what
   EVO's recorded `pie-imports.txt` lists, so the comparison is like for like.

What did not resolve at first, and how each was settled:

| Symbol | Cause | Settled by |
|---|---|---|
| duplicate `posix_fallocate`, `posix_fadvise` | libcurl's `if_nametoindex` came from `libc.a(if_nametoindex.o)`, which drags in `libc.a(syscalls.o)` (raw system calls, written for payloads) and collides with EVO's `libc_ext.c` | `if_nametoindex`, `getifaddrs`, `freeifaddrs` defined in `tube_js_console.c`; the build fails if `syscalls.o` is ever pulled in again |
| `getpwuid_r`, `pipe2` | would come from `libc.a` | defined in `tube_js_console.c` (same stand-ins as the hardware-proven boilerplate `console_curl.c`) |
| `fnmatch` | nothing supplies it | defined in `tube_js_console.c` |
| `ZSTD_trace_*` (weak) | libcurl pulls zstd's stream decoder; the converter refuses an import nobody exports | defined in `tube_js_console.c` and left visible to `libzstd.a` |
| `evo_boot_log` (weak, from `evo_dns.c`) | probe only; the app has it | `link_probe_tail.c` |
| `fcntl` on sockets | not a link error: the console's libc refuses it at run time | `--wrap=fcntl`, resolved inside one object (section 3) |

**Imports the app gains** (16; each is exported by an SDK stub library, none
only by `libScePosixForWebKit`, which is the known null-at-run-time case and
makes the build fail):

* libkernel: `__inet_ntop`, `__inet_pton`, `pipe`, `select`,
  `pthread_condattr_init`, `pthread_condattr_destroy`,
  `pthread_condattr_setclock`, `sceKernelGetFsSandboxRandomWord`
* libSceLibcInternal: `__swbuf`, `putc`, `putchar`, `setvbuf`, `basename`,
  `gethostname`, `memrchr`, `realpath`

The libcurl ones are the imports the owner's other apps already run with (same
PacBrew archive). The engine's own are `__swbuf`/`putc`/`putchar` (debug dumps)
and the three `pthread_condattr_*`.

## 3. HTTP, TLS and certificates

### What the app has today (EVO `9715f6e`, paths under `third_party/evo/`)

* **Provider requests** go through `evo_net_request_async`
  (`native/tube_provider.c:2442` `send_request`, `:2519` `begin`): EVO's own
  HTTP/1.1 client on BSD sockets, one worker thread created with the default
  stack (`projects/evoplayer/addons/src/evo_net.c:1298`), callbacks delivered
  on the main thread from `evo_net_poll()` (`addons/include/evo_net.h:143`,
  `core/src/Application.cpp:1163`). It follows redirects, does no content
  decoding (no gzip), and its single worker runs one request at a time.
* **Its TLS is OpenSSL without certificate verification**: `evo_net.c:328-341`
  creates the `SSL_CTX` and never calls `SSL_CTX_set_verify` or loads a root
  (there is no such call anywhere under `projects/evoplayer`). Fine for a LAN
  companion; not acceptable for talking to YouTube directly.
* **Media** is opened by FFmpeg's own `http`/`https`/`tls` protocols
  (`scripts/build-ffmpeg.sh:351,549,551`, `--enable-openssl`), with the
  provider's headers and user agent passed as options
  (`media/src/evo_stream_io.c:219-233`, `:480` for the parallel reader). No
  `tls_verify` or `ca_file` option is set, so FFmpeg's default (no
  verification) applies. No CA bundle is in the package.
* **Linked today**: `libssl.a`/`libcrypto.a` (OpenSSL 3.5.2), `libz.a` 1.3.2,
  `libzstd.a` 1.5.6 (`Makefile:66-67`, `package-app.sh:417-421`). **libcurl is
  not linked**, but PacBrew v0.39 installed `libcurl.a` 8.18.0 and `libpsl.a`
  0.21.5 into the same SDK. `prospero-curl-config`/`libcurl.pc` list its
  features: `alt-svc AsynchDNS HSTS HTTPS-proxy IPv6 Largefile libz NTLM PSL
  SSL threadsafe TLS-SRP UnixSockets zstd`. So: OpenSSL backend, threaded name
  lookup, gzip/deflate/zstd. **No brotli and no HTTP/2** - requests go out as
  HTTP/1.1 and `Accept-Encoding` never offers `br`.
* Name lookup is EVO's `tools/native-app/stubs/evo_dns.c` (`getaddrinfo` on the
  console's resolver); libcurl's lookup threads use it as they are.
* `sceHttp` is not an option: this app elevates through Lapy for
  `/data/prosperotube`, and the owner's other apps found `sceHttp`/`sceSsl`
  fail on every public site once elevated, while PacBrew libcurl + OpenSSL
  works both elevated and sandboxed.

### What was chosen

**libcurl 8.18.0 (multi interface) on the OpenSSL 3.5.2 the app already
links**, driven from the engine thread; no worker thread of its own beyond
libcurl's short-lived lookup threads.

`build/tube_js_net.o` is `tube_js_http.o` + `tube_js_console.o` + the libcurl
and libpsl members they need, joined by `prospero-lld -r --wrap=fcntl` and then
reduced by `prospero-objcopy` to export only `tube_js_*` (and `ZSTD_trace_*`).
Consequences: the `fcntl` stand-in reaches libcurl's calls only (EVO, FFmpeg
and OpenSSL keep the `fcntl` they have; the final link needs no `--wrap`), and
no libcurl symbol or libc stand-in can collide with anything in the app.

`tube_js_console.c` carries what the hardware-proven boilerplate
(`ps5-native-app-boilerplate/examples/update-check/console_curl.c`) established:
`SO_NBIO` (0x1200) on every new socket through `CURLOPT_SOCKOPTFUNCTION`, the
`fcntl` stand-in for sockets, `CURLOPT_NOSIGNAL`, and the stand-ins of section 2.

**Certificates.** Verification is always on (`VERIFYPEER`, `VERIFYHOST`); there
is no switch to turn it off. The roots are a **pinned Mozilla bundle**:
curl.se's dated extract `cacert-2026-09-25.pem` (121 roots, 188,900 bytes,
SHA-256 `a41b5d356aea97a529fe27e0f7316d2f9d946d75927476cf9cf1b90637d00505`,
`fetch_cacert.sh`), **embedded in `eboot.bin` as a byte array** by
`embed_data.py` and handed to libcurl in memory (`ca_data`,
`CURLOPT_CAINFO_BLOB`). Embedded rather than a package file because EVO
embeds its own UI assets for a hardware reason - directory operations on the
app's `/app0` mount fail with EPERM (`tools/bundle_rml_assets.py`, issue #60) -
and a read from memory involves no file system at all. `ca_file` exists too, and `tube_js_console_ca_file()` returns the
console's own list (`/system/common/cert/CA_LIST.cer` when elevated, under the
sandbox word otherwise - the path the owner's other apps use) as a fallback
that needs no rebuild when the pin ages. The certificate data is MPL-2.0: the
file keeps its header, and the notice lines are in section 5.

### The bindings

`tube_js_http.h` - one client per engine thread; only `wake` may be called
from another thread:

```
tube_js_http *tube_js_http_new(const tube_js_http_config *config);
void          tube_js_http_free(tube_js_http *client);
int           tube_js_http_start(client, const tube_js_http_request *, uint32_t *id);
int           tube_js_http_cancel(client, uint32_t id);
int           tube_js_http_poll(client, int wait_ms);
tube_js_http_response *tube_js_http_take(client);
void          tube_js_http_response_free(tube_js_http_response *);
int           tube_js_http_active(client);
int           tube_js_http_wake(client);
const char   *tube_js_http_error_name(int error);
const char   *tube_js_http_backend(void);
```

* Request: method, URL (`http`/`https` only), headers, body bytes (copied),
  `redirect` = follow / manual / error, timeout, response bound.
* Response: id, error class (`ABORTED`, `TIMEOUT`, `TOO_LARGE`, `REDIRECT`,
  `RESOLVE`, `CONNECT`, `TLS`, `NETWORK`, ...), status, final URL, `redirected`,
  the final answer's headers, the decoded body (NUL-terminated for convenience).
* Redirects are followed here, by fetch's rules, not by libcurl: 301/302 turn
  POST into GET, 303 turns everything but GET/HEAD into GET (body and
  `Content-*` headers dropped), 307/308 resend method and body, at most 20
  hops, `Authorization` dropped when the origin changes, a redirect out of
  http(s) refused. The time limit covers all hops.
* Bounds: decoded body (default 16 MiB), headers (256 KiB / 512 lines),
  requests in flight (64), connections (6).
* Headers the transport owns are dropped from the caller's list
  (`Accept-Encoding`, `Content-Length`, `Host`, `Connection`, `Expect`,
  `Transfer-Encoding`, ...); a name or value with a line break is refused.
* No cookie jar unless `cookies = 1` (then in memory, per client).

`tube_js_platform.h`:

```
uint64_t tube_js_monotonic_ns(void);  uint64_t tube_js_monotonic_ms(void);  int64_t tube_js_wall_ms(void);
int      tube_js_random_bytes(void *output, size_t length);
void     tube_js_sha1_init/update/final/digest(...);   void tube_js_sha256_init/update/final/digest(...);
tube_js_timers *tube_js_timers_new(void);              void tube_js_timers_free(...);
uint32_t tube_js_timers_add(timers, now_ms, delay_ms, repeat_ms);
int      tube_js_timers_cancel(timers, id);
int      tube_js_timers_next_delay(timers, now_ms);    uint32_t tube_js_timers_pop_due(timers, now_ms);
```

* Random bytes on the console: the checked `kern.rng_pseudo` reader the app
  already uses for phone pairing (`native/tube_random.h`), same rules (64 bytes
  per call, a short or oversized answer is a failure, a failure leaves zeros).
  On the host: `getrandom`.
* SHA-1 and SHA-256 are written here from FIPS 180-4 (no OpenSSL dependency in
  the bindings), checked against the standard vectors.
* The timer queue holds deadlines only; the engine side maps ids to functions.

`tube_js_console.h` (console only): `tube_js_console_socket_setup`,
`tube_js_console_ca_file`.

### Host test results

`./test_bindings.sh` - clang, `-fsanitize=address,undefined`, leak detection on:

```
test_bindings: 466 checks, 0 failures, 98 requests served on 127.0.0.1 (libcurl/8.18.0 OpenSSL/3.5.5 ...)
test_random_console: 0 failures
engine 0.17.0: 0 {"sum":42,"re":"34","big":"1180591620717411303424","date":"1970-01-01T00:00:00.000Z","map":["k"]}
test_bindings: ok
```

Covered: status, headers, empty and HEAD answers, gzip decoding, binary
bodies, the request as the server saw it (method, headers kept and dropped,
`Content-Length`), every redirect rule above, manual and error modes, the
redirect limit, cross-origin `Authorization`, body and header bounds (exact
size passes, one byte more fails), timeout, cancellation, a refused
connection, twelve concurrent requests over six connections, the request
limit, wake from another thread, cookies off and on, freeing a client with
work in flight; TLS against a server with a throw-away certificate: trusted
root as a file and in memory, wrong host name, unknown root, wrong root in
memory, bytes that are no certificate, a missing root file (all fail closed);
hash vectors including one million `a` in odd-sized pieces; the console random
path against a stand-in kernel call that fails, answers short and answers too
long. The host's libcurl is the same 8.18.0 as the console's. No test resolves
a name over the network or leaves the machine.

## 4. Threading and lifecycle (design; nothing of this is wired in yet)

* **One dedicated thread owns the `JSRuntime`**, the context, the
  `tube_js_http` client and the timer queue, and is the only thread that ever
  touches them. It creates the runtime itself (the stack-limit check measures
  from the creating thread; otherwise call `JS_UpdateStackTop`).
* **Stack**: create the thread with an explicit size, as EVO does everywhere -
  the console's default was too small even for a decoder call
  (`media/include/evo_thread.h:13`, 1 MiB for playback threads;
  `core/src/screens/ProviderHostScreen.cpp:185` and
  `core/src/Application.cpp:715`, 2 MiB). The SDK headers name no default.
  Proposed: **4 MiB** thread stack, `JS_SetMaxStackSize(runtime, 3 MiB)` so the
  engine reports "stack overflow" as an exception a megabyte before the real
  stack ends. (The engine's default limit is 1 MiB, and compiling the 1.6 MB
  bundle fits in it.) TLS handshakes run on this thread too, from the poll call
  at the top of the loop, never under a deep JavaScript stack.
* **Memory**: `JS_SetMemoryLimit(runtime, 128 MiB)` to start with; the bundle's
  bytecode alone is 6.4 MB of heap. All of it comes from EVO's allocator
  (`malloc_shim.c`: flexible memory, then direct memory for large blocks), which
  playback also draws on, so run `JS_RunGC` when a command completes and before
  playback starts. The host half should report the real peak per command.
* **Requests in**: the provider keeps its main-thread `Request` state machine.
  Where `send_request` calls `evo_net_request_async` today, it puts a command
  (JSON text) on a mutex-protected queue and calls `tube_js_http_wake()`.
  **Results out**: the engine thread puts each answer on a completed queue; the
  main thread drains it once per frame next to `evo_net_poll()`
  (`Application.cpp:1163`) and calls the same `response` handler, so everything
  after the request still runs on the main thread as it does now.
* **The loop**: take commands and call the bundle's dispatcher; run pending
  jobs (`JS_ExecutePendingJob`) until none are left; pop due timers; then
  `tube_js_http_poll(client, wait)` with `wait` = the next timer's delay capped
  at 100 ms (0 if jobs are pending); take completed responses and settle their
  `fetch` promises. The 100 ms cap makes the loop correct even if
  `curl_multi_wakeup` cannot work on the console (it needs a pipe); with a
  working wake it is only a heartbeat.
* **Runaway scripts**: `JS_SetInterruptHandler` returning non-zero when the
  stop flag is set or a command passed its deadline.
* **Soft close**: `Application.cpp:1102-1126` releases media on the first
  soft-close frame and parks after about 150 frames; the parked loop only
  sleeps. Stop the service on that first frame: set the stop flag, wake, and
  the thread cancels every request (immediate; `CURLOPT_QUICK_EXIT` means no
  wait for a name lookup), frees context, runtime and client, and exits; the
  main thread joins it before the park. Parked, the app then holds no engine
  heap, no socket and no timer - "soft close releases resources" holds. While
  the app is in use and idle, keep the runtime (session state) and let libcurl
  close idle connections on its own (118 s default).
* **Where the bundle comes from**: embedded in `eboot.bin` like the roots
  (`tube_js_bundle[]`), evaluated once when the thread starts. A development
  override from `evo_data_path("js/service.js")` is easy to add but is a
  decision for the owner (anyone with FTP could replace the code that holds the
  account tokens).
* **Bytecode precompilation: not worth it now.** Measured on the host with
  `bytecode_bench.c` on the host half's 1.63 MB bundle (Ryzen 9 3900X, the same
  Zen 2 cores as the console at a higher clock): compile from source 60.6 ms;
  bytecode 4.62 MB, read back in 4.7 ms. It would save well under 0.2 s once per
  launch, cost 3 MB of package, tie the bundle to one exact engine build
  (bytecode is same-version only and unchecked) and need a host tool built from
  the same amalgamation. Revisit only if a console measurement shows start-up
  compile above ~0.3 s.

## 5. Package integration

Applied since by `scripts/embedded_service_overlay.py` (section 7); what
follows is the plan it implements, kept for the reasoning.

`scripts/package-app.sh` needs **no change**: OpenSSL, zlib and zstd are
already in its archive group, libcurl and libpsl are inside `tube_js_net.o`,
and nothing new is shipped as a file. These lines were checked against a copy
of the prepared EVO Makefile (the four objects build with EVO's own rules and
appear in `make print-objects`).

`scripts/prepare_native.py`, next to the existing `addons/src` copies:

```python
js = ROOT / "standalone/ps5"
qjs = pathlib.Path(subprocess.run([str(js / "fetch_quickjs.sh")], check=True, capture_output=True, text=True).stdout.strip())
roots = subprocess.run([str(js / "fetch_cacert.sh")], check=True, capture_output=True, text=True).stdout.strip()
shutil.copyfile(qjs / "quickjs-amalgam.c", WORK / player / "addons/src/quickjs-amalgam.c")
shutil.copyfile(qjs / "quickjs.h", WORK / player / "addons/include/quickjs.h")
for name in ("tube_js_platform", "tube_js_http", "tube_js_console"):
    shutil.copyfile(js / (name + ".c"), WORK / player / "addons/src" / (name + ".c"))
    shutil.copyfile(js / (name + ".h"), WORK / player / "addons/include" / (name + ".h"))
subprocess.run([sys.executable, str(js / "embed_data.py"), str(WORK / player / "addons/src/tube_js_data.c"),
                f"tube_js_ca_pem={roots}", f"tube_js_bundle={bundle}"], check=True)   # bundle: the host half's build output
replace(player + "Makefile", "    addons/src/tube_provider.c",
        "    addons/src/tube_provider.c \\\n    addons/src/quickjs-amalgam.c \\\n"
        "    addons/src/tube_js_platform.c \\\n    addons/src/tube_js_data.c")           # + the engine host unit(s)
replace(player + "Makefile", "C_OBJS := $(C_SRCS:.c=.o)\n", MAKE_TUBE_JS)
```

with `MAKE_TUBE_JS` (recipe lines start with a tab):

```make
C_OBJS := $(C_SRCS:.c=.o) addons/src/tube_js_net.o
addons/src/quickjs-amalgam.o: CFLAGS += -w
TUBE_JS_LIB := $(PS5_PAYLOAD_SDK)/target/user/homebrew/lib
addons/src/tube_js_net.o: addons/src/tube_js_http.o addons/src/tube_js_console.o
	$(LD) -r --wrap=fcntl -o $@ $^ $(TUBE_JS_LIB)/libcurl.a $(TUBE_JS_LIB)/libpsl.a
	$(OBJCOPY) --wildcard --keep-global-symbol='tube_js_*' --keep-global-symbol='ZSTD_trace_*' $@
```

`tube_js_http.c` and `tube_js_console.c` are deliberately **not** in
`ADDON_SRCS`: they reach the link only inside `tube_js_net.o`.

Client set-up on the console:

```c
tube_js_http_config config = {.ca_data = tube_js_ca_pem, .ca_length = tube_js_ca_pem_length,
                              .socket_setup = tube_js_console_socket_setup, .poll_slice_ms = 10};
```

Notices (`THIRD_PARTY_NOTICES.md`, texts copied to
`assets/prosperotube-notices/` as `prepare_native.py` does for cJSON): QuickJS-NG
v0.17.0, MIT (its `LICENSE` is not in the amalgamation archive: add
`native/licenses/QuickJS-NG-MIT.txt` from the tag); curl 8.18.0, curl license;
libpsl 0.21.5, MIT; Mozilla root certificates as extracted by curl.se on
2026-09-25, MPL-2.0. OpenSSL, zlib and zstd are already covered.

**Size.** `eboot.elf` (what gets signed; `eboot.bin` is within a few percent of
it) from the relink of the app's own objects:

| | bytes |
|---|---|
| as it is today | 39,540,820 |
| + engine, bindings, libcurl | about +2,242,800 (QuickJS 1.07 MB, libcurl + libpsl + bindings 0.75 MB, the rest mostly OpenSSL and zstd members the app did not need before) |
| + root certificates | +188,900 |
| + service bundle (host half's current build) | +1,626,285 |
| **total** | **43,598,780 (+4,057,960, about +10 %)** |

## 6. What only a console can settle

1. **The 16 new imports bind on the console's firmware.** Each is in an SDK stub
   library, and the libcurl ones already run in the owner's other apps; the
   engine's (`__swbuf`, `putc`, `putchar`, `pthread_condattr_*`) have no such
   precedent. A missing one shows as a failure before `main` or a null call.
2. **libcurl in this app, elevated through Lapy**: the `fcntl` stand-in,
   `SO_NBIO`, the wake-up `pipe()`, and `poll()` over a pipe and sockets
   together. All proven in other titles, none in this one. If the wake pipe
   fails, `tube_js_http_wake()` returns -1 and the loop's 100 ms cap carries it.
3. **Certificate verification end to end**: the embedded bundle against
   YouTube's and Google's chains, with the console's clock (OpenSSL checks
   dates through `gmtime_r` from `libc.a`), and OpenSSL's own entropy
   (`/dev/urandom`, which EVO's HTTPS already relies on).
4. **Speed of the interpreter on the console**: bundle compile at start, and
   the heavy step - evaluating YouTube's player script for signatures.
5. **Memory under playback**: engine heap plus libcurl next to a 4K software
   decode in the same flexible/direct pools.
6. **Thread stack**: that a 4 MiB stack is granted and the 3 MiB engine limit
   leaves room for the deepest native call.
7. **Responses without brotli or HTTP/2**: the console's libcurl speaks
   HTTP/1.1 and decodes gzip, deflate and zstd only. Expected to be enough, not
   observed here (the host's libcurl has both, so the host half's live checks do
   not show it; `curl --http1.1 --compressed` with `br` removed would).
8. **Shutdown inside the soft-close window**: stop, join and park with requests
   in flight and a name lookup pending.

## 7. Checked against the hardware-proven libcurl reference

Reference: `ps5-native-app-boilerplate/examples/update-check/console_curl.c`
(line numbers below) and the written guide `ProsperoRadio-curl-guide/README.md`
(sections). Both describe PacBrew libcurl 8.18.0 + OpenSSL 3.5.2 running on a
console, elevated and sandboxed.

| Item | Reference | Here | State |
|---|---|---|---|
| Name lookup | own `getaddrinfo`, `freeaddrinfo`, `gai_strerror`, `getnameinfo` on `sceNetResolver` (lines 173-269, guide 5): the SDK's come from `libScePosixForWebKit` and are null | The app already has them: EVO's `tools/native-app/stubs/evo_dns.c`, linked into every package. libcurl's lookup threads call that one (no `getaddrinfo` import remains in the relinked app) | covered |
| `gethostbyname` | returns NULL (line 248) | was a null import of the app (OpenSSL `bio_sock.o`): now defined in `tube_js_libc.c` | **gap, closed** |
| `isatty`, `mkstemp` | defined (lines 397-410, guide 4): they link from `libScePosixForWebKit` | were null imports of the app (FFmpeg `log.o`, `file_open.o`): now defined app-wide in `tube_js_libc.c` | **gap, closed** |
| `SO_NBIO` | `CURLOPT_SOCKOPTFUNCTION` (lines 78-98, guide 6) | `tube_js_console_socket_setup` through `socket_setup` | same |
| `NOSIGNAL` | line 95 | always set | same |
| `fcntl` on sockets | `--wrap=fcntl` (lines 104-150) | the same function, wrapped inside `tube_js_net.o` only | same |
| CA list | `/system/common/cert/CA_LIST.cer`, else under the sandbox word (lines 53-72, guide 7) | embedded Mozilla bundle; `tube_js_console_ca_file()` returns the reference's paths as the fallback | same, plus the pin |
| `gmtime_r` | own, must be right (line 438) | the SDK's `libc.a(gmtime.o)`, a real one, as EVO's link already uses | covered |
| `_setjmp`, `dladdr`, `recvmmsg`, `sendmmsg` | lines 309-378 | EVO's `stubs/libc_ext.c` | covered |
| `getpwuid_r`, `if_nametoindex`, `pipe2`, `fnmatch`, `ZSTD_trace_*` | lines 271-356, 412-434 | `tube_js_console.c`, same bodies | same |
| `openlog`, `closelog`, `popen`, `pclose` | lines 319-394 | nothing in this link refers to them | not needed |
| Waiting | "`curl_multi_poll` does not always wake early" on the console; wait 10 ms and only after a pass without progress (guide 8 and 10.5) | `tube_js_http_poll` trusted one long wait | **gap, closed**: `poll_slice_ms = 10` waits in 10 ms slices and moves the transfers along between them |
| Wake | `curl_multi_wakeup` (needs the pipe from `pipe2`) | relied on it alone | **gap, closed**: a flag set by `tube_js_http_wake()` and read every slice; the pipe is no longer needed for a wake to be seen |
| HTTP version, buffer | `CURL_HTTP_VERSION_1_1`, 256 KiB (guide examples) | now set the same | aligned |

Two more null imports of today's app turned up in the same scan and are **not**
touched here, because nothing on the libcurl path calls them: `strcasestr`
(EVO's `provider_xtream.o`, SDL) and `arc4random_buf` (FFmpeg
`random_seed.o`).

## 8. Wired into the app

**Flag.** `TUBE_EMBEDDED_SERVICE`. `scripts/build_ps5.sh` exports it as 1
unless told otherwise; `scripts/prepare_native.py` on its own (every host
test) leaves it off, so those tests build and behave as before. At run time a
file named `external-companion` in the app's data folder keeps the LAN
companion in a package that carries the service (development).

**Provider** (`native/tube_provider.c`, `tube_provider.h`):
`tube_embedded_mode/start/stop/error`, `tube_art_url_allowed`.
* `init()` decides the mode, reads the store, moves the stored companion
  address and token aside (they are written back unchanged on every save and
  never used) and calls `tube_service_start(evo_data_dir(), ...)`. The
  provider's source becomes `http://127.0.0.1:<port>` with the returned token,
  in memory only.
* `begin()` never pairs in this mode: it makes sure the service runs (asking it
  again after a refused token) or fails the request with the service's reason.
* Failure: the app opens; the main menu's first row is "YouTube service did
  not start - select to retry" with `tube_service_error()` under it; Options
  retries too. The local library keeps working.
* Stop: `tube_embedded_stop()` on the first soft-close frame
  (`Application.cpp`, before "soft close: media released" is logged) and in
  the provider's shutdown. It cancels requests, clears the address and token,
  stops the service; afterwards requests fail quietly and nothing reaches the
  network path. The parked loop of `Application.cpp` never returns to the
  frame loop, so there is no resume today; `tube_embedded_start()` is the call
  for one.
* Hidden in this mode: the "Phone pairing" row, voice search (L3 does
  nothing, its scope stays closed), the address keyboard on Options, and in the
  package the on-screen texts that name the companion.
* Artwork: the fetch (`evo_rmlui_provider_art.cpp`) now asks
  `tube_art_url_allowed()`. External companion: unchanged. Built-in service:
  its own loopback address, or `https://` on exactly `i.ytimg.com`,
  `i9.ytimg.com`, `yt3.ggpht.com`, `yt3.googleusercontent.com`, default port,
  no user part. Size bounds are EVO's, unchanged. (The fetch itself is EVO's
  client, whose TLS does not verify; a wrong image is the worst case.)
* Streams: a choice must still be an address of the source, i.e. the loopback
  manifest. An optional `"headers"` text on a choice (`"Name: value\r\n"`
  lines) is passed to the player; anything malformed is dropped.

**Loopback.** EVO's client reaches `127.0.0.1` without a name lookup:
`evo_dns.c` answers IPv4 literals itself (`parse_ipv4_literal`, lines 148 and
399), and nothing in `evo_net.c` refuses loopback.

**Player facts** (FFmpeg 7.1.1 as built by `third_party/evo/scripts/build-ffmpeg.sh`):
* https is in: `--enable-openssl` (line 351), `--enable-protocol=https` and
  `=tls` (549, 551), `--enable-demuxer=dash` with libxml2 (468-469); the
  build's `config_components.h` has `CONFIG_HTTPS_PROTOCOL`,
  `CONFIG_TLS_PROTOCOL`, `CONFIG_DASH_DEMUXER` 1. `tls_verify` defaults to 0
  (`libavformat/tls.h:49`) and nothing sets it.
* A manifest from `http://127.0.0.1` may name absolute `https` addresses:
  `dashdec.c` resolves `BaseURL` with `ff_make_absolute_url` (493-500, 1703),
  accepts any protocol whose name starts with `http` (434), and the http
  protocol's default whitelist includes `https` and `tls` (`http.c:2073`).
* What the app passes (`media/src/evo_stream_io.c:222-233`): `headers` from
  the choice, `user_agent` (cleared for provider streams,
  `ProviderHostScreen.cpp:201-202` of the prepared tree, so FFmpeg's
  `Lavf/...` unless a `User-Agent:` line is among the headers), `reconnect*`,
  `rw_timeout` and `timeout` 5 s. The manifest is a "playlist" for EVO and is
  read by FFmpeg's own http, not the parallel reader (`:478-480`).
* What reaches the segment requests: only what `ffio_copy_url_options` copies
  from the manifest's connection (`dashdec.c:2043`) - `headers`, `user_agent`,
  `cookies`, `http_proxy`, `referer`, `rw_timeout`, `icy`. So the choice's
  headers and the read timeout do; **the reconnect options and `timeout` do
  not**. Nothing was changed here: whether googlevideo needs a particular
  `User-Agent` is the service's knowledge, and it can say so per choice.

**Build.** `scripts/embedded_service_overlay.py`, called last by
`prepare_native.py`: copies the engine, the bindings, `tube_js_libc.c` and
the service, embeds the bundle and the roots (`tube_js_data.c`), edits the
Makefile as in section 5, ships the license texts. The service is:

| | sources | bundle |
|---|---|---|
| `PROSPEROTUBE_STUB_SERVICE=1` | `standalone/ps5/tube_service_stub.c` (never starts) | `standalone/ps5/stub_bundle.js` |
| otherwise | every `standalone/src/*.c` except `main.c`, headers of `standalone/src` and `standalone/include` | `PROSPEROTUBE_JS_BUNDLE`, default `standalone/build/tube_js.bundle.js` |

Without the stub, a missing `standalone/include/tube_service.h`, bundle or
source stops the build with a message that names them.

**Tests.** `scripts/test_embedded_service.sh` (ASan/UBSan): the provider built
with the flag against a stand-in service and queued HTTP.

**To plug in the real service:** put its sources in `standalone/src/` and its
interface in `standalone/include/tube_service.h` (then delete
`standalone/ps5/tube_service.h`), produce the bundle, and build without
`PROSPEROTUBE_STUB_SERVICE`. It must create its client with the configuration
of `tube_js_console.h` on the console, read `tube_js_bundle` /
`tube_js_ca_pem` from `tube_js_data.h`, listen on a port between 20000 and
40000 (the console refuses 49152 and above), and cap its own waits as section
4 says.
