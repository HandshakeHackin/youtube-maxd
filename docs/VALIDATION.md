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
