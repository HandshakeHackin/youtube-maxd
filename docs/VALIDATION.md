# Offline validation record — 0.1.0

Host: Windows/WSL Ubuntu, LLVM 18.1.8, Python 3.14. Native implementation
revision: `ecacf92` plus the packaged-notices change. This record concerns
offline behavior and packaging, not console or live-service acceptance.

* Companion: 30 automated tests passed. One upstream Starlette TestClient
  deprecation warning remains; no test failed. Tests include authentication,
  pairing expiry/rate limits, stable catalog identities/link browsing, format
  eligibility, session bounds/expiry, range relay, credential scoping across
  redirects, caption timing, asynchronous jobs, cancellation and SponsorBlock.
* Shipping native provider: AddressSanitizer/UndefinedBehaviorSanitizer passed
  with leak checking. Registry isolation, persistence, local favorites/queue/
  follows/history, pairing/jobs, cancellation, origin rejection, chapters and
  transitive SponsorBlock overlaps are checked with mocked native HTTP.
* Real HTTP fixture probe: AVC/AAC DASH, VP9/Opus DASH and finite HLS each
  exposed both audio/video streams, decoded successfully, and passed a host
  seek/decode probe. Timed SRT captions and local PNG thumbnails passed.
  Detailed generated evidence: `output/validation/companion-media.json`.
* Shipping TV RML: host RmlUi/EVO renderer checked eight-card bounds, D-pad
  focus, stable selection, empty/loading/error states. PNG previews were
  inspected. RmlUi emits transient array-binding warnings when fixture rows
  shrink; the rendered states and selection assertions pass.
* PS5 packaging: all 210 native objects compiled; native app converted and
  signed containers inspected with valid integrity. MkPFS packed/verified the
  `.ffpfsc`; folder-title ZIP was generated. Final artifacts/checksums are in
  `dist/`. The app uses `PPSA99142` and independent storage, with original art.

No deployment, console launch, PS5 frame/audio measurement, Google login,
live YouTube extraction or live SponsorBlock call was performed. Follow
HARDWARE_ACCEPTANCE.md before describing the app as hardware-validated.

## PS5 parity integration — 2026-10-09

On the PS5-only branch at `7c63593`, 41 Python tests, 14 Node tests, and the
native provider ASan/UBSan suite with leak checking passed. Library coverage
includes profile isolation, modern cards, bounded continuation and replies;
native mocked HTTP covers profile/feed/playlist navigation. These are offline
checks and do not establish live account behavior or full SmartTube parity.

Candidate `bc3a686` compiled 210 PS5 objects and passed native container and PFS
packaging checks with development controls enabled. Its commands, status and
logs use the application's own data directory. Normal builds keep development
controls disabled. The exact candidate is frozen separately from release output;
console acceptance is recorded in HARDWARE_ACCEPTANCE.md.

## Audio tail completion — 2026-10-09

Revision `0af336d` adds bounded FFmpeg decoder and resampler EOF flushing,
counted padding for the final PCM block, and an epoch-scoped audio drain signal.
The actual generated EVO audio implementation and playback clock were linked
with host FFmpeg 8 and exercised under ASan/UBSan. The cases passed for real AAC
decoder EOF, a 44.1 kHz resampler tail, partial-block padding, output failure,
stale seek epochs, producer parking during seek, and video continuing after the
audio clock stops. Evidence: `.deps/audio-drain-current.log`.

Natural demux EOF is now distinguished from failed input recovery. Known-duration
queue completion additionally requires audio drain. These tests mock AudioOut
and do not prove audible hardware output, video/presentation drain, or completion
of unknown-duration streams. Those remain explicit acceptance and implementation
gaps; the full SmartTube parity goal remains open.

The PS5 build passed compilation of 211 objects, linking, native container
validation and PFS/ZIP packaging. Candidate `0af336d4-audio-drain` is frozen
with 126 files and eboot SHA-256 prefix `374108a2168bc7cb`; its embedded build ID
is `prosperotube-0af336d4_1009-1854`. Evidence:
`.deps/audio-drain-ps5-build.log`. No console deployment or launch was performed.

## Decoder EOF and final-frame retirement — 2026-10-09

Revision `5b98769b` makes decoder EOF distinct from starvation and failure. The
native flush resumes after reorder-buffer backpressure and requires a successful
empty flush before reporting drained. Its unchanged generated flush body passed
sanitizer cases for terminal EOF, backpressure, errors and bounded runaway output
with Sony calls mocked. The actual EVO decode loop, FFmpeg AVC/VP9 decoders,
presentation clock and locked frame snapshot passed sanitizer cases for buffered
tail frames, unknown duration, final-frame retirement and display interval,
audio/input failure gates, post-EOF decoder error, held-frame timestamps, and
stale acknowledgments after seek/reopen. Evidence:
`.deps/video-drain-snapshot-test.log`. Audio and provider regressions also passed
in `.deps/video-drain-audio-regression.log` and
`.deps/video-drain-provider-regression.log`.

