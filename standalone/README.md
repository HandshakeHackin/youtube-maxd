# Embedded companion

ProsperoTube without the PC: the QuickJS-NG engine inside the app runs the
pinned `youtubei.js` 18.1.0 and the companion's own library modules, and a
small listener on `127.0.0.1` answers **the same HTTP API as
`companion/prosperotube/app.py`**. The native provider only needs another
address and token. Media bytes are never relayed: manifests and playlists name
YouTube's servers.

Everything here runs and is tested on a PC. Nothing was run on a console and
nothing is wired into the app yet. Live numbers are from 2026-10-11, Guest, no
cookies.

| Part | Where |
|---|---|
| C interface for the app | `include/tube_service.h` |
| Engine thread, `__host` bindings, service start/stop | `src/tube_engine.c` (725 lines) |
| Loopback HTTP/1.1 listener (own thread) | `src/tube_listener.c` (396) |
| HTTPS client, clock, timers, random, hashes | `ps5/tube_js_http.c`, `ps5/tube_js_platform.c` (shared with the console build, see `ps5/README.md`) |
| Host tool | `src/main.c` |
| The API (routes, jobs, sessions, pairing) | `js/companion.js` |
| Request models of `app.py`/`models.py` | `js/schema.js` |
| Library bounds and error mapping, playback client | `js/core.js` |
| Search/channel/playlist pages, video record | `js/catalog.js` |
| Player answer, format policy, manifest, captions, chapters, storyboards, HLS | `js/resolve.js` |
| Manifests with byte-range segments | `js/segments.js` |
| Thumbnails, DeArrow, SponsorBlock | `js/extras.js` |
| Web-platform shims | `js/host_shims.js`, `js/url_shim.js`, `js/fetch_shim.js`, `js/platform.js`, `js/node_crypto.js` |
| Canned upstream answers, generator | `fixtures/`, `fixtures/make.py` |
| Tests | `test_host.sh`, `test/service_test.py`, `js/selftest.js`, `scripts/test_standalone.sh` |

```
standalone/build_host.sh                 # build/tube_js + build/tube_js.bundle.js
standalone/test_host.sh                  # offline tests, plain and under ASan/UBSan
build/tube_js serve --host 127.0.0.1 --port N --state-dir DIR [--token T] [--sponsorblock]
                    [--sponsorblock-server https://host] [--pairing-code 123456] [--ranged-manifest]
build/tube_js search|info|comments|channel|resolve|probe|ranges|library|version|selftest ...   # one-shot diagnosis
```

`serve` prints `{"listening":"http://127.0.0.1:N"}`; with no `--token` it writes
the generated one to `DIR/token` (mode 0600) for tools, never to the output.
Environment: `TUBE_JS_FIXTURES=<dir>` (answer every upstream request from
canned files), `TUBE_JS_TRACE`, `TUBE_JS_VERBOSE`, `TUBE_JS_STATS`,
`TUBE_JS_CA_BUNDLE`, `TUBE_JS_LOCAL_SESSION`, `TUBE_JS_PLAYER_SCRIPT`,
`TUBE_JS_SHOW_URLS`, `TUBE_JS_MINIFY` (build), `TUBE_JS_SANITIZE=0` (tests).

## What is implemented

Routes, with the Python service's fields, error codes, statuses and bounds:

| Route | Notes |
|---|---|
| `GET /v1/health` | `mode: youtube`; features list below |
| `POST /v1/pair` | kept for compatibility; answers `pair_expired` (410) unless a pairing code was configured; same rate limit |
| `POST /v1/jobs`, `GET`/`DELETE /v1/jobs/{id}` | actions `search`, `catalog`, `details`, `resolve`, `library`; 16 in flight, 80 retained, 120 s, `expired_job` 410, `job_limit` 503; `auth` answers `invalid_job` |
| `POST /v1/library` | every action of `library.mjs`, through the same 8 KiB / 16 pending / 30 s / 1 MiB bounds and error mapping as `library.py`; account-only actions answer `account_required` exactly as today |
| `GET /v1/search`, `/v1/catalog`, `/v1/video/{id}`, `POST /v1/resolve` | direct forms |
| `GET`/`HEAD /media/{session}/{key}` | `dN.mpd` manifest, `hN.m3u8` live playlist, `cN.srt` captions (fetched as WebVTT, converted), `sbL-M.jpg` storyboard sheets (fetched through the engine), `pN` muxed file (302 to YouTube; no test covers it yet) |
| `GET /v1/sponsor/{id}` | hashed-prefix query, categories, `alt`, one-hour cache; off unless enabled |
| `GET /art/dearrow/{key}` + DeArrow titles/thumbnails on pages | as `dearrow.py` |
| `GET /v1/dislikes/{id}` | `{"enabled": false}` |

