# Standalone service spike: youtubei.js inside QuickJS

Feasibility study for running ProsperoTube without the PC companion: the
QuickJS-NG engine embedded in a C program runs the pinned `youtubei.js` 18.1.0
and the companion's own library modules, with HTTPS supplied by libcurl. No
Node, no Python, no relay. Everything here runs on a PC; nothing is wired into
the PS5 app.

**Verdict: feasible with conditions.** Catalog, metadata, comments, channel
pages, captions and playback links all work from the embedded engine as an
anonymous Guest, at 25 MB of memory and about a quarter of a second per
resolve. The conditions:

1. Playback depends on one client identity (`VISIONOS`) that today returns
   links readable anywhere in the file without a proof token. Two other app
   clients lost that property (their links answer 403 some tens of megabytes
   in). YouTube can change this again at any time, so the service code must be
   updatable without a new package.
2. The session identity must come from YouTube (one extra request). A locally
   generated one gets "Sign in to confirm you're not a bot" on the app clients.
3. Nothing was run on a console. The engine's portability, libcurl with a CA
   bundle, and the EVO player reading two direct HTTPS links are still to be
   proven there.
4. Signed-in playback was not studied live (no sign-in was performed).

All live numbers below were measured on 2026-10-11 from this PC (WSL Ubuntu,
clang 21.1.8), Guest, no cookies.

## Build and run

```
standalone/build_host.sh        # fetches QuickJS-NG, bundles the service, links build/tube_js
standalone/test_host.sh         # offline: engine + shims + parser on canned responses

build/tube_js search "me at the zoo" [more]
build/tube_js info <videoId> [full]
build/tube_js comments <videoId>
build/tube_js channel <UC…>
build/tube_js resolve <videoId> [client]
build/tube_js probe <videoId> [client,…|all] [ranged]     # player answers + direct ranged reads
build/tube_js ranges <videoId> [client] [itag] [steps]    # reads deeper in one file
build/tube_js library '<json request>' …                  # any docs/LIBRARY_SERVICE.md action; "$cursor" = previous cursor
```

Output is one line of compact JSON; an `{"error":…}` answer exits with status 1.
Media and caption addresses are replaced by a description unless
`TUBE_JS_SHOW_URLS=1`. Other switches: `TUBE_JS_STATS=1` (timings, request
count, memory), `TUBE_JS_TRACE=1`, `TUBE_JS_VERBOSE=1`, `TUBE_JS_CACHE_DIR`,
`TUBE_JS_CA_BUNDLE`, `TUBE_JS_LOCAL_SESSION=1`, `TUBE_JS_MAX_HEIGHT`,
`TUBE_JS_CODEC`, `TUBE_JS_MINIFY=1` (build), `TUBE_JS_FIXTURES=<dir>` (answer
every request from canned files).

Needs: clang, libcurl development files (through `~/pt-wt/env.sh` on this
machine), Node with `companion/innertube/node_modules` (`npm ci
--ignore-scripts`) for esbuild at build time only.

| File | Lines | Role |
|---|---|---|
| `src/tube_js.c` | 517 | host: engine thread, job/timer loop, `__host` (HTTPS, SHA-1, random, clock, UTF-8, files, isolated evaluation) |
| `js/host_shims.js` | 176 | console, timers, TextEncoder/Decoder, base64, crypto, structuredClone, events, abort, Intl stub |
| `js/url_shim.js` | 165 | URL, URLSearchParams |
| `js/fetch_shim.js` | 123 | fetch, Request, Response, Headers; canned-response mode |
| `js/platform.js` | 42 | `Platform.load(...)` for youtubei.js, file cache |
| `js/node_crypto.js` | 49 | `randomUUID`, `createHash` for the library modules |
| `js/resolve.js` | 228 | client choice, format policy, manifest, captions (port of `sessions.py`, `captions.py`) |
| `js/service.js` | 229 | commands over `Library` (unchanged companion module) and the resolver |
| `js/selftest.js` | 99 | offline checks |

## 1. Engine

- **QuickJS-NG `v0.17.0`** (commit `6d46d07d04041b40f4f49eaa7fdebe44c314c699`,
  released 2026-09-18), MIT licence (Bellard, Gordon, Noordhuis, Ibarra
  Corretgé). Fetched as the release's `quickjs-amalgam.zip`, SHA-256
  `a0955463c74809173a253ff87365095e6972e8b171cfb6aa8a1e781adf35cfeb`, into the
  ignored `standalone/.cache`. It needs a `THIRD_PARTY_NOTICES.md` entry when
  it ships.