The renderer now acknowledges the snapshot's epoch/serial only after a successful
video draw, GPU fence retirement and completed VideoOut flip. Automatic queue
completion requires natural input EOF, audio drain, decoder drain and retirement
of the final published frame for its display interval. It no longer relies on
metadata duration. Development status exposes `complete=` separately from raw
`eof=`. The host injects acknowledgment at the presentation boundary; actual Sony
flush, audible AudioOut and GPU/VideoOut retirement still need PS5 acceptance.

Follow-up `0a2167c` declares the seek reset through the playback header and latches
publication/conversion failure so an unpublishable tail cannot complete playback.
The strengthened actual-engine sanitizer case passed in
`.deps/video-drain-publication-test.log`. The initial PS5 compile exposed the
missing demux declaration; that failed build was not deployed.

The corrected PS5 build passed compilation of 211 objects, linking, native
container validation and PFS/ZIP packaging. Candidate `0a2167ce-video-drain` is
frozen with 126 files and eboot SHA-256 prefix `bafde456039f717d`. Embedded build ID:
`prosperotube-0a2167ce-dirty_1009-1916`; the suffix reflects pending validation
documentation only, with shipping sources committed at `0a2167ce`. Evidence:
`.deps/video-drain-ps5-fixed-build.log`.

The bounded storage/startup case's first console operation found a running title
and stopped before service probes, upload, launch, close or input. Its owned lock
was released. Evidence: `.deps/console/storage-0a2167ce/`. This candidate remains
undeployed; Lapy storage/relaunch and the new playback gates remain unverified
on PS5.

Device authorization backend validation: 89 Python tests and 22 Node tests passed.
Coverage includes authenticated API privacy, polling intervals/slow-down, expiry,
single-flight behavior, cancellation while queued or in flight, rejection of
late credentials, atomic persistence, malformed file preservation, concurrent
writer rejection, dynamic profile registration and private refresh updates.
Pinned YouTube.js TVHTML5 session and refresh paths are exercised with mocked
network responses. Evidence: `.deps/auth-verified-python.log`,
`.deps/auth-final-node.log`, `.deps/auth-worker-build.log`. Follow-up asynchronous
job cases passed in `.deps/auth-jobs-full-python.log`, including nonblocking
requests, delivered/abandoned start ownership and late responses after DELETE.
The first backend commit inadvertently included unfinished job edits while its
shell was still committing. The follow-up corrects these edits and passed the
full suite; the intermediate revision was not deployed.

The explicit public probe obtained a real YouTube device-code response, canceled
locally, stored no credentials and closed its worker. It never polled the token
endpoint, authorized an account or contacted the console. Evidence:
`.deps/auth-device-public-probe.json` and `.deps/auth-device-public-probe.log`.
This establishes public initiation only; real sign-in, private TV catalogs,
OAuth playback and native sign-in acceptance remain pending. No native sources
changed in this stage, so the frozen PS5 candidate remains `0a2167ce`.

The next native sign-in stage passed actual shipping provider ASan/UBSan cases:
pairing/job initiation, automatic bounded polling/single-flight, stable pending
revision/focus, authorized profile selection, cancellation during initiation or
polling, late original-source cleanup, expiry/denial, malformed URL/interval
rejection and absence of codes/session references/tokens in saved PS5 state.
Evidence: `.deps/auth-native-verified-test.log`. Existing provider regressions
passed in `.deps/auth-native-regression.log`. The updated shipping RML rendered
on the host, with sign-in reader/footer visibility and cancel-row activation
checked; `output/preview/device-sign-in.png` was inspected. Evidence:
`.deps/auth-native-final-preview.log`. This proves offline behavior/layout only;
PS5 and real account authorization remain unverified.

Native sign-in candidate `a07e1733` compiled 211 PS5 objects and passed linking,
native container validation and PFS/ZIP packaging. Frozen candidate:
`a07e1733-device-auth`, 126 files, eboot SHA-256 prefix `0b9789b1b61704c4`;
clean build ID `prosperotube-a07e1733_1009-1957`. The Lapy helper remains packaged.
Evidence: `.deps/auth-native-ps5-build.log`, `.deps/auth-native-freeze.log`.

The bounded storage/startup case's first console operation found a running title.
It stopped before probes, upload, launch, close or input and released its owned
lock. Evidence: `.deps/console/storage-a07e1733/` and
`.deps/console/storage-device-auth-run.log`. The candidate remains undeployed;
Lapy storage/relaunch, playback gates and native sign-in require PS5 acceptance.