`features`: `search channels playlists dash captions dearrow locale
thumbnail_source storyboards translated_captions video_details library
content_filters channel_pages kids stream_reminder playlist_order
subscribe_notifications comment_post search_first channel_new_content embedded`.
Left out because they do not exist here: `relay`, `subscriptions_import`,
`device_auth`, `voice`, `dislikes`, `proxy`.

Not in this stage, answered with the existing conventions: `/v1/auth/*`
(`auth_unavailable` 503), `/v1/voice` (`voice_unavailable` 503), `/v1/remote`
(`remote_session` 404), `/v1/subscriptions/import` (404), any profile other
than `guest` (`unknown_profile` 404).

Known differences from the Python answers:

- Every address inside a manifest or playlist is YouTube's, not the service's.
- `paid_content_text` is always empty and `is_short` always false in the
  resolve result (the pinned parser does not expose them from the player
  answer); `chapters` come from timestamps in the description only.
- A codec that is not offered still answers `unsupported_media` (422), as the
  Python selection does; there is no silent fallback to another codec.
- Audio: among equal candidates the original, uncompressed-dynamics track is
  preferred (the Python order leaves that to yt-dlp's listing).
- 422 answers keep FastAPI's `{"detail": [...]}` shape but not pydantic's wording.
- An expired signed caption/storyboard address answers `expired_session` (410)
  instead of being renewed behind the client's back; the provider resolves again.

Companion modules: bundled **unchanged**. One Node global they use was added
as a shim (`Buffer.byteLength`); `node:crypto` is aliased to `js/node_crypto.js`.
`npm test` in `companion/innertube` still passes (194).

## Playback

- Client: `VISIONOS` with a session fetched from YouTube. Its links need no
  signature, no `n` transform and no proof token, and can be read anywhere in
  the file (`ANDROID_VR` and `IOS` links answer 403 some tens of megabytes
  in). yt-dlp 2026.08.19 uses the same client by default.
- The player script is never loaded on this path. Its analysis (10 s, 376 MB
  in this engine) is refused unless the host sets `allow_player_script`; the
  engine's heap is limited to 128 MiB, so a runaway ends as a failed request.
- Format policy: `sessions.py` ported (height and frame-rate limits, codec
  choice with H.264 > VP9 > HEVC > AV1 on ties, SDR only, eight choices, audio
  language, muxed fallback).
- Manifest: by default **exactly the text `dash_manifest()` produces**
  (checked against the Python output), one segment per file, with the direct
  address in `BaseURL` and `SegmentURL@media`.
- Live: YouTube's HLS master is read at resolve time; each choice is a small
  master for one variant with absolute addresses, served from `/media`.
- Link lifetime: links last about six hours and carry the requesting IP
  address; the session lease is the companion's 30 minutes (`expires_in`).

### Seeking, and the optional ranged manifest

FFmpeg's DASH demuxer cannot seek inside a segment (`pb.seekable = 0` in
`dashdec.c`): with one segment per file a jump forward reads everything in
between, and YouTube paces one long request at about twice real time.
Measured on the host: a seek from 0 to 317 s took **152 s** (H.264 1080p60)
and **169 s** (VP9 1080p60). Whether the console player shows the same today
through the relay was not checked here; the cause is in FFmpeg and in the
server, not in the relay.

`ranged_manifest` (`--ranged-manifest`) builds the manifest from each MP4
file's `sidx` on first request (one small ranged read per track): fragments are
grouped on a fixed grid so that FFmpeg's "time divided by segment duration"
lands on or before the target. Same seek: **1.2 s**, nine requests. WebM
tracks (VP9, Opus) keep the one-segment shape: with cluster ranges host FFmpeg
played from the start but a seek did not return within 40 s. So today the
ranged shape helps H.264/AV1 with AAC, not VP9. It is off by default because
only the classic shape is proven on the console.

