# Build and offline validation

Use Linux/WSL. Keep the native cache on its Linux filesystem; scripts default
to `$HOME/.cache/prosperotube`. Sources may be on `/mnt/c`.

```sh
git submodule update --init --recursive
sudo apt-get install clang-18 lld-18 llvm-18 cmake ninja-build make git \
  python3 python3-venv pkg-config ccache wget curl unzip zip nasm yasm \
  libssl-dev libfreetype-dev libzstd-dev libbz2-dev liblzma-dev ffmpeg
export PATH=/usr/lib/llvm-18/bin:$PATH
bash scripts/bootstrap_sdk.sh
bash scripts/build_ps5.sh --ffpfsc
```

LLVM 18.1.8 was used. Package names vary by distribution. The bootstrap fetches
release assets into an isolated user prefix. `PS5_PAYLOAD_SDK` selects an existing
SDK, but the build installs libraries into it; use a dedicated copy.
`PROSPEROTUBE_NATIVE_WORK` selects a dedicated generated work tree.

Pins: EVO `9715f6ee9bf9b53461d6fdf9a2e11b4c48357783`, UI
`4bd942579dd981b3df9c740438489ca6a614ddc0`, SDK v0.42, Pacbrew v0.39,
FFmpeg 7.1.1 minimal, dav1d 1.5.1, cJSON 1.7.19, MkPFS
`6cb8313dfe0c988ac52617794553f343243d3a56`. Upstream build scripts check
source archive checksums. SDK/Pacbrew assets are release-tag pinned, not
independently checksum-pinned by this wrapper.

`prepare_native.py` checks the EVO commit, extracts pinned sources, restores
every overlaid file before applying checked patches, and copies the original
provider/UI and complete cJSON. It never edits the submodule checkout.
`build_ps5.sh` uses EVO's native packaging pipeline, builds PS5 FFmpeg,
signs/inspects containers and copies artifacts to `dist/`. MkPFS verifies its
image. Network access is needed only for uncached dependencies. Scripts never
deploy or launch on PS5.

The pinned EVO app pipeline also includes its PS5 Videodec2 hardware backend;
the secondary high-resolution decoder is enabled by its packaging recipe.
FFmpeg supplies demuxing and the software fallback. Presence in a binary is
not hardware acceptance: the selected decoder, presentation, sound and timing
must be checked on the console. The current companion format policy selects
SDR AVC/VP9 and has not yet enabled the backend's wider HDR/HEVC capabilities.
See [reference candidates](REFERENCE_CANDIDATES.md) for source evidence.

Set `PROSPEROTUBE_PS5_DIST` to an isolated candidate directory when checking
new changes, preserving existing release files and their recorded manifests.
`PROSPEROTUBE_NATIVE_WORK` separately isolates the generated build tree.
These options do not deploy a candidate or change the console target.
Optional account library setup is documented in [library service](LIBRARY_SERVICE.md).

For bounded console acceptance, `build_ps5.sh --usb-remote` enables EVO's
existing development command/status/UI instrumentation. The overlay scopes its
files to `dev_command`, `dev_status` and `dev_ui.json`, and the diagnostic log
to `console.log`, under `/data/prosperotube`. Startup requests filesystem access
through Lapy and proves writable app storage before initializing services.
The packaged one-shot helper targets only `PPSA99142`; a resident Lapy service
gets the first bounded opportunity. Failed elevation stops startup rather than
loading or saving transient state. Lapy's handshake markers still use
`/download0`, as required by its protocol. The normal build leaves
development commands disabled. Use the title-aware protocol tooling for
deployment/launch/close; upstream EVO helper scripts still target EVO's own
title and shared USB filenames and must not be invoked against ProsperoTube.
Add EVO's `--virtual-keyboard` option for scripted app text-entry cases; the
development input commands cannot type into the PS5 system IME.

