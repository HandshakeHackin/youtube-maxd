# Embedded companion

ProsperoTube without the PC: the QuickJS-NG engine inside the app runs the
pinned `youtubei.js` 18.1.0 and the companion's own library modules, and a
small listener on `127.0.0.1` answers **the same HTTP API as
`companion/prosperotube/app.py`**. The native provider only needs another
address and token. Media bytes are never relayed: manifests and playlists name
YouTube's servers.

Everything here runs and is tested on a PC. The app is wired to it since
(`standalone/ps5/README.md`, section 8), and inside the app it ran on a console
in one bounded Guest session on 2026-10-13 (`docs/HARDWARE_ACCEPTANCE.md`):
start on the loopback address, Discover and one topic, one resolve, and H.264
1080p with AAC played from YouTube directly, with a seek and a pause. Other
codecs and 4K, a link expiring during playback and accounts are not yet
verified on a console. Live numbers are from 2026-10-11, Guest, no
cookies. The subscriptions import and the phone receiver (2026-10-13) have run
offline only.

| Part | Where |
|---|---|
| C interface for the app | `include/tube_service.h` |
| Engine thread, `__host` bindings, service start/stop | `src/tube_engine.c` (725 lines) |
| Loopback HTTP/1.1 listener (own thread) | `src/tube_listener.c` (396) |
| HTTPS client, clock, timers, random, hashes | `ps5/tube_js_http.c`, `ps5/tube_js_platform.c` (shared with the console build, see `ps5/README.md`) |
| Host tool | `src/main.c` |
| The API (routes, jobs, sessions, pairing) | `js/companion.js` |
| Accounts: credential file, device-code sign-in, one-attempt guards | `js/accounts.js` |
| Signed-in playback (off) | `js/account_playback.js` |
| Request models of `app.py`/`models.py` | `js/schema.js` |
| Library bounds and error mapping, playback client | `js/core.js` |
| Search/channel/playlist pages, video record | `js/catalog.js` |
| Player answer, format policy, manifest, captions, chapters, storyboards, HLS | `js/resolve.js` |
| Manifests with byte-range segments | `js/segments.js` |
| Thumbnails, DeArrow, SponsorBlock | `js/extras.js` |
| Subscriptions import (NewPipe, PocketTube, GrayJay exports) | `js/subscriptions_import.js` |
| Phone receiver (YouTube Lounge, "Link with TV code") | `js/lounge.js` |
| Web-platform shims | `js/host_shims.js`, `js/url_shim.js`, `js/fetch_shim.js`, `js/platform.js`, `js/node_crypto.js` |
| Canned upstream answers, generator | `fixtures/`, `fixtures/make.py` |
| Tests | `test_host.sh`, `test/service_test.py`, `test/account_test.py`, `test/import_test.py`, `test/remote_test.py`, `js/selftest.js`, `scripts/test_standalone.sh` |

```
standalone/build_host.sh                 # build/tube_js + build/tube_js.bundle.js
standalone/test_host.sh                  # offline tests, plain and under ASan/UBSan
build/tube_js serve --host 127.0.0.1 --port N --state-dir DIR [--token T] [--sponsorblock]
                    [--sponsorblock-server https://host] [--return-youtube-dislike]
                    [--pairing-code 123456] [--ranged-manifest] [--account-playback]
                    [--lounge-base http://127.0.0.1:N/api/lounge]   # tests only: a stand-in for YouTube's phone service
build/tube_js search|info|comments|channel|resolve|probe|ranges|library|version|selftest ...   # one-shot diagnosis
```