Two FFmpeg facts found on the way: the `BaseURL` text loses its first `&`
when FFmpeg re-parses it (hence the address repeated in attributes, as the
Python manifest does), and a `SegmentTimeline` inside a `SegmentList` is not
used for seeking.

### Playback proof (host FFmpeg 8.0.1, options as `evo_stream_io.c` sets them)

| Case | Result |
|---|---|
| H.264 1080p60 + AAC, classic manifest, from 0 | 3 s decoded in 0.4 s (184 video, 135 audio frames, 0 errors) |
| same, seek to 317 s | decoded correctly, after 152 s (see above) |
| VP9 1080p60 + AAC, classic manifest, from 0 | 3 s decoded in 0.5 s |
| same, seek to 317 s | decoded correctly, after 169 s |
| H.264 1080p60, ranged manifest, from 0 / seek to 317 s | 0.6 s / 1.2 s |
| VP9, ranged manifest (cluster ranges, not shipped) | start 0.7 s; seek: no result in 40 s |
| Live stream, HLS choice (720p30 H.264 + AAC) | 3 s decoded in 0.9 s |

Headers FFmpeg sent to YouTube: `User-Agent` (its own `Lavf`), `Accept`,
`Range`, `Connection`, `Host`, `Icy-MetaData`. None of them was needed beyond
`Range`; the provider sets no stream headers, so nothing of the service's
token can reach YouTube. The manifest is fetched with one request per
connection; FFmpeg needed neither keep-alive nor ranges on `/media`.
`allowed_extensions=ALL` is required (EVO sets it).

One run met 403 on every fragment after the seek; FFmpeg then asked for 114
fragments in nine seconds before giving up. It did not happen again and a
ranged read of the same file worked minutes later. It came after about half a
gigabyte of test reads in ten minutes, so it may have been a limit; either
way the player must stop, not walk the segment list, when YouTube refuses.

## Live check against `scripts/probe_live_guest.py`

`tube_js serve --sponsorblock`, token from the state directory:

| Check | Result |
|---|---|
| health, pairing (stored token) | pass |
| search job, two pages | pass |
| library search, continuation | pass |
| search filters | pass |
| search suggestions, first playable | pass |
| home | pass |
| sections music, gaming, news, sports, live, movies, kids | pass (7) |
| sections trending, shorts | pass (unavailable upstream) |
| channel tabs, order, search | pass |
| playlist, playback window | pass |
| video details, description, related | pass |
| comments top, newest, replies | pass |
| resolve per codec | pass: avc 1080p60, vp9 2160p60, av1 2160p60, hevc none |
| DASH manifest and ranged media | **not applicable**: the probe refuses to follow an address outside the companion; the FFmpeg proof above covers it |
| loudness, chapters, storyboard, captions | pass (loudness -4.71, storyboard 32 KB JPEG, SubRip read) |
| live HLS and chat | **not applicable**, same reason; covered by the FFmpeg live case |
| locale, content filters, thumbnail source | pass |
| SponsorBlock, DeArrow | pass |

23 of 25; the two others cannot pass by design. (The first run failed the six
sections and the locale check on `Buffer.byteLength`; fixed with the shim.)

## Measured

| | |
|---|---|
| Host program | 1.25 MB (1.14 MB code); bundle 1.72 MB |
| Service resident memory, after the probe and playback runs | 26–29 MB peak |
| Bundle evaluation at start | about 80 ms |
| Search job through the service (create + poll) | 494 ms; repeat within the page cache 3 ms; page 2: 541 ms |
| Resolve job | 106 ms fresh; 200 ms with a new session; 11 ms within the 30 s extraction cache |
| Engine limits in use | 4 MiB thread stack, 3 MiB JS stack limit, 128 MiB heap limit (all tests and live runs fit) |

## Tests