OAuth resolution adapter: 95 Python and 24 Node tests passed. New cases exercise
the actual pinned yt-dlp TV player request/context, authenticated bounded transport,
redirect/origin rejection, TV script selection, normalized AVC/AAC adaptive formats
and DASH capability creation; they use synthetic responses and no real account.
Profile/cache isolation, TV search continuations, near-expiry token refresh and
private preflight are covered. A real bundled Node worker runs with external fetch
disabled; its private credential result is verified and HTTP attempts to access
that operation are rejected. Evidence: `.deps/oauth-playback-verified-python.log`,
`.deps/oauth-playback-verified-node.log`, `.deps/oauth-playback-worker-build.log`.

The explicit public TV player probe obtained LOGIN_REQUIRED with zero formats
for `jNQXAC9IVRw`. It made one public API request, with no account authorization,
token polling, media download or console traffic. Evidence:
`.deps/tv-player-verified-public-probe.json` and its matching log. This does not
verify account-only streams or current TV signature challenges. No native sources
changed in this stage; the frozen native candidate remains `a07e1733` and undeployed.

The follow-up TV timestamp correction exercises the actual pinned timestamp
parser/cache with distinct synthetic TV variants and mismatching WEB/config/
caller override timestamps. The actual TV player request is checked for the
matching script timestamp in its playback context. Missing script timestamps
remain missing instead of using WEB STS. This is identity/request regression
coverage, not live cipher verification. Evidence:
`.deps/oauth-tv-timestamp-test.log` and `.deps/oauth-tv-timestamp-full-python.log`.

TV evaluator follow-up: 100 Python tests passed, including real WSL/Linux
resolver/evaluator-descendant cleanup on cancellation, timeout and success.
The shared slot is released, failures stay uncached, and a descendant cannot
outlive its isolated worker group. Public probe transport tests cover anonymous
GETs, exact TV origins/paths, response bounds and redirect rejection. Evidence:
`.deps/tv-evaluator-process-verified-test.log`, `.deps/tv-evaluator-full-python.log`.
The Node library implementation is unchanged; its 24-test suite remains passing.

`scripts/probe_tv_signature.py` fetched the real public TV configuration using
the pinned TV user agent, then its matching 2,452,196-byte `tv-player-ias.js`.
The actual pinned timestamp parser and Node/EJS evaluator passed synthetic
signature and `n` challenges. Script SHA-256 prefix: `276ad984607a69bb`.
No account authorization, token poll, Bearer header, media download or console
traffic occurred in this probe. Evidence: `.deps/tv-signature-public-probe.json`
and `.deps/tv-signature-public-probe.log`. This is compatibility evidence for
that script with synthetic inputs, not real signed-media or account playback.

- 2026-10-09 | Lapy | a07e1733 | PS5 6.02 | no-run: first title check busy; no further operations; owned lock released | .deps/console/storage-a07e1733-second/ | storage/startup acceptance

Account removal: 109 Python tests passed. Coverage includes private store reload,
foreign lock/write preservation, post-replacement directory-sync uncertainty,
real bundled Node retirement with fetch disabled, resolver caches and real child
process invalidation, queued extraction rejection, stale authorization/job
results, removal jobs retaining their own confirmation, relay capability/active
stream invalidation without an open-stream TTL regression, peer/Guest availability,
fixture API behavior and shutdown reaping of discarded pending jobs.
Evidence: `.deps/account-remove-full-python.log`,
`.deps/account-remove-verified-focused-python.log`.

Native ASan/UBSan cases passed for full review, one-time removal, profile-bound
confirmation, mismatching confirmations, Guest reset, persisted selection and
authoritative missing-profile reconciliation. Malformed/Guest-missing lists
preserve selection. Existing provider regressions passed. Evidence:
`.deps/account-remove-native-verified-test.log`,
`.deps/account-remove-native-regression.log`. The shipping RML removal review
rendered on the host and was inspected; evidence:
`.deps/account-remove-native-preview.log`, `output/preview/account-removal.png`.
These establish offline implementation only; PS5 and real-account acceptance
remain pending. The Node service source is unchanged, with 24 tests previously
passing; account removal is serialized in the Python companion.

Account-removal PS5 candidate `0e75c5ea` compiled 211 objects and passed linking,
native container validation and PFS/ZIP packaging. Its clean build ID is
`prosperotube-0e75c5ea_1009-2106`. The immutable 126-file candidate is stored at
`/home/denis/.cache/prosperotube/candidates/0e75c5e-account-remove/PPSA99142`,
with eboot SHA-256 prefix `862761034df260c3`. The Lapy helper and notices are
packaged, and PS5 storage remains `/data/prosperotube`. Evidence:
`.deps/account-remove-ps5-build.log`, `.deps/account-remove-freeze.log`.
No console operations occurred in this stage; this candidate remains undeployed.