`serve` prints `{"listening":"http://127.0.0.1:N"}`; with no `--token` it writes
the generated one to `DIR/token` (mode 0600) for tools, never to the output.
Environment: `TUBE_JS_FIXTURES=<dir>` (answer every upstream request from
canned files), `TUBE_JS_TRACE`, `TUBE_JS_VERBOSE`, `TUBE_JS_STATS`,
`TUBE_JS_CA_BUNDLE`, `TUBE_JS_LOCAL_SESSION`, `TUBE_JS_PLAYER_SCRIPT`,
`TUBE_JS_SHOW_URLS`, `TUBE_JS_MINIFY` (build), `TUBE_JS_SANITIZE=0` (tests),
`TUBE_JS_AUTH_CLOCK` and `TUBE_JS_REMOTE_CLOCK` (tests: the sign-in clock with
canned answers, the receiver's lease clock with `--lounge-base`).

## What is implemented

Routes, with the Python service's fields, error codes, statuses and bounds:

| Route | Notes |
|---|---|
| `GET /v1/health` | `mode: youtube`; features list below; `account_playback` (false unless the host switched it on) |
| `POST /v1/auth/start`, `/status`, `/cancel` | device-code sign-in, as `device_auth.py`; see Accounts |
| `POST /v1/pair` | kept for compatibility; answers `pair_expired` (410) unless a pairing code was configured; same rate limit |
| `POST /v1/jobs`, `GET`/`DELETE /v1/jobs/{id}` | actions `search`, `catalog`, `details`, `resolve`, `library`; 16 in flight, 80 retained, 120 s, `expired_job` 410, `job_limit` 503; `auth` runs a sign-in step |
| `POST /v1/library` | every action of `library.mjs`, through the same 8 KiB / 16 pending / 30 s / 1 MiB bounds and error mapping as `library.py`; account actions need an account profile (`account_required` for Guest) and run behind the one-attempt guards |
| `GET /v1/search`, `/v1/catalog`, `/v1/video/{id}`, `POST /v1/resolve` | direct forms |
| `GET`/`HEAD /media/{session}/{key}` | `dN.mpd` manifest, `hN.m3u8` live playlist, `cN.srt` captions (fetched as WebVTT, converted), `sbL-M.jpg` storyboard sheets (fetched through the engine), `pN` muxed file (302 to YouTube; no test covers it yet) |
| `GET /v1/sponsor/{id}` | hashed-prefix query, categories, `alt`, one-hour cache; off unless enabled. `server=https://host` names the alternative server for this request (see below) |
| `GET /art/dearrow/{key}` + DeArrow titles/thumbnails on pages | as `dearrow.py` |
| `GET /v1/dislikes/{id}` | the Return YouTube Dislike relay, as `dislikes.py`; `{"enabled": false}` and nothing sent unless enabled (see below) |
| `POST /v1/subscriptions/import` | the export file itself as the body, up to 1 MiB on this route alone; answers `{format, channels[{channel_id, title, group?}], skipped, truncated}` as `subscriptions_import.py` |
| `POST /v1/remote` | the phone receiver, as `remote_lounge.py`: `start`, `pair`, `poll`, `update`, `stop`; see Phone receiver |

`features`: `search channels playlists dash captions dearrow locale
thumbnail_source storyboards translated_captions video_details library
content_filters channel_pages kids stream_reminder playlist_order
subscribe_notifications comment_post search_first channel_new_content device_auth
subscriptions_import remote sponsorblock_server embedded`. `remote` is this
service's name for the phone receiver (the Python service has the route
without listing it). `sponsorblock_server` says that a segments request may
name the alternative server. `dislikes` is listed only while the relay is
enabled. Left out because they do not exist here: `relay`, `voice`, `proxy`.

**Dislikes.** `js/extras.js` (`Dislikes`, `votes`) is `dislikes.py` ported:
off unless the host enables it (`--return-youtube-dislike`,
`tube_service_options.return_youtube_dislike`); enabled, one
`GET https://returnyoutubedislikeapi.com/votes?videoId=<id>` without any
credential, five seconds and 64 KiB at most, and only an object for that video
whose `likes` and `dislikes` are non-negative integers up to 10^12, written as
integers, is accepted. A count is kept for an hour and "not available" for
five minutes, 128 videos at most; a failure is never an error. That service
learns which video is asked about, so the app enables the relay at start and
asks only for a profile whose **Show dislikes** switch is on, once per opened
details or description page (`native/tube_provider.c`, `detail_finish`): the
switch can change while the service runs, so it cannot be a start option, and
with the switch off no request reaches this route. The option keeps the relay
off for every other embedder and for the host tool, as it is in the companion.