- One C translation unit (89,860 lines). `clang -std=c11 -D_GNU_SOURCE -O2`
  compiles it without errors in 19 s. Plain `-std=c11` fails only on
  `clock_gettime`, `CLOCK_MONOTONIC` and `readlink` being hidden by glibc
  without a POSIX feature macro; nothing in the compiler is missing.
- Interpreter only: no JIT, no executable memory, no `dlopen`. The `std`/`os`
  modules are left out (`QJS_BUILD_LIBC` undefined); the test asserts they do
  not exist.
- What it asks of the platform (undefined symbols of the object): libc
  (`malloc`/`realloc`/`free`, `malloc_usable_size`, stdio for diagnostics,
  `clock_gettime`, `gettimeofday`, `localtime_r`, `strtod`), libm, pthreads
  (mutex, condition variable, `pthread_once`, for `Atomics`), plus compiler
  features clang has: `<stdatomic.h>`, `alloca`, computed goto.
  `malloc_usable_size` has Linux and FreeBSD branches; a runtime created with
  its own allocator functions may leave it out.
- It needs a deep C stack for the player-script analysis: the host gives the
  engine thread 64 MB.
- Sizes: object 1.06 MB of code; the whole host program 1.22 MB (1.15 MB
  stripped), linked dynamically to libcurl.
- Not checked: a build with the PS5 toolchain. The repository's PS5 build uses
  the `prospero-*` clang of the payload SDK, FreeBSD-like libc and pthreads,
  which is the profile above, but that is an expectation, not a result.

## 2. The bundle in the engine

esbuild 0.25.10, `--format=iife --platform=neutral --target=es2022`, entry
`js/service.js`. `youtubei.js` is aliased to its platform-neutral entry
(`dist/src/platform/lib.js`) and `node:crypto` to `js/node_crypto.js`. The
companion's `library.mjs`, `device_auth.mjs` and the modules they import are
bundled **unchanged**. The bundle loads and initialises with no engine
conformance problem; no source of youtubei.js was patched.