OAuth host-probe follow-up: 111 Python tests passed. The existing loopback
API/relay/FFmpeg probe now accepts an explicitly configured OAuth profile and
private companion store. Shipping API tests verify private credential preflight,
profile routing, Guest separation, unavailable-profile rejection and response
privacy. Missing/incomplete/Guest selections fail, corrupt stores remain intact,
and nonexistent paths do not create empty state. Evidence:
`.deps/oauth-probe-test.log`, `.deps/oauth-probe-full-python.log`.
These tests use synthetic account credentials and media metadata; no real
authorization, upstream account playback, media download or console traffic
occurred. Native sources are unchanged from the frozen `0e75c5ea` candidate,
whose complete checksum manifest also passed verification.

Comment ratings: 116 Python and 26 Node tests passed. Shipping API/job fixture
cases cover comment/reply rating capabilities, Guest/peer isolation, removal,
changed-intent rejection, expiration and cursor-kind separation. Actual Python
subprocess cases prove confirmed-result reuse and uncertainty retention across
timeout, cancellation, mismatching responses and worker retirement. The pinned
CommentView/NavigationEndpoint/HTTP client sends the retained synthetic command
with TVHTML5 OAuth context; opaque capability responses exclude command keys.
Evidence: `.deps/comment-rating-python-test.log`,
`.deps/comment-rating-full-python.log`, `.deps/comment-rating-node-test.log`.

Shipping-provider ASan/UBSan regressions passed for full comment review,
one-time submissions, mismatching confirmations, rating removal and stale-profile
contexts. The shipping RML rendered; comment-rating review and the updated
discussion footer were inspected. Native sign-in/removal regressions also passed.
Evidence: `.deps/comment-rating-native-verified.log`, `.deps/comment-rating-auth-regression.log`,
`.deps/comment-rating-preview.log`, `output/preview/comment-rating.png`.
No real account mutation or console traffic occurred; live vote readback and
PS5 controls remain unverified.

Comment-rating candidate `0b5f0712` compiled 211 PS5 objects and passed linking,
native container validation and PFS/ZIP packaging. Its clean build ID is
`prosperotube-0b5f0712_1009-2129`. The immutable 126-file candidate is
`/home/denis/.cache/prosperotube/candidates/0b5f071-comment-ratings/PPSA99142`,
with eboot SHA-256 prefix `5487ccd4e7593cd7`; the packaged Lapy helper remains
included. Evidence: `.deps/comment-rating-ps5-build.log`,
`.deps/comment-rating-freeze.log`. The bounded console case's first title check
found a running title and stopped immediately, before probes, upload, launch,
close or input; its owned lock was released. This candidate is undeployed.

- 2026-10-09 | Lapy | 0b5f0712 | PS5 6.02 | no-run: first title check busy; owned lock released | .deps/console/storage-0b5f071/ | storage/startup acceptance

### Live HLS relay and idle capabilities (2026-10-09)

The current public NASA Space Station stream `awQzjn72bI0` passed the shipping
companion pairing/resolve/relay path with `--transport hls --expect-live`. Host
FFmpeg found H.264 video and AAC audio and decoded two seconds at the live edge.
Evidence: `.deps/live-split-public-youtube.json` and its matching log. This was
anonymous public YouTube traffic with no account authorization or console traffic.
The earlier candidate `DfEr5XCFNWM` failed resolution; the first current-stream
case returned `unsupported_media` because split audio/video HLS was not paired.

The pinned yt-dlp master parser now retains explicit audio codec/group relations
from the original response. A second master fetch was rejected as an approach
because refreshed signed URLs did not exactly match the first response. The
shipping adapter does not add that fetch, infer AAC from format IDs, or pair
unrelated manifests. It applies to public/cookie and TV OAuth extractors; real
OAuth live-stream acceptance remains pending.

Active delivery renews the 30-minute idle capability lease. Rolling HLS resource
registries retain the current graph and retire older unreferenced windows at
capacity; invalid/oversized replacements leave the previous graph intact.
Sixteen focused tests passed, including the actual pinned HLS parser, synthetic
rolling windows, shipping HTTP lease renewal, OAuth regressions and real worker
process-group cleanup. The cleanup test's marker is now published atomically to
avoid observing an empty file during the timeout case. Evidence:
`.deps/live-split-focused.log`. Signed upstream URL renewal, long-duration/DVR
behavior and PS5 decoding/audio remain separate acceptance gates.

The final full Python suite passed 121 tests (one known Starlette deprecation
warning): `.deps/live-split-full-python.log`. Native sources were unchanged, so
this companion revision did not produce or deploy a new PS5 package.

### Request-driven live-chat service (2026-10-09)