**Alternative SponsorBlock server.** The companion's host starts it with
`--sponsorblock-server`; that option exists here too. The app has no host to
choose a server, so the address the user typed (SponsorBlock actions >
Alternative server, kept in `youtube.json` as `sponsorblock_server`) comes with
the request as `server=`, and a change needs no restart. It is used only
together with `alt=true`, passes the same rule as the option (an `https://`
origin: a plain host name, no port, path, query or user; `invalid_server` 400
otherwise) and is cached apart from the default server's answers.

Not offered, answered with the existing conventions: `/v1/voice`
(`voice_unavailable` 503; `docs/VOICE_SEARCH.md` has the assessment), a profile
that is neither `guest` nor a signed-in account (`unknown_profile` 404).

Known differences from the Python answers:

- Every address inside a manifest or playlist is YouTube's, not the service's.
- `is_short` comes from the player answer's microformat when there is one; the
  client used here sends none, so the shape decides (every picture portrait, at
  most three minutes, not live). `paid_content_text` is read from the raw player
  answer (`paidContentOverlay`). `chapters` come from the watch page's structured
  list (the uploader's, else YouTube's automatic one) and only then from
  timestamps in the description; this costs one more request (`next`) per resolve.
- A codec that is not offered still answers `unsupported_media` (422), as the
  Python selection does; there is no silent fallback to another codec.