For a bounded Lapy diagnostic candidate, set
`PROSPEROTUBE_LAPY_RESULT_FILE=/data/prosperotube/lapy-helper.log` when running
`build_ps5.sh`. This selects the pinned upstream helper's file-mirror option;
the diagnostic installer must create `/data/prosperotube` before launch and
record that preparation. The helper refuses to start if it cannot open this
file. Normal builds omit the variable and do not require an existing directory.
The helper manifest records the exact source revision and optional result path.
No shared ps5log configuration is changed. See `native/elevation/README.md` for
the PR #50 revision and hardware acceptance limits.

`package_release.py` packages companion/documentation bytes from the committed
Git revision and only named generated media fixtures. Local account files,
dependency directories and generated library bundles are excluded. Install the
pinned library dependencies and rebuild its bundle on the companion host.
Packaging records artifact hashes; test evidence belongs in the validation
record and is not inferred from running the packager.

## Checks

```sh
python3 -m venv .venv
.venv/bin/python -m pip install -r companion/requirements.lock
PYTHONPATH=companion .venv/bin/python -m pytest -q
bash scripts/test_native.sh
bash scripts/test_recovery_handoff.sh
bash scripts/test_elevation.sh
bash scripts/test_subtitles.sh
python3 scripts/generate_fixtures.py
PYTHONPATH=companion .venv/bin/python scripts/probe_companion.py
bash scripts/preview_native.sh
```

The native provider runs with AddressSanitizer/UndefinedBehaviorSanitizer and
mocked HTTP. Tests cover isolation, persistence, stable identities, pairing,
jobs/cancellation, origin checks, chapters and SponsorBlock.
The media probe runs the real HTTP API/relay against generated AVC/AAC, VP9/Opus
and HLS, then decodes and seeks with host FFmpeg and checks caption timing.
This tests host FFmpeg, not the PS5 build. The UI preview builds pinned RmlUi 6.2
and renders the shipping interface with EVO's CPU renderer, checking card bounds,
focus and selection. PNGs: `output/preview`; media evidence: `output/validation`.
Fixtures and outputs are generated, not tracked.

`bash scripts/test_device_auth_native.sh` exercises the shipping native sign-in
state machine with sanitizers, a deterministic clock and queued HTTP responses.
It checks initiation, poll intervals/single-flight, cancellation, late original-
source cleanup, expiry/denial, malformed responses and exclusion from PS5 state.
The host UI preview includes the device-code screen and cancel-row activation.

To characterize the public TV player without an account, media download or console
traffic, run `node companion/innertube/probe_tv_player.mjs PUBLIC_VIDEO_ID OUTPUT.json`
with the configured Node runtime. The runner bundles the pinned dependency into
an owned temporary directory, bounds the child to 90 seconds, and cleans up.
Reports contain only status/counts/booleans, never signed URLs or token material.
An unauthenticated LOGIN_REQUIRED response is useful diagnostic evidence, not a
successful playback case. Full account validation requires real authorization.

## Public YouTube probe

For an explicit public YouTube check on the companion host:

```sh
PYTHONPATH=companion .venv/bin/python scripts/probe_youtube.py \
  --video PUBLIC_VIDEO_ID --codec avc --height 720 \
  --js-runtime node --js-runtime-path /path/to/node
```