The service reader now opens top/live chat, polls at the server interval, applies
message replacement and moderator item/author deletions, and closes all references
to a context. Messages and continuations are bounded and profile-isolated. This
is a service implementation; native display, replay timing, posting and complete
scrollback remain unfinished.

All 29 JavaScript tests passed using the installed Windows Node runtime, including
the actual pinned InnerTube chat HTTP transport and parser. They exercise literal
text and paid messages, normalization/privacy, bounded snapshots, cached retries,
early-poll suppression, mode selection, failed updates, capacity, expiry and
close. Evidence: `.deps/live-chat-node-windows.log`. The earlier WSL test run
passed 28 tests before the paid-message/snapshot case was added. The rebuilt
worker passed bundling and `git diff --check` (`.deps/live-chat-build.result`).

The full Python suite passed 122 tests, including authenticated API validation,
asynchronous chat jobs, polling and closure: `.deps/live-chat-full-python.log`.
The first API test expected HTTP 202; it was corrected to the shipping job API's
HTTP 200 contract. A WSL connection timeout prevented the first public probe from
starting; the VM later responded without a forced restart.

Anonymous public chat probes for `awQzjn72bI0` and site-linked `X4VbdwhkE10`
returned `chat_unavailable`/404 at open. These are negative service results, not
chat success or proof that the service works for all current chat layouts.
Evidence: `.deps/live-chat-public.json`, `.deps/live-chat-lofi.json`. No console
traffic, account authorization, chat posts or message-menu actions occurred.

Resolving the official channel's `/@LofiGirl/live` URL returned current live
video `1-LpQekNa9g`. Its anonymous top-chat service probe passed open, one
server-paced poll and close, with 24 messages in both returned snapshots and
`ended: false`. Evidence: `.deps/live-chat-current.json` and matching log. Only
counts, IDs and stable result metadata were persisted; this is public host
service acceptance, not native display, OAuth chat or PS5 acceptance.

The same verified-current video also passed explicit `--mode live`: open,
server-paced poll and close each completed, both snapshots contained 24 messages,
and the chat remained active. Evidence: `.deps/live-chat-live-mode.json` and log.

### Native recent live-chat reader (2026-10-09)

The native C reader passed ASan/UBSan checks for pairing, direct library polls,
server intervals, single-flight requests, 24-message bounds, stable message
identities, mode changes, ended/failed states, old-source late cleanup and saved
state privacy. Evidence: `.deps/native-chat-test.log`. The full Python suite
passed 124 tests, including abandoned chat-job cleanup and rolling profile-bound
offline fixtures: `.deps/native-chat-full-python.log`.

The actual generated EVO RequestPage, RefreshChat, ItemsCallback, ApplyItems and
focus methods passed a host integration check with the shipping C reader and
RML. It moves selection while a request is pending, retains completion-time
selection/scroll, handles selected-message removal, switches top/live mode and
closes chat on setup. Literal text cannot create markup elements. Evidence:
`.deps/chat-ui-test.log`. The test uses mocked HTTP/time and a host focus/render
pump; it does not establish console controller timing or rendering acceptance.
Replay, posting, complete scrollback, real account chat and PS5 chat remain
unverified or unfinished.

Existing provider and device-auth/removal sanitizer regressions passed:
`.deps/chat-native-regression.log`, `.deps/chat-auth-regression.log`.
The shipping RML preview passed its binding, navigation, literal-text and
reader-scroll assertions; `output/preview/live-chat.png` was inspected for
reader and chat-footer visibility. Evidence: `.deps/chat-preview.log`.

Candidate `30a1bef8` compiled 211 PS5 objects and passed linking, native container
validation and PFS/ZIP packaging. Clean build ID:
`prosperotube-30a1bef8_1010-0015`. The frozen folder contains 126 files, with
eboot hash prefix `b92428ceaf99e560`, under
`candidates/30a1bef8-native-chat/PPSA99142` in the local build cache. Evidence:
`.deps/native-chat-ps5-build.log`, `.deps/native-chat-freeze.log`.

The bounded Lapy storage/startup case found an active title on its first console
operation, the minimal read-only FTP running-title check. It stopped before
service probes, upload, launch, close or input, and released its owned lock.
The candidate remains undeployed. Evidence: `.deps/native-chat-console.log`.

- 2026-10-09 | native chat / Lapy storage | 30a1bef8 | PS5 | no-run: first title check busy | .deps/native-chat-console.log | continue offline, await idle console

### VOD expired-link renewal (2026-10-09)

The shipping relay now performs one cache-bypassing, same-profile resolution
after an expired VOD HTTP/DASH request. It preserves session links/manifests,
atomically maps exact format roots, and shares representation observations with
older in-flight responses. Resumed ranges verify strong HTTP identity, total
size, requested bounds and content coding before any renewed body is forwarded.
The policy and remaining paths are detailed in `docs/PROTOCOL.md`.