- Audio: among equal candidates the original, uncompressed-dynamics track is
  preferred (the Python order leaves that to yt-dlp's listing).
- 422 answers keep FastAPI's `{"detail": [...]}` shape but not pydantic's wording.
- An expired signed caption/storyboard address answers `expired_session` (410)
  instead of being renewed behind the client's back; the provider resolves again.
- Subscriptions import: a body over its limit is refused by the listener (413)
  before the bearer is looked at; PocketTube groups whose names are whole
  numbers are read before the others (JavaScript's key order), which can
  change the order of the channels, not the set; a number written as `NaN` in
  the file is not JSON here.
- Phone receiver: a start that names no microphone and one that does are the
  same receiver (the capability is always off); a request that the client
  abandons is not cancelled upstream, it ends by its own time limit.

## Subscriptions import

`js/subscriptions_import.js` is `subscriptions_import.py` ported: strict UTF-8
with one optional byte-order mark, JSON, then the first reader that fits
(NewPipe's `subscriptions` list with service 0, GrayJay's list of addresses or
records, PocketTube's groups without its `ysc_` keys), channels recognised only
by a channel ID or a `youtube.com/channel/UC…` address, duplicates dropped,
2,048 channels at most, titles and group names bounded. Nothing is stored and
no account is touched.

The listener's request-body limit stays 8 KiB. `POST /v1/subscriptions/import`
alone may carry 1 MiB (`TUBE_LISTENER_LARGE_BODY_LIMIT`): the rest of such a
body is received into one heap block that is handed to the engine without
another copy, one such request at a time (a second is answered 503), and a
request that is not complete 15 seconds after it connected is dropped, as any other.

## Phone receiver

`js/lounge.js` is `remote_lounge.py` ported: the same five actions on
`POST /v1/remote`, the same typed command list, queue windows, expansion
through `remote_playlist`, lease, bounds and error codes. Nothing is contacted
before a `start`. `docs/REMOTE_SERVICE.md` (Built-in receiver) describes the
contract and what differs; here is what the service had to gain for it.

- **Streaming answers.** YouTube's back channel is one long chunked response
  that carries frames as they happen, and the HTTPS client delivered whole
  bodies. `tube_js_http_request.stream` makes a request a streaming one: its
  pieces are handed over after each `tube_js_http_poll()` by
  `tube_js_http_take_chunk()` (id, status, bytes), its `timeout_ms` bounds each
  silence instead of the whole request, `max_response_bytes` bounds what waits
  to be taken, and it completes, fails or is cancelled like any other. The
  engine passes the pieces to `__tubeHttpChunk(id, status, bytes)` before the
  completion; in JavaScript, `fetch(url, { onChunk, idle_ms })` calls
  `onChunk(bytes, status)` for each piece and resolves with an empty body when
  the answer ends; what `onChunk` throws ends the transfer. The same C runs on
  the host and the console (`ps5/tube_js_http.c`, libcurl multi).
- **Framing.** A strict incremental UTF-8 decoder (a character split between
  two pieces waits for its rest), then frames of a decimal length, a newline
  and that many UTF-16 code units of JSON.
- **Connections.** The client's connection limit went from six to ten, so
  that four receivers, each holding one connection open, leave the six there
  were.
- **Tests only:** `--lounge-base` (`tube_service_options.lounge_base`) points
  the receiver at a stand-in on `http://127.0.0.1` and is the one case in
  which the engine starts a request that is not HTTPS. The app never sets it.

## Accounts

Tested offline only; no real account has signed in. Decisions and their
reasons: `docs/DECISIONS.md` (2026-10-13).

- **Sign-in** (`DeviceAuth` in `js/accounts.js`, a port of `device_auth.py`
  over `companion/innertube/device_auth.mjs`): `start` reads the public TV
  client identity from YouTube's TV page and asks for one device code; every
  `status` performs at most one token poll and only after the interval YouTube
  asked for (five seconds at least, five more after `slow_down`); one active
  session, sixteen retained for an hour, no timer. States: `pending`,
  `authorized`, `denied`, `expired`, `canceled`, `failed`. An uncertain
  redemption is never tried again. The device code, the client identity and
  the tokens never appear in an answer.
- **Storage** (`OAuthProfiles`): `<state dir>/accounts/oauth-profiles.json`,
  the companion's format (`{"version": 1, "profiles": [{id, name, cookie: "",
  oauth}]}`), eight accounts at most, 0600 in a 0700 directory, written to a
  temporary file, flushed, renamed, then read back. Plain text: the console
  has no encrypted storage. A file that cannot be understood is left as it is
  and accounts answer `auth_storage` (507) until it is removed.
- **Sessions:** one YouTube.js session per profile and content locale, on the
  TV client with the account's token (`createProfileClient`). The token is
  refreshed through the engine's HTTPS when it ran out and the new one is
  stored. Adding or removing an account rebuilds the library object, which
  retires every client, cursor and retained feed.
- **Guards** (`Guards`, the ones `library.py` kept outside the worker): chat
  messages, comment ratings, mark-as-watched, feedback, playlist save and
  history controls, reminders, playlist order and comment posts are sent once
  per confirmation; history reports stop for a session after an unconfirmed
  one. In memory for thirty minutes, not on disk.
- **Removal** (`profile_remove`): the file first; then the account's clients,
  cursors, guards, playback sessions, catalog pages and jobs. Nothing is
  changed or revoked at Google.
- **Playback:** every profile resolves as Guest (`VISIONOS`, no credentials).
  `account_playback` (a `tube_service_options` field, `--account-playback` on
  the host, off) lets an account profile ask its own TV session for a video
  the Guest path answered "sign in" for; without `allow_player_script` only
  answers with direct links are usable. Unverified.
- **Catalog jobs** of an account profile: search and channel pages with the
  Guest session, playlists with the account's.
- **Known gap found offline:** the canned account lists are WEB-shaped, as the
  pinned parsers expect; what the TV client returns to a signed-in session is
  not known here.

Companion modules: bundled unchanged except `library.mjs`, whose
`playlist_delete` now posts `playlist/delete` itself (the pinned library's
method throws before sending). One Node global they use was added
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

### Seeking

The console does not use stock FFmpeg. `scripts/patch_ffmpeg_dash_seek.py`
changes two places in `dashdec.c` (7.1.1): a complete single-file VOD
representation (one fragment from offset 0, no separate initialisation
section, seekable input) keeps its inner I/O **seekable**, and a seek on it
becomes a seek of the underlying HTTP input (a new ranged request), with the
segment offset kept in step. The container's own index (`sidx`/`moof` in MP4,
Cues in WebM) then takes the demuxer straight to the target. Without the patch
the inner I/O is not seekable and a jump forward reads everything in between,
which YouTube paces at about twice real time: that was the 152 s / 169 s
measured in the previous stage with stock FFmpeg 8.0.1. It does not describe
the console.

`test/seek_lab.py` reproduces the console's demuxer on the host (FFmpeg 7.1.1
built with and without the patch under `~/.cache/prosperotube/ffmpeg-host/`)
against a local paced server with ten-minute H.264/AAC (fragmented MP4, one
`sidx`) and VP9/Opus (WebM, Cues in front) files; three seconds copied after
each seek, 25 s limit:

| FFmpeg 7.1.1 | File | Manifest | 0 s | 300 s | 580 s | Media read for a seek |
|---|---|---|---|---|---|---|
| patched | MP4 | classic (Python shape) | ok | ok | ok | 2.3 MiB, 8 requests |
| patched | WebM | classic | ok | ok | ok | 1.6 MiB, 4 requests |
| patched | MP4 | ranged (`sidx`) | ok | ok | ok | 0.9 MiB, 7 requests |
| patched | WebM | ranged (Cues) | ok | **no result in 25 s** | **no result** | 0.6 MiB |
| stock | MP4 / WebM | classic | ok | **read-through, no result in 25 s** | **same** | 7 MiB and counting |
| stock | MP4 | ranged (`sidx`) | ok | ok | ok | 0.9 MiB |
| stock | WebM | ranged (Cues) | ok | **no result** | **no result** | 0.6 MiB |

Every "ok" finished in well under a second locally. Live, once each, patched
FFmpeg, classic manifest, 1080p60: H.264 start 1.4 s, middle 4.9 s, near the
end 0.6 s; VP9 0.6 s / 5.6 s / 0.7 s. The two middle seeks each sat out one
5-second read time-out (`rw_timeout`) before succeeding; nothing was read
through. Why that one position waits was not pursued within this round's
traffic limit.

**Default: the classic manifest.** With the console's demuxer it seeks by byte
range in both containers, needs no index read, and is the shape already proven
there. `ranged_manifest` stays as an option for an unpatched FFmpeg and covers
MP4 tracks only: FFmpeg (7.1.1 and 8.0.1) plays WebM cluster ranges from the
start but never returns from a seek into them, so the service leaves WebM
tracks in the one-segment shape (`cueSegments` is kept for the laboratory).

Two FFmpeg facts found on the way: the `BaseURL` text loses its first `&`
when FFmpeg re-parses it (hence the address repeated in attributes, as the
Python manifest does), and a `SegmentTimeline` inside a `SegmentList` is not
used for seeking.

### Refused and expired links

What FFmpeg does when the media server answers 403 (`seek_lab.py --only
refuse`, patched and stock alike):

| Manifest | Refused from the first request | Refused after six requests |
|---|---|---|
| classic | open fails at once, 1 request | 1 refused request, then the input ends |
| ranged, 240 segments | open fails at once, 1 request | **119 refused requests in 0.1 s**: it walks the rest of the list |
| ranged, 30 s segments (41) | open fails at once | 20 refused requests |

So the classic shape is also the safe one: one refused request per event.

- **Links refused when new.** Seen live: whole sessions whose every read by
  FFmpeg was 403 while the service's own read succeeded. The likely cause is
  the address family: YouTube binds links to the address that asked for them,
  libcurl may ask over IPv6, FFmpeg connects over IPv4. The service's HTTPS
  client is now IPv4-only (`ipv4_only` in `ps5/tube_js_http.h`, a small
  addition to the shared layer that the other branch needs too); the run after
  that change passed six of six. Independently, resolve now reads one kilobyte
  of the first video file and asks the client again (three tries, short
  pauses) before answering `upstream_media` (502).
- **Links that expire or are refused during playback** (about six hours, or a
  changed network address). The 403 arrives in FFmpeg, not in the service. The
  app already has the path for it: `evo_demux_input_failed()` in the player
  screen calls `tube_queue_recovery()` (three attempts), which resolves again
  and reopens at the saved position (`scripts/playback_recovery_overlay.py`).
  That works unchanged, because a new resolve returns new links. Two things the
  app must make sure of: that a read ended by 403 counts as an input failure
  and not as the natural end (FFmpeg's command line ended "successfully" in
  the lab), and that the recovery resolve carries `fresh: true`.
- **Service side, done:** `GET /media/<session>/<manifest>?refresh=1` rebuilds
  the session's manifests, playlists, caption and storyboard sources from a new
  player answer under the same names, and any manifest request does so on its
  own once the links are within ten minutes of their end. An app that prefers
  not to run a whole resolve can reopen the same address with `?refresh=1`.

### Playback proof

Stock host FFmpeg 8.0.1 (previous stage): H.264 and VP9 1080p60 with AAC from
the start in 0.4–0.5 s; live HLS 720p in 0.9 s. Patched 7.1.1: the tables above.

Headers FFmpeg sent to YouTube: `User-Agent` (its own `Lavf`), `Accept`,
`Range`, `Connection`, `Host`, `Icy-MetaData`. None of them was needed beyond
`Range`; the provider sets no stream headers, so nothing of the service's
token can reach YouTube. The manifest is fetched with one request per
connection; FFmpeg needed neither keep-alive nor ranges on `/media`.
`allowed_extensions=ALL` is required (EVO sets it).

## Live check against `scripts/probe_live_guest.py`

`tube_js serve --sponsorblock`, token from the state directory:

| Check | Result |
|---|---|
| health (now with `embedded: true`), pairing (stored token) | pass |
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
| DASH manifest and ranged media | pass: manifest and 64 KiB of each track, read from YouTube directly |
| loudness, chapters, storyboard, captions | pass (loudness -4.71, storyboard 32 KB JPEG, SubRip read) |
| live HLS and chat | pass: 1080p30 H.264 + AAC, 6 segments, chat 24 messages |
| locale, content filters, thumbnail source | pass |
| SponsorBlock, DeArrow | pass |

The probe now follows `*.googlevideo.com` addresses when health says
`embedded` (the strict check stays for the Python companion). The first 23
checks passed in the full run of the previous stage; the last two were run on
their own afterwards (4 of 4 with health and pairing), because a second full
run would not fit this round's request limit. So: 25 of 25, over two runs.

## Measured

| | |
|---|---|
| Host program | 1.25 MB (1.14 MB code); bundle 1.72 MB |
| Service resident memory, after the probe and playback runs | 26–29 MB peak |
| Bundle evaluation at start | about 80 ms |
| Search job through the service (create + poll) | 494 ms; repeat within the page cache 3 ms; page 2: 541 ms |
| Resolve | 0.85–0.95 s fresh with a new session (player answer, watch-next for chapters, one-kilobyte link check); 10 ms within the 30 s extraction cache. Before chapters and the check: 106–200 ms |
| Engine limits in use | 4 MiB thread stack, 3 MiB JS stack limit, 128 MiB heap limit (all tests and live runs fit) |

## Tests

`test_host.sh` (also `scripts/test_standalone.sh`, picked up by
`scripts/test_all.sh`): 20 engine checks, 131 HTTP checks, 201 account
checks (`test/account_test.py`: sign-in, storage, refresh, lists, changes and
their guards, history reports, removal, the playback switch), 51 import checks
(`test/import_test.py`: the three formats, addresses, refusals, the one-mebibyte
route) and 272 phone-receiver checks (`test/remote_test.py`: a stand-in for
YouTube's phone service on 127.0.0.1 with a back channel that stays open and
arrives in small pieces; registration, TV code, commands and acknowledgments,
state reports, reconnects, queue windows and expansion, every refusal and
upstream failure, identity, capacity and the lease) against a running
`tube_js serve` with canned InnerTube answers: health, authorisation,
unknown routes and methods, body/header/connection bounds, 24 parallel
connections, pairing, job lifecycle and cancellation, search pages and
continuation, video/link queries, channel and playlist (with playback
contexts), library actions and account refusals, details, resolve record,
manifest text, HEAD, caption SRT, storyboard sheet, automatic and translated
captions, frame-rate and codec limits, removed video, live playlist,
SponsorBlock and a server named by the request, the dislike relay (off, on,
refused answers), DeArrow title and image, thumbnail source, ranged manifest,
structured chapters, a Short with a paid-promotion notice, manifest refresh,
refused links, clean shutdown. Everything runs a second time under AddressSanitizer + UBSan
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
  `SO_NBIO` there. Settings (SponsorBlock, the dislike relay, ranged manifest)
  go through `tube_service_configure`.
- State under `state_dir`: `cache/` (small youtubei.js entries) and, once an
  account signed in, `accounts/oauth-profiles.json` (the tokens; keep it out
  of logs, uploads and backups: the app's own Backup and restore copies
  `youtube.json` alone, says so on its screen, and a restore neither reads nor
  replaces this file nor restarts the service; `docs/DECISIONS.md`, "Backups
  do not hold sign-ins"). No log. The phone receiver stores nothing:
  its identity is the provider's (`youtube.json`), passed with each start.
- The provider must not send its stream headers to `/media` links' targets
  (it sends none today) and should treat a 403 from YouTube as "resolve again".

## Open risks

- **One client identity.** If YouTube puts a proof token on `VISIONOS` links
  as it did on two other app clients, playback stops until the bundle is
  updated (HLS from the same answer is the first fallback, already used for
  live). The bundle must be replaceable without a new package.
- **Seeking** rests on the repository's DASH patch being in the console's
  FFmpeg; live, a seek to the middle waited one 5 s time-out (above).
- **Address family**: the IPv4-only fix is supported by one passing run after
  several failing sessions, not by a controlled comparison.
- **Console unknowns**: the 2026-10-13 session showed the listener bound on
  the loopback address (port 37841), requests answered through it and FFmpeg
  reading one H.264 video from YouTube directly with the app's TLS setup.
  Still open: what `ps5/README.md` section 6 lists and one short Guest session
  does not exercise (memory beside a 4K software decode, shutdown with
  requests in flight, a connection held open for minutes), other codecs and a
  link expiring during playback.
- **Accounts** have met canned answers only: the sign-in from this engine,
  the TV client's real list shapes and every change are unproven, and
  signed-in playback is off and unmeasured.
- **No relay** means storyboards and captions still pass through the engine
  (small), and nothing can renew a link behind the player's back.
- **The phone receiver** has met a stand-in only. Unobserved from this engine:
  whether YouTube's phone service accepts it as it accepted the Python
  receiver, how often keep-alive frames come against the 65-second silence
  limit, how long one long poll lasts, and long-lived streaming through the
  console's libcurl and its sliced wait. Closing the app does not end the
  channel upstream.

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

## Live requests

Previous stage: about 250 upstream API requests and roughly 0.45 GB of media
(two read-through seeks with stock FFmpeg, which the console does not use).
This stage: about 65 API requests and about 40 MB of media; every FFmpeg run
copied three seconds under a 20 s limit, and all seek and refusal measurements
that did not need YouTube ran against the local server.