The probe uses no account cookies and does not contact the PS5. It starts a
temporary loopback companion, pairs through the real API, resolves the video,
probes its relayed streams, decodes two seconds of audio/video, and repeats after
a short seek. Select `--codec vp9` to check that path. Only allowlisted result
metadata is saved; media URLs, pairing tokens and raw decoder messages are not.
Success is host companion/FFmpeg evidence, not console decoder or audio proof.
For the VOD renewal path, add `--transport dash --simulate-vod-expiry`. After a
real successful ffprobe, this injects one local 403 for a selected video track.
The runner then requires exactly one fresh resolution plus actual audio/video
decode and seek through the same relay URLs. The server itself is never changed.
Reports label the injected expiry; this is not evidence of a naturally expired
six-hour link, long-session playback or PS5 recovery. The source must return
verifiable representation metadata, or this case fails explicitly.
The selected runtime and installed `yt-dlp-ejs` are required for full YouTube
extraction; see upstream [EJS setup](https://github.com/yt-dlp/yt-dlp/wiki/EJS).

Add `--transport hls --expect-live` for a currently active public live-stream
check. The runner requires an HLS choice flagged live, then probes and decodes
at its live edge. Its result reports `live_edge_decode`; it does not perform or
claim a VOD seek for live media. DVR seek, long-duration playback, reconnect and
PS5 decoding/audio remain separate checks.

Add `--simulate-hls-expiry` to that live HLS case to inject one local 403 for
the selected video playlist root after a real ffprobe. The probe requires one
fresh resolution and audio/video live-edge decode through the unchanged local
manifest. The result labels `live_hls_root` and reports injection/resolution
counts. This proves host root renewal with real media, not naturally expired
links, child-resource/DVR renewal, long-session continuity or PS5 playback.

The companion retains complete downloaded HLS resources in a session-owned RAM
buffer: 64 MiB across all sessions, at most 256 objects and 8 MiB per object.
At most four downloads stage copies concurrently. Partial, encoded, oversized
and interrupted downloads are excluded. Cached GET byte ranges and HEAD metadata
use the exact retained bytes; root playlists remain fresh. Session expiry,
resource retirement and account removal invalidate buffered access. No disk
files or account credentials are added.

After generating fixtures, verify buffered seeking with host FFmpeg:

```sh
PYTHONPATH=companion .venv/bin/python scripts/probe_hls_buffer.py \
  --result .deps/hls-buffer-decode.json
```

The probe decodes eight seconds, makes synthetic segment origins reject access,
then performs an input seek to three seconds and requires two seconds of
audio/video decode without another origin segment request. Decoded frame/audio
fingerprints must match a local seek reference and differ from playback at the
beginning. Clearing the buffer must make the expired-origin request fail.
This exercises the shipping HTTP relay;
it does not establish real YouTube DVR or PS5 behavior. Never-downloaded or evicted
expired objects still require renewal/restart handling.

Check the native seek handoff with `bash scripts/test_seek_handoff.sh`. It
extracts the shipping UI method and demux request into an ASan/UBSan regression.
Seek requests retain the previous playback position until the demuxer succeeds;
only the demuxer announces seek begin/end. Refused, queued, fast-completed,
clamped, inactive and invalid targets are covered. Nonfinite targets and targets
beyond one million seconds are rejected before pausing or publishing a request.

With generated fixtures and host FFmpeg development libraries installed, run
`bash scripts/test_seek_timeline.sh`. It exercises real HLS `av_seek_frame`
and packet reads using the shipping timestamp helper under ASan/UBSan. The
helper adds the common container origin for demux/video discard timestamps;
audio packet comparisons subtract that same origin. Boundary tests cover
distinct stream starts, negative and missing origins, fractional positions,
invalid time bases and overflow. This is host evidence, not PS5 acceptance.
The handoff script also extracts the actual initial-resume block and audio
re-anchor function, exercises real HLS seeking, and checks relative clock
anchoring, refused seeks and invalid/overflow conversion without a seek call.
It likewise requires host FFmpeg development libraries and generated fixtures.
The same case checks video-only resume with real host AVC decoding and the
extracted shipping video-clock callback. Only a valid frame outside seek
discard can anchor it; A/V playback retains audio ownership.

For expiry during an ongoing live decode, use `--decode-seconds 60
--hls-expiry-after-requests 3` with `--simulate-hls-expiry`. Earlier requests for
the selected root reach YouTube normally; its third request receives the single
local 403. The same FFmpeg process must finish the requested decode duration.
FFmpeg progress timestamps are checked, so an exit code of zero after an early
ending is not sufficient. Reports include decoded durations and the root request
count at injection. Increase `--decode-seconds` up to 3600 for longer host cases;
this remains separate from PS5 continuity and naturally expired-link acceptance.

`bash scripts/test_live_chat_native.sh` covers the shipping reader and native
compose/review/send provider with ASan/UBSan. It prepares the native overlay;
then `bash scripts/test_live_chat_ui.sh` exercises the actual generated RML
callbacks, L3 composition, review text and literal rendering. These host tests
mock HTTP and do not send real account messages or prove the PS5 virtual keyboard.

After building the library worker, an explicit anonymous chat-service probe is:

```sh
PYTHONPATH=companion .venv/bin/python scripts/probe_live_chat.py \
  --video LIVE_VIDEO_ID --node /path/to/node --mode top
```

It opens, polls once at the server interval, closes the chat and terminates its
owned worker. Diagnostics retain counts and stable error codes, without messages,
server continuations or account credentials. This does not exercise native chat
display, replay, posting or console behavior.

To run the same host decode/seek check through an already authorized OAuth
profile, explicitly add all three account options:

```sh
PYTHONPATH=companion .venv/bin/python scripts/probe_youtube.py \
  --video VIDEO_ID --codec avc --height 720 \
  --js-runtime node --js-runtime-path /path/to/node \
  --profile AUTHORIZED_PROFILE_ID --library-node /path/to/node \
  --account-state /path/to/private/companion-state \
  --output output/validation/oauth-youtube.json
```

The profile must exist in the supplied `oauth-profiles.json`; missing, corrupt,
Guest or incomplete account selections fail before the server starts. The probe
uses the shipping OAuth preflight, TV resolver and relay, with no anonymous
substitution. An expiring account token may refresh and persist through the
normal companion storage guards. It does not initiate device authorization,
submit history reports or perform account mutations. Account credentials remain
on the companion host, while the PS5 uses Lapy storage in `/data/prosperotube`.
Its metadata report omits profile names, store paths, credentials and media URLs.
Host success still requires separate PS5 playback acceptance.

Expected upstream failures are mapped to `video_unavailable`,
`authentication_required`, `youtube_challenge`, `youtube_rate_limited`, or
`resolver_network` when recognized. Unknown errors retain `extraction_failed`.
The worker returns only an allowlisted code, and the parent supplies a fixed
message. Raw upstream text, cookies and signed URLs never enter API errors.
YouTube authentication errors use 403 so they do not invalidate LAN pairing.

For a public TV script/evaluator compatibility check in WSL/Linux:

```sh
PYTHONPATH=companion .venv/bin/python scripts/probe_tv_signature.py \
  --node /path/to/node --output output/validation/tv-signature.json
```

This explicitly fetches the public TV configuration and its matching player
script using the pinned TV user agent. It checks the pinned timestamp parser
and Node/EJS signature and `n` transforms with synthetic inputs. Requests reject
redirects, other origins, credentials and responses over 8 MiB; the overall
worker deadline is 90 seconds, with its process group retired afterward.
The report contains only hashes, counts and booleans. A pass establishes
compatibility with that fetched script; real account media URLs and playback
still require separate acceptance. No account, media or console request occurs.

Shipping WSL/Linux resolver workers also run in isolated process groups.
Cancellation, timeout and completion retire evaluator descendants before
returning the shared worker slot.

## Offline console fixtures

After generating media:

```sh
PYTHONPATH=companion .venv/bin/python -m prosperotube \
  --host 0.0.0.0 --public-base http://YOUR-PC-LAN-IP:8765 \
  --fixtures fixtures/generated
```

Pair and search any term. Three labelled synthetic videos exercise AVC/AAC DASH,
VP9/Opus DASH and finite HLS. Thumbnails/media are local. This mode does not
contact YouTube or SponsorBlock and does not establish service compatibility.
The HLS fixture also exposes generated rolling top/live chat through R3. Select
an offline account, then use L3 to compose, review the full message and press
Cross to send. A confirmed message appears once in subsequent polls. This checks
the reader, send intent and context lifecycle without posting to YouTube;
physical PS5 keyboard/controller acceptance remains pending.

Native chat checks run in sequence, sharing the prepared EVO tree:

```sh
bash scripts/test_live_chat_native.sh
bash scripts/test_live_chat_ui.sh
bash scripts/preview_native.sh
```

The first command prepares the tree and runs the shipping C reader under
ASan/UBSan. The second compiles the generated EVO callbacks with the shipping
RML and tests selection/scroll retention, removals, mode changes and setup
cleanup. Its host pump simulates the render-time scroll restoration; PS5
controller/render timing still needs hardware acceptance. The preview includes
`output/preview/live-chat.png`. Do not run preparations concurrently.