The full Python suite passed 146 tests: `.deps/media-refresh-python.log`.
Coverage includes ETag/Last-Modified/Latin-1 tag handling, first-open renewal,
both-track mapping, concurrent single-flight requests, cooldown, cancellation,
profile invalidation, changed/ambiguous formats, changed duration/video/entity,
ignored/wrong/oversized ranges and repeated upstream expiry. Actual OAuth
preflight/base-extractor tests verify fresh extraction bypasses its cache while
retaining the selected profile and private token IPC.

Public video `jNQXAC9IVRw` passed AVC/AAC and VP9/AAC DASH cases at 240p: actual
API pairing/resolution/ffprobe, one local injected expired-link response, exactly
one fresh resolution, then real audio/video decode and seek through the same
relay URLs. Evidence: `.deps/media-refresh-public.json`, matching log, and
`.deps/media-refresh-public-vp9.json`, matching log. The reports explicitly mark
the simulated expiry. These are host proofs with real YouTube media; naturally
expired links, uninterrupted mid-playback continuity, OAuth media and PS5 recovery
still need acceptance. Live HLS renewal and controlled native restart remain
unfinished. No native source/build or console traffic changed in this stage.

### Native retry exhaustion classification (2026-10-09)

Repeated premature EOF responses could previously pass the natural-EOF check
after the demuxer's ten read-recovery attempts. The generated engine now
publishes one atomic input state (active, natural EOF, terminal failure).
Exhausted retries, other terminal read errors, and demux packet-allocation
failure cannot authorize completion. Open/seek and newly received packets reset
the input state through the existing reset path.

`scripts/test_video_drain.sh` passed ASan/UBSan checks. The AVC and VP9 tests
extract the exact generated demux classifier and retry-finality function,
exercise premature EOF through the retry limit, and feed their classification
to the real decode/render completion gate after final-frame retirement.
EOF with a latched I/O error, exhausted EOF with and without a latched error,
ordinary read failure, allocation failure, reset, and clean EOF are covered.
Evidence: `.deps/input-failure-test.log`, exit result 0. This proves host
classification and completion behavior; controlled native restart and PS5
network interruption, queue/watch-state and sound acceptance remain pending.

- 2026-10-09 | retry exhaustion | 0f961373 | PS5 build | pass: 211 objects, linking, native containers, PFS/ZIP; frozen 126 files, eboot 3edf184947505bd1 | .deps/input-failure-ps5-build.log; .deps/input-failure-freeze.log | controlled restart, PS5 acceptance

Clean build ID: `prosperotube-0f961373_1010-0043`. Both build and freeze returned
0; the candidate remains undeployed. No console connections were made.

### Controlled native restart (2026-10-09)

The native player now routes terminal input failures through the existing
stop/resolve/launch lifecycle, with a RAM-only, same-video/profile/source/token
recovery context. Each explicit selection permits three restarts. The provider
retains fractional VOD position, caption visibility and the selected format;
live reopens use the live edge. Paused, scrubbing and seeking playback waits.
Missing/ambiguous replacement formats fail rather than opening another quality.
Cancellation and scope changes invalidate pending recovery. A changed context
after open stops the new session before navigation. Open failure returns to the
browser; decoder fatal errors retain their independent stop path.

Full Python suite: 147 passed, one known Starlette deprecation warning.
`scripts/test_native.sh` passed original provider and recovery ASan/UBSan
regressions. Shipping provider/HTTP jobs verify `fresh` resolution, exact
fractional resume, live-edge position, unique format matching, queue retention,
budget retention across reopens, cancellation and profile/source invalidation.
Evidence: `.deps/recovery-python.log`, `.deps/recovery-native.log`; both exit 0.
The Python API test verifies fresh dispatch through both direct and job routes.
These are offline proofs; hardware restart,
caption/audio/position continuity, controller feel and network interruption
acceptance remain pending. No console traffic occurred.

The exact generated resolve callback, launch worker, pending record and
post-open context guard also passed a host ASan/UBSan regression. The test uses
real pthread creation and joins, with mocked playback-engine/UI boundaries.
It verifies stale callback rejection, selection of the matched choice, fractional
resume, live-edge restart, headers, caption restoration, failed open and stopping
an opened session after context invalidation. Reproduce with
`bash scripts/test_recovery_handoff.sh`. It does not prove actual PS5 teardown,
demux seek, audible output or physical controller behavior.

- 2026-10-09 | native recovery | 17f1b904 | PS5 | pass: build/package/freeze, undeployed | .deps/recovery-ps5-build.log; .deps/recovery-freeze.log | interruption/continuity acceptance