One trap: youtubei.js modules import each other in cycles, and the bundle only
orders them correctly when `utils/Utils.js` is the first of its modules to be
imported, as its own platform entries do (otherwise: "parent class must be
constructor" at load).

What had to be supplied:

| Facility | Where | Notes |
|---|---|---|
| HTTPS request | C, libcurl | blocking, one at a time, reused connection; https only; gzip handled by libcurl; size and time limits |
| monotonic clock, sleep | C | timers and `performance.now` |
| random bytes | C (`getentropy`) | `crypto.getRandomValues`, `randomUUID` |
| SHA-1 | C | `Platform.shim.sha1Hash` (cookie authorisation header); 30 lines |
| UTF-8 encode/decode | C | `TextEncoder`/`TextDecoder`, `Response.text()`; engine strings do the work |
| file read/write/remove | C | youtubei.js cache (player analysis, later credentials); canned responses |
| isolated evaluation | C | a second context of the same runtime, without `__host` |
| job and timer loop | C + JS | C drains promise jobs; `setTimeout`/`setInterval` are a JS queue the loop polls |
| `fetch`, `Request`, `Response`, `Headers` | JS | whole-buffer bodies |
| `URL`, `URLSearchParams` | JS | hierarchical URLs; no IDNA |
| `btoa`/`atob`, `structuredClone`, `Event`/`EventTarget`/`CustomEvent`, `AbortController`/`AbortSignal`, `console`, `queueMicrotask` | JS | used only when the engine lacks them (QuickJS-NG already has `performance`, `queueMicrotask`, `atob`/`btoa`, `DOMException`) |
| `Intl` | JS stub | youtubei.js reads only `Intl.DateTimeFormat().resolvedOptions().timeZone` |
| SHA-256, `randomUUID` as `node:crypto` | JS | imported by seven library modules |
| `ReadableStream`, `FormData`, `File` | not provided | only `download()` and uploads use them; the platform gets classes that throw |

`Platform.shim.eval` (how youtubei.js runs the functions it extracts from the
player script) is `new Function`-equivalent evaluation in the isolated context,
so code that came from the network cannot reach the network or files.

## 3. Catalog, live

All through `Library.execute` (the companion's adapter), so the JSON is the
existing contract.

| Operation | Result | Requests | Time |
|---|---|---|---|
| `search "me at the zoo"` | 24 rows + cursor; first row `jNQXAC9IVRw`, "Me at the zoo", jawed, 19 s | 2 (config + search) | 0.55 s |
| continuation (network) | 24 more rows, cursor | 1 | 0.59 s |
| `channel UC4QobU6STFB0P71PMvOGN5A` | channel facts (tabs home/videos/playlists, search), 1 video | 3 | 0.25 s |
| `comments jNQXAC9IVRw` | 20 comments with likes, replies, pinned/owner flags, cursor | 2 | 0.41 s |
| `info jNQXAC9IVRw` (`getBasicInfo`) | title, author, channel, 19 s, views, category, keywords | 2 | 0.16 s |
| `info jNQXAC9IVRw full` (`getInfo`) | the same + likes, publish date, 20 suggestions | 3 | 1.04 s |

The config request is made once per session, not per operation; each line
above was a fresh process.

## 4. Playback links, live

Videos: `jNQXAC9IVRw` (240p, 19 s) and `aqz-KE-bpKQ` (Big Buck Bunny, up to
2160p60, 635 s). "Local session" = visitor identity generated by youtubei.js;
"YouTube session" = fetched from YouTube (now the default).

| Client | Player answer | Formats with an address | Ranged reads directly from the program |
|---|---|---|---|
| `VISIONOS` | OK with a YouTube session; refused with a local one | all 32: H.264 144p to 1080p60, VP9 and AV1 to 2160p60, AAC 44.1 kHz stereo (and DRC), Opus; HLS manifest offered too | video first 256 KB: 206 (`bytes 0-262143/1362269481`); video at 681 MB: **206**; audio first 256 KB: 206 |
| `ANDROID_VR` | OK with a YouTube session; "Sign in to confirm you're not a bot" with a local one | all 29, same video ladder, plus AAC 5.1 | first 256 KB, 1 MB, 7.7 MB: 206; 30.9 MB, middle, last 64 KB: **403** (Range header, with the client's User-Agent, and `range=` parameter alike) |
| `IOS` | OK with either session | all (10 / 26); no Opus | first 256 KB: 206; middle of the long file: **403** |
| `ANDROID` | OK | only itag 18 (muxed 360p H.264+AAC) | first 256 KB: 206 |
| `MWEB` | OK | local session: all 20, with an `n` parameter; YouTube session: itag 18 only | after the `n` transform: start and middle of the 433 KB file: 206; a long file was not tried |
| `TV` | "Sign in to confirm you're not a bot" (local) / "The page needs to be reloaded." (YouTube session) | none | |
| `TV_SIMPLY` | not-a-bot refusal | none | |
| `TV_EMBEDDED`, `WEB_EMBEDDED` | "This video is unavailable" | none | |
| `WEB` | OK | none: 14 formats without addresses (SABR only) | |

- **Usable today: `VISIONOS` only.** Its addresses arrive ready: no signature
  cipher, no `n` parameter, so the player script is not needed. No request
  header was needed either (the reads sent only `Range`). Links expire after
  21,600 s and carry the requesting IP address.
- Cross-check with yt-dlp 2026.08.19 from the main checkout's environment: its
  default selection also returns the https formats from `VISIONOS` (32) and
  reads at 12 % and 50 % of the 1080p60 file succeed; forced to `android_vr`
  it offers itag 18 only. This agrees with the table.
- The 403 deeper in the file is how a missing proof token shows on the other
  app clients: the start plays, a seek or the continued download fails.
- **Deciphering in QuickJS:** the player script (`5203c085`, signature
  timestamp 20732) is fetched, analysed by youtubei.js (meriyah parser) and its
  functions extracted correctly. The `n` transform of a live `MWEB` address
  took 22 ms and the transformed address served data. The signature-cipher
  path was not exercised live (no client returned ciphered formats today); it
  is covered only by the synthetic test. Cost of the analysis: see section 5.
- **DASH:** `info.toDash()` works: 52 KB, 32 representations with
  `SegmentBase` (init and index ranges come from the player answer), 22 ms, no
  extra request. It fetches only for OTF formats and post-live recordings, and
  refuses live streams (those have YouTube's own HLS/DASH manifest addresses).
  `resolve` instead builds the small two-track manifest the player is already
  validated with (`sessions.py`), about 6 KB per choice.
- **Captions:** track list from the player answer (en, de for the zoo video);
  the English track downloaded as WebVTT (HTTP 200, 440 bytes) and converted to
  SRT in JS (6 cues, "All right, so here we are, in front of the elephants").
  That download used the address from the `IOS` answer; `VISIONOS` lists the
  same tracks but its caption address was not fetched.
- `resolve aqz-KE-bpKQ` returns eight choices from "1080p 60 fps H264 / AAC"
  down, each with video and audio address and its manifest.

## 5. Cost

| Step | Wall time | Memory (peak RSS) | Notes |
|---|---|---|---|
| process start to bundle evaluated | 72–85 ms | 25 MB (JS heap 6 MB) | 1.62 MB source; 807 KB minified evaluates in 72 ms |
| first search, cold process | 0.64 s | 26 MB | 2 requests, 282 KB received; 532 ms of it is network |
| first resolve, cold process (`VISIONOS`) | 0.25–0.27 s | 25 MB | 2 requests: session + player answer |
| player script, first time for a player version | **10.0 s** | **376 MB** (JS heap 350 MB) | 2 requests, 2.7 MB script; parse + analysis in the interpreter |
| player script, cached | 0.09–0.12 s | 26 MB | 70 KB cache file; one small request to learn the current player id |
| one address transform (`n`) | 22 ms | | |

Sizes: bundle 1.62 MB (275 KB gzip), minified 807 KB (208 KB gzip); host
program 1.22 MB.

What would hurt on a console:

- The player-script analysis. It is only needed for clients whose addresses
  need a transform, which the working client does not. If it becomes necessary
  it must run in the background, once per player version, and it will be
  slower than here on the console's CPU. Start-up must never wait for it.
- HTTPS is blocking and serial: one slow request delays the next operation,
  and there is no cancellation mid-transfer (only a time limit). Good enough
  for a spike; the real service needs either libcurl's multi interface in the
  loop or a second engine context for playback.
- Bytecode was not needed: evaluating the source costs under 0.1 s.

## 6. Sign-in outlook (nothing was signed in)

From `core/OAuth2.js` and the companion's `device_auth.mjs`, the TV
device-code flow needs:

- `fetch` to `www.youtube.com`: GET `/tv` with the Cobalt User-Agent, GET of
  the TV script it references (to read the client id and secret by regular
  expression), POST `/o/oauth2/device/code`, POST `/o/oauth2/token` (poll and
  refresh), POST `/o/oauth2/revoke`.
- `Platform.shim.uuidv4`, `Date`, JSON; `TextEncoder`/`TextDecoder` and the
  platform `Cache` if youtubei.js stores the credentials itself (key
  `youtubei_oauth_credentials`); `setInterval` only for its own polling loop,
  which the companion module replaces with one bounded poll per call.
- `structuredClone` and the session's `update-credentials` event for refreshed
  tokens (`createProfileClient`).

All of these exist in the shims, so the flow should run as is; it was not run.
Credentials (access token, refresh token, expiry, client id/secret) would be a
file under `/data/prosperotube/` written by the host, readable by the app
only, replacing the Python store (`device_auth.py`). The open question is
playback for a signed-in profile: the companion resolves it through the TV
client with the OAuth token (`oauth_video.py`), and the TV client refused
Guest playback today; whether it answers with usable links when signed in must
be measured before promising account playback.

## 7. Integration proposal

The native provider has one transport seam: `send_request()` in
`native/tube_provider.c` calls `evo_net_request_async(method, url, payload,
headers, n, callback, request)` against the companion origin and polls
`/v1/jobs/{id}`.

**Recommended: an in-process local service behind that seam.** A new native
unit owns one engine thread (this host program turned into a library) and a
request queue. `send_request()` hands `(method, path, payload)` to it instead
of the network and receives the same JSON through the same callback. The JS
side answers the routes the provider already uses (`/v1/health`, `/v1/jobs`,
`/v1/jobs/{id}`, `/v1/library`, `/v1/resolve`, …) with the documented
contracts, so the provider, its parsers and the interface stay as they are.

Becomes unnecessary: the relay (`/media/...`, `proxy.py`, `hls_buffer.py`),
pairing and PIN, bearer tokens, the source address setting, sessions as relay
capabilities, the yt-dlp worker and its JS runtime, FastAPI.

To re-implement (Python today):

| Piece | Today | Standalone |
|---|---|---|
| job layer and routes | `app.py` (931 lines) | JS dispatcher over `Library` and the resolver; jobs become promises with an id |
| resolve and format policy | `sessions.py`, `extractor.py`, yt-dlp | `js/resolve.js` (started here): client order, selection, choices; still to add: live streams (YouTube's HLS manifest address), chapters, storyboards, loudness, paid-content text, Shorts flag, audio language |
| manifest for the player | relay URL to a generated MPD | the same MPD written to a file under `/data/prosperotube/cache` (or HLS address for live) with direct addresses |
| captions | relay converts WebVTT to SRT | JS fetches and converts (`vttToSrt`, ported), host writes the `.srt` file; translated tracks = same address with `tlang` |
| link expiry | `media_refresh.py` | re-resolve on 403 or before `expires_in` (6 h), keep position |
| SponsorBlock, DeArrow, dislikes | `app.py`, `dearrow.py`, `dislikes.py` | direct HTTPS from JS to those origins (extend the origin allow-list per feature) |
| account sign-in and credential store | `device_auth.py` (316 lines) | `device_auth.mjs` already in the bundle + a credentials file |
| phone remote, voice, subscriptions import | `remote_lounge.py`, `voice.py`, `subscriptions_import.py` | separate decisions; they are PC-side services today |

What the EVO player needs for direct links: nothing special was required by
the server (no User-Agent, Origin or cookie; plain `Range`), reads anywhere in
the file, HTTPS to `*.googlevideo.com` with redirects allowed between its
hosts, tolerance of a 403 as "re-resolve", and, if it opens the manifest as a
local file, permission for that file to reference `https` resources.

Stages (rough sizes):

1. **Engine on the console** (3–4 days): build QuickJS-NG and the host as a
   library with the PS5 toolchain, libcurl with a CA bundle, run the offline
   test and one live search on a test console; measure start-up and memory.
2. **Local service transport** (about a week, ~800 lines JS + ~400 C): queue,
   route dispatcher, job semantics, switch `send_request()`; Guest catalog
   end to end with the existing interface.
3. **Playback** (about a week): resolve contract complete, manifest and
   caption files, expiry handling, player check with 1080p60 and 2160p60,
   seeking, live through HLS.
4. **Accounts** (1–1.5 weeks): device-code sign-in in process, credential
   file, account actions already in `Library`; measure signed-in playback.
5. **Extras** (3–5 days): SponsorBlock, DeArrow, dislikes; decide the fate of
   phone remote, voice and import.
6. **Hardening** (ongoing): service bundle replaceable by the in-app update
   without a new package, non-blocking HTTPS or a second context, cancellation,
   limits, removal of companion-only settings.

Main risks:

- **Proof tokens reaching `VISIONOS`.** Two app clients already lost
  full-file access. Fallbacks to keep ready: YouTube's HLS manifest (offered
  by `VISIONOS` today), another client identity, and at worst a token
  generator running in this engine, which is a much larger project. The
  replaceable bundle is what makes a quick answer possible.
- **Single source of truth is a pinned library.** youtubei.js 18.1.0 parsers
  and client versions age; updates must be testable offline (this test) and
  live (the `probe` command).
- **Console unknowns:** TLS trust store, libcurl behaviour in the app's
  sandbox, thread stack size, `getentropy`, file locations.
- **Blocking transfers** in one engine thread (see section 5).
- **Engine conformance:** none found in this run; the shims are minimal by
  design, so new library code may need a few more globals.
- **Signed-in playback** is unmeasured.

## Offline test

`test_host.sh` runs `tube_js selftest` with `TUBE_JS_FIXTURES=test/fixtures`:
16 checks covering the absence of host modules, SHA-1/SHA-256, UTF-8, base64,
randomness, timers, `structuredClone`, URL parsing and parameters, isolated
evaluation, the address transform through youtubei.js' `Player.decipher` with a
synthetic player, the format policy, manifest and caption conversion, the
youtubei.js session with the search parser and `Library` normalisation on a
canned InnerTube answer, and refusal of any request without a canned answer.

## Live requests made by this study

About 72 by `tube_js` (15 catalog and metadata, the rest player answers,
ranged reads of at most 256 KB, one player-script download, one caption) and
about 11 by the yt-dlp cross-check: more than the few dozen planned, because
the first clients tried turned out to be limited and the cause had to be
found. All sequential, no full download; about 9 MB received in total.