`test_host.sh` (also `scripts/test_standalone.sh`, picked up by
`scripts/test_all.sh`): 20 engine checks and 126 HTTP checks against a running
`tube_js serve` with canned InnerTube answers: health, authorisation,
unknown routes and methods, body/header/connection bounds, 24 parallel
connections, pairing, job lifecycle and cancellation, search pages and
continuation, video/link queries, channel and playlist (with playback
contexts), library actions and account refusals, details, resolve record,
manifest text, HEAD, caption SRT, storyboard sheet, automatic and translated
captions, frame-rate and codec limits, removed video, live playlist,
SponsorBlock, DeArrow title and image, thumbnail source, ranged manifest,
clean shutdown. Everything runs a second time under AddressSanitizer + UBSan
with leak detection; shutdown must leave nothing on stderr.

## What the PS5 integration must provide

```c
#include "tube_service.h"
int  tube_service_start(const char *state_dir, unsigned short *port, char token[65]);
void tube_service_stop(void);
int  tube_service_running(void);
const char *tube_service_error(void);
void tube_service_configure(const tube_service_options *options);   /* optional, before start */
```

- `start` creates the engine thread (4 MiB stack), evaluates the bundle,
  opens the listener (its own thread, 512 KiB stack) and returns; it does not
  touch the network. The provider's source becomes `http://127.0.0.1:<port>`
  and its token the returned one (64 hex digits, new at every start, kept only
  in memory). A zero port picks a free one between 20000 and 39999.
- `stop` (first soft-close frame) interrupts a running script, drops every
  transfer and connection, joins both threads and frees the engine.
- Console build: compile `src/tube_engine.c`, `src/tube_listener.c` with EVO's
  flags next to the `ps5/` units; the bundle and the roots come from
  `ps5/embed_data.py` as `tube_js_bundle` and `tube_js_ca_pem`; sockets use
  `SO_NBIO` there. Settings (SponsorBlock, ranged manifest) go through
  `tube_service_configure`.
- State under `state_dir`: `cache/` only (small youtubei.js entries). No log.
- The provider must not send its stream headers to `/media` links' targets
  (it sends none today) and should treat a 403 from YouTube as "resolve again".

## Open risks

- **One client identity.** If YouTube puts a proof token on `VISIONOS` links
  as it did on two other app clients, playback stops until the bundle is
  updated (HLS from the same answer is the first fallback, already used for
  live). The bundle must be replaceable without a new package.
- **Seeking** with the classic manifest (above); the ranged shape is unproven
  on the console and does not cover WebM.
- **Refused fragments** make FFmpeg walk the list (above).
- **Console unknowns**: everything in `ps5/README.md` section 6, plus the
  listener (loopback bind in the 20000–39999 range, the datagram wake socket,
  `poll` on it) and FFmpeg reading YouTube directly with the app's TLS setup.
- **Accounts** are not in this stage; signed-in playback is unmeasured.
- **No relay** means storyboards and captions still pass through the engine
  (small), and nothing can renew a link behind the player's back.

## Earlier findings kept for reference

Engine: QuickJS-NG `v0.17.0` (commit `6d46d07d04041b40f4f49eaa7fdebe44c314c699`,
MIT), amalgamation SHA-256
`a0955463c74809173a253ff87365095e6972e8b171cfb6aa8a1e781adf35cfeb`; one C11
file, interpreter only, no `std`/`os` modules. youtubei.js modules must be
imported starting from `utils/Utils.js` or the bundle fails at load. Clients
tried live for the player answer: `VISIONOS` (usable), `ANDROID_VR` and `IOS`
(403 deep in the file), `ANDROID` (itag 18 only), `MWEB` (needs the `n`
transform: 22 ms with a cached player script), `TV`/`TV_SIMPLY`/embedded
(refused), `WEB` (SABR only). Sign-in outlook: the TV device-code flow needs
only `fetch`, a UUID, JSON and a credentials file; `device_auth.mjs` is already
in the bundle.

## Live requests made in this stage

About 175 requests to the service for the probe (some 250 upstream API
requests), and the FFmpeg runs: roughly 0.45 GB of media, almost all of it
from the two linear seeks and one run that read every fragment up to the seek
point. That is far more than intended; the cause (the demuxer reading through
to a seek target) was only visible by running it.