All 211 PS5 objects, linking, native container inspection and PFS/ZIP packaging
passed. Clean build ID `prosperotube-17f1b904_1010-0100`; immutable candidate
contains 126 files with eboot SHA-256 prefix `e0c8e041e3dc43c4`. Build and freeze
both returned 0. The exact-title Lapy helper is packaged and verified; no console
connections occurred during the offline build/test. Host handoff evidence:
`.deps/recovery-handoff.log`, `.deps/recovery-handoff-promoted.log` (both exit 0).

- 2026-10-09 | recovery / Lapy startup | 17f1b904 | PS5 | no-run: first title check busy | .deps/recovery-console.log | continue offline

The bounded storage/startup case stopped at its first console operation, the
minimal read-only FTP running-title check. No further service probes, upload,
launch, close or input occurred. Its owned lock was released. The candidate
remains undeployed; PS5 recovery and storage acceptance remain pending.

### Paired live HLS root renewal (2026-10-09)

The shipping relay now renews expired live playlist roots under the owning
profile, retaining local manifest/capability URLs. Exact unique format identity
and the refreshed audio-group/master pairing are checked before atomic root
replacement. Segments, keys and initialization objects are not silently
remapped. Dynamic roots omit byte ranges and immutable VOD validators. Late
responses cannot restore the previous root URL.

Full Python suite: 160 passed, one known Starlette deprecation warning; result
0 in `.deps/live-refresh-python-final.result`, log
`.deps/live-refresh-python-final.log`. Tests exercise shipping GET/HEAD relay,
fresh scoped headers, stable manifests, concurrent failures, cooldown, changed
formats/pairing, late responses, cancellation and removed-session rejection.

Real public YouTube NASA stream `awQzjn72bI0`, AVC/AAC split live HLS at 240p,
passed pairing, resolution, ffprobe and two-second audio/video live-edge decode
after one locally injected root 403 and exactly one fresh resolution through
the unchanged local relay manifest. Evidence: `.deps/live-refresh-public.json`,
`.deps/live-refresh-public.log`, `.deps/live-refresh-public.result` (0).
Reproduce with `scripts/probe_youtube.py --transport hls --expect-live
--simulate-hls-expiry` and the required video/runtime options in BUILD.md.
No account profile or console traffic was used. This is host renewal evidence;
natural expiry, long-session continuity, child-resource/DVR recovery, audible
audio and PS5 playback remain separate acceptance requirements. Native sources
are unchanged; the frozen recovery candidate remains undeployed.

### Sustained decode across live root expiry (2026-10-09)

The probe now checks the final FFmpeg decoded timestamp instead of accepting
exit status alone. Missing/malformed progress and early successful endings fail.
An optional delayed injection forwards the first root requests normally, then
expires exactly the requested playlist refresh during the same decode process.
Full Python suite: 167 passed, known Starlette warning only; evidence
`.deps/live-sustained-python.log` and `.deps/live-sustained-python.result` (0).

The real public NASA AVC/AAC split HLS case passed with `--decode-seconds 60
--simulate-hls-expiry --hls-expiry-after-requests 3`: final decoded timestamp
60.01 seconds, exactly one injected expiry on root request three and one fresh
resolution. The same FFmpeg process used the unchanged local manifest throughout
the decode. Evidence: `.deps/live-sustained-public.json`,
`.deps/live-sustained-public.log`, `.deps/live-sustained-public.result` (0).
This extends the two-second startup-renewal case to an ongoing host decode;
it does not establish PS5 pacing/audio, natural signed-link expiry, 30-minute
lease continuity or full DVR behavior. No account profile or console traffic.

### Companion live-chat message intents (2026-10-09)

Added account-only `chat_send` with an owned active chat cursor, exact video ID,
unique intent and bounded Unicode text. The actual pinned YouTube.js sender
creates the request; one matching returned text-message action is required for
confirmation. No automatic reader starts and no optimistic message is inserted.
Companion guards survive Node loss, timeout and cancellation without replaying
an uncertain send; changed text/video/cursor cannot reuse an intent. Confirmed
retries return allowlisted metadata. Native compose/review/cancel and real-account
posting acceptance remain pending. No real messages were sent.

Full Python suite: 172 passed, known Starlette warning only; Node worker suite:
31 passed; worker bundle build: pass. All three returned 0. Evidence:
`.deps/chat-send-python.log`, `.deps/chat-send-node.log`,
`.deps/chat-send-bundle.log` and corresponding `.result` files. Tests use the
shipping direct/job APIs, actual worker subprocess loss, and the actual pinned
sender/parser with fetch replaced. Unicode request text, WEB request shape,
profile/video ownership, one-time confirmation, blank/control/oversized text,
worker restart, timeout and cancellation are covered. These are offline proofs,
not authorization or YouTube acceptance of an actual account message.

- 2026-10-09 | recovery / Lapy startup | 17f1b904 | PS5 | no-run: current first title check busy | .deps/storage-current-console.log | continue offline

The current frozen recovery candidate case again stopped at its first read-only
running-title check. No other probe, deployment, launch, close or input occurred;
the owned lock was released. The frozen candidate remains undeployed.

### Native chat compose/review/send (2026-10-09)

L3 in an active chat opens a RAM-only message prompt, retaining its owned reader
context. Review displays the selected profile, video and full literal text.
Cross creates one chat-send job; repeated confirmation does not submit or cancel
another job. Returning to chat, leaving the browser or profile/source changes
invalidate the draft and stale keyboard callbacks. The confirmed response must
echo the video and intent and include a message ID. Uncertain sends cannot reuse
the action. UTF-8 validation bounds input to 200 scalar characters / 800 bytes,
rejecting blank, control, malformed, surrogate and overlong forms. R3 and
Touchpad retain their reader-mode/comments functions.

Shipping reader and compose/review provider ASan/UBSan tests passed, as did
actual generated RML callbacks and broader provider/recovery regressions.
Evidence: `.deps/chat-compose-native.log`, `.deps/chat-compose-ui.log`,
`.deps/chat-compose-provider.log` and corresponding `.result` files (all 0).
The RML case verifies L3 navigation, review state, full RAM text, no early HTTP
send and literal markup. Existing RML warnings remain in the host log. These
tests use deterministic HTTP; physical PS5 virtual keyboard/controller behavior,
real-account sending and hardware continuity remain unverified.

- 2026-10-09 | fallback availability | read-only | pass: idle running-title list, no other probes or changes; owned lock released | .deps/fallback-idle.json | recheck idle before deployment

- 2026-10-09 | native chat / PS5 package | 9a7609c8 | PS5 | pass: 211 objects, link, native container, package and freeze | .deps/chat-compose-ps5-build.log; .deps/chat-compose-freeze.log | storage/startup acceptance

Build ID `prosperotube-9a7609c8_1010-0141`; frozen candidate contains 126 files,
eboot SHA-256 prefix `08b98295d73f6689`. Build and freeze returned 0.

- 2026-10-09 | fallback Lapy startup | 9a7609c8 | PS5, firmware unreported | failed: Lapy helper apply_failed, startup stopped | .deps/chat-compose-fallback-console.log; .deps/console/storage-chat-compose-9a7609c8-fallback/klog.txt | diagnose offline, no identical-byte retry

The idle-first case uploaded and read back all 126 frozen files, verified them
again after the 130-second settle, established exact title registration and
rechecked idle before sending one launch. The script returned 1 with an
inconclusive missing-checkpoint result. Captured klog strengthens that result:
the matching build reached main, configured 3840x2160 display and native IME,
then reported `Lapy: path=helper status=7; durable storage unavailable` and
`ProsperoTube requires writable /data/prosperotube; startup stopped`.
Status 7 is `elevation::Status::apply_failed`. The system subsequently captured
a title coredump and terminated it. The exact helper failure stage and shutdown
behavior remain to be diagnosed; no root-layout cause is assumed from this code.
No application status/UI/storage-ready checkpoint was produced. No input,
second launch or close payload was sent; the owned lock was released. No further
console traffic followed the failure. Storage persistence, clean teardown,
service health after termination, playback, audio and chat remain unverified.

### Startup logger / Lapy precondition correction (2026-10-09)

Closer inspection of the captured fallback klog identifies SIGSYS (signal 12)
after `eboot.bin calls exit() exit_value=1`; it does not establish a GPU fault.
The application returns from failed initialization without its normal run-loop
shutdown. That failure lifecycle remains a separate hardware acceptance item.

The generated boot logger now refuses to open its file, start the writer or
start the log server until `evo_jailbreak_is_open()` confirms the complete
storage proof. Previously a writable sandbox-mounted path could activate those
threads before Lapy; the pinned helper requires a single-threaded target with a
private credential. The native video watchdog does not start in the initial
probe: its start calls belong to decoder renewal and playback. No claim is made
that the logger was the exact cause of the captured status 7. Helper stage logs
were unavailable in that case. No further console connection or retry occurred.

`scripts/test_startup_log_gate.py` exercises the actual generated flush body:
repeated unproved calls retain buffered messages without disk/thread/server
effects; proved startup opens the log and enables those sinks, and subsequent
flushes drain without reopening. Run after `scripts/prepare_native.py`.

Generated-flush regression and both existing/resident/helper and mounted-data
listing regressions passed; wrapper result 0. Evidence:
`.deps/startup-log-test.log`, `.deps/startup-log-elevation.log` and
`.deps/startup-log.result`. This is host evidence. A new PS5 build and an
idle-first console case with helper-stage capture remain required.
