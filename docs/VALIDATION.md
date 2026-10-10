# Offline validation record — 0.1.0

## Explicit highlight jumps and paused seek intent — 2026-10-10

R3 in the player now selects the nearest future SponsorBlock highlight point
with silent/message/ask/disabled actions. Points remain zero-width markers;
they never create automatic intervals from zero to the highlight. Manual intent
ignores automatic minimum/repeat policy. Point review has its own start position,
survives before the target, expires at the point, and does not consume overlapping
interval review. Duplicate requests before seek clock movement are guarded;
finished/stale/source/profile/live contexts and invalid or endpoint targets are
rejected. Player R3 is routed through the global dispatcher; player L3 and
both sticks on other screens retain capture. Keyboard/browser ownership is kept.

An explicit pause flag now reaches the shipping controller's seek mailbox.
Highlight jumps and accepted reviews preserve the engine's paused state, including
request refusal; the one-argument scrub/legacy seek contract still resumes.
Confirmation cannot submit a competing seek during scrub/seek/prebuffer ownership,
while Circle can cancel review. Resolving a former live video as VOD now refreshes
its live flag before optional metadata eligibility is decided.

The final unchanged-source run passed the three shipping native provider,
recovery and SponsorBlock ASan/UBSan executables; extracted shipping player paths;
combined global dispatch/provider bindings/player highlight input; and both
shipping controller seek overloads with mocked mailbox/engine functions.
Coverage includes unsorted points, disabled colors, a two-second manual jump with
60-second automatic minimum, confirmation/decline, overlap, duplicate requests,
archive transition, source/profile invalidation, endpoint/non-finite input,
playback gates, pause on success/refusal and unchanged legacy behavior.
Evidence: `.deps/highlight-{native,player,dispatch,seek-pause}.log` and matching
`.result` files (all 0). New-package/physical controller acceptance remains pending.

Reference review used SmartTube `2c60453007dfc3775ff525c3837c2d1d4e95aab9`
SponsorBlockController/SponsorBlockData and MediaServiceCore
`4d9767733aa92269b9d24f24844a7fa1bb5196c1`
[YouTubeSponsorSegment](https://github.com/yuliskov/MediaServiceCore/blob/4d9767733aa92269b9d24f24844a7fa1bb5196c1/youtubeapi/src/main/java/com/liskovsoft/youtubeapi/service/data/YouTubeSponsorSegment.java)
and Segment. They preserve point coordinates; R3 is the explicit PS5 control
mapping. This does not establish complete SmartTube SponsorBlock parity.

## Provider stick controls survive global dispatch — 2026-10-10

The committed `d4d5b66d` PS5 candidate passed all 211 objects, linking, signed
containers, packaging and immutable freeze: `prosperotube-d4d5b66d_1010-1450`,
126 files, eboot SHA-256 prefix `2f50242646332878`. Evidence:
`.deps/controller-dispatch-ps5-build.log` / `.result` (0) and
`.deps/controller-dispatch-freeze.log` / `.result` (0). Console acceptance of
this new package remains pending.

The generated application now reserves L3/R3 for the provider screen rather
than consuming them for screenshot capture. Keyboard and system-browser input
also retain their buttons. Screenshot capture remains available on other
screens, including the player.

`scripts/test_controller_dispatch.py` extracts and compiles the shipping global
dispatch block together with the shipping YouTube provider stick bindings.
ASan/UBSan passed L3 composition, R3 discussion, Touchpad comments, L2/R2,
simultaneous-stick precedence, unchanged press/hold/release forwarding, screenshot
success/failure on player/settings, other-provider forwarding, keyboard/browser
ownership and idle input. The screen manager, capture and discussion callbacks
are mocked; physical pad/chat behavior and the new package remain unaccepted.
Evidence: `.deps/controller-prepare.log`, `.deps/controller-dispatch.log` and
`.deps/controller-dispatch.result` (0).

## First actual YouTube playback on PS5 — 2026-10-10

The fresh reserved `.40` case used unchanged native build
`prosperotube-b9b5c916_1010-1402` and corrected Windows companion `c7d6f35`.
Both idle checks, both full installed-byte verifications, settling, registration,
Lapy storage, UI and first frame passed. Native pairing and direct public lookup
returned `jNQXAC9IVRw` ("Me at the zoo"). The resident Sony AVC decoder opened
320x240 at 15 fps; native AAC initialized for stereo 44.1 kHz with an AudioOut
handle. Playback progressed from 0 to 18.87 seconds, reporting `eof=1`,
`complete=1`, `fatal=0`. All 133 sampled statuses had no decoder fatal; 130 were
active. Shipping engine stop statistics report 284 frames in/converted/published
and zero late drops. The 3840x2160 scanout capture visibly contains the video,
English captions and native OSD. Physical audible sound remains unverified.

The original adapter retains result 1: its final-position threshold required
within 0.10 seconds of metadata duration 19, while the last presented frame is
0.13 seconds short (two frames at this source's 15 fps). This is a verifier
failure, not evidence that playback failed. Independent read-only analysis of
the saved samples proves progress and final EOF/completion within 2.1 frames.
Evidence: `.deps/public-video-utf8-reserved-console.log`,
`.deps/console/public-video-utf8-b9b5c916-fallback/`, and
`.deps/verify-public-video-end.py`.

A bounded cleanup verified our launched process identity, requested quit,
proved matching GPU park, then sent one exact-title close. Matching kill,
video-owner removal and app-mount completion, idle titles and reachable services
passed. Saved version-2 pairing and Guest history contain this video with
`played=true`, `position=0`, `duration=19`. Evidence and capture:
`.deps/console/public-video-utf8-b9-cleanup/` (including `youtube-scanout.bmp`).
No account mutation or real chat message occurred. History reload, pause/seek,
other formats, physical audio/controller pacing and long sessions remain open.

## Windows public resolver encoding corrected — 2026-10-10

The first real public-video case on `.40` passed immutable installed-byte
checks, settling, final idle, Lapy storage and native UI startup. Pairing saved a
version-2 configuration under `/data/prosperotube`, but public search returned
`invalid_response`; no playback started. Original case result remains 1:
`.deps/public-video-reserved-console.log` and
`.deps/console/public-video-b9b5c916-fallback/`.

A separate bounded cleanup verified that only our launched PID 121/app ID
`0x8018` remained, requested graceful quit, proved matching `parked=1`, then
sent one exact-title close. Matching kernel kill/video-owner/mount-end records,
absence of running titles and reachable services passed. Evidence:
`.deps/console/public-video-b9-cleanup/`. This cleanup does not accept playback.

The shipping Windows worker reproduced the fault locally: exit 0, 105,427 output
bytes, invalid UTF-8 but valid CP1252 JSON for `jNQXAC9IVRw` with 24 formats.
The worker now explicitly configures protocol input/output as strict UTF-8,
independent of the Windows system code page. A real redirected-stream regression
round-trips Japanese, accented text and emoji with CP1252 initial streams and
keeps extractor output separate. All 18 resolver-boundary tests passed on Windows.
Fresh actual public extraction then returned valid UTF-8 JSON and 24 formats.
Console playback with this companion correction remains the next case; native
package bytes remain the accepted `b9b5c916` build.

## PS5 storage startup and first graceful close — 2026-10-10

Source `b9b5c916` passed all 211 PS5 objects, linking, signed containers,
packaging and immutable freeze: `prosperotube-b9b5c916_1010-1402`, 126 files,
eboot SHA-256 prefix `91e48da19d2a2c14`. Build and freeze result files are 0.
The reserved `.40` case verified all installed bytes twice before one launch.
Lapy observed one stopped thread and two credential references, completed the
credential/root transition and donor balance, and reported success. Required
storage, resident AVC/HEVC/VP9/HEVC10 creation, native UI and first frame passed.

The app reached `parked=1` before the exact-title close. The original adapter
retains result 1 because it required a ShadowMount release phrase absent on this
console. Captured kernel records instead bind PID 106/app ID `0x4018` to the
title's kill, video-owner removal and completed `EndAppMount`. A separate fresh
idle-first read-only case confirmed no title sandbox and reachable FTP, klog and
elfldr. Evidence: `.deps/lapy-warmup-reserved-console.log`,
`.deps/console/storage-lapy-warmup-b9b5c916-fallback/` and
`.deps/console/warmup-release-readonly-b9b5c916/`.

No preferences were changed; `youtube.json` was absent and a second launch was
not attempted. Configuration persistence across relaunch, actual YouTube
playback, physical sound/pacing and controller acceptance remain pending. The
next case's release predicate requires all three matching kernel records and
absence of the running title; positive/wrong-identity/missing-record offline
checks passed, as did initial-FTP/SELF-mode ordering checks.

Review also found that the application's screenshot handler consumes L3/R3
before provider screen input. Standalone input tests do not prove those physical
chat controls; dispatch integration remains to be corrected and tested.

## Native warmup before single-thread Lapy request — 2026-10-10

The complete diagnostic app build/package/freeze passed for source `146945b5`:
`prosperotube-146945b5_1010-0612`, 126 files, eboot SHA-256 prefix
`5c55d9800fcf741b`. Evidence: `.deps/lapy-stop-ps5-build.log` / `.result` (0)
and `.deps/lapy-stop-freeze.log` / `.result` (0).

The reserved fallback case passed idle-first installation, both complete byte
readbacks, settling, registration and final idle check. One launch again failed
the storage gate; the adapter stopped without input/close/relaunch and released
its owned lock. A separate idle-first read-only case retrieved the matching
diagnostic helper log. Evidence: `.deps/lapy-stop-reserved-console.log` /
`.result` (1), `.deps/console/lapy-stop-helper-readonly-146945b/`.
The 2,680-byte helper log SHA-256 is
`acbd341e849fef0404add54027e84c907a49deea34b49af61bcb5835ec89264d`.

Both stopped snapshots report `fd_refs=1`, `threads=92`, `suspended=92`,
`cred_refs=92`, matching proc/fd/credential/prison/root/jail/cwd and thread-list
identities, and `stable=1`. The helper requires `cred_refs=2` there and a
single-thread private credential later. Its `target_stop error=16` rejection
therefore precedes root/credential mutation, with `detached=1`. These values
establish the incompatible startup state, not which individual preload created
each worker. FTP retrieval passed after failure; comprehensive service-health
and clean app teardown acceptance remain absent.

The startup overlay now follows ProsperoLight's query-only VideoDec2 warmup
approach (`src/main.cpp`, `src/moonlight_stream.cpp` in the inspected reference):
load VideoDec2 and query compute/selected codec memory information before Lapy,
without creating compute queues, allocating decoder memory or creating resident
decoders. Cache that module result across the credential transition. Resident
video decoder and AGC/display creation now follow successful storage proof.
Existing audio-library and dialog preloads retain their ordering; a fresh
changed-candidate console case must establish whether they meet the remaining
single-thread precondition. This is not yet a proven startup fix.

The extracted generated preload body passed host ASan/UBSan for AVC-only,
secondary codecs and 10-bit configurations, checking query-only calls,
configuration fields, idempotence, failed module loading and optional query
failure. Actual generated application-order assertions verify that preloads
precede Lapy and display/resident creation follows it. Evidence:
`.deps/native-preload.log` / `.result` (0). The existing generated startup-log
gate regression also passed, preserving buffered logging before storage proof.
Sony module/decoder behavior remains mocked in these host tests.

## Reserved console startup and Lapy stop diagnosis — 2026-10-10

Frozen `prosperotube-58390ef6_1010-0550` passed the initial idle check,
126-file stored-byte upload/readback, 130-second settling, a second complete
readback, registration and final idle check on `192.168.4.40`. One launch reached
the matching build's `main()`. Lapy returned `apply_failed` (7), startup stopped
at the required `/data/prosperotube` gate, and the system recorded user-process
SIGSYS. The adapter stopped without input, close or relaunch and released its
owned shared lock. Evidence: `.deps/marker-storage-reserved-console.log` and
`.result` (1), `.deps/console/storage-marker-58390ef6-fallback/klog.txt`.

A separate idle-first, read-only case retrieved the matching helper log and
released its lock. Evidence: `.deps/console/marker-helper-readonly-58390ef6/`.
The 2,331-byte log SHA-256 is
`62501a31de0592c27689a4a66a82a59496148d7556f3cc0aa9b0253c6c70c944`.
Helper build `2daddf913d859dd6358295c6cf46573e7196ef478ccd67d3acec730b0860b5d9`
recorded firmware `12700001`, successful helper/root-layout initialization,
then `stage=target_stop error=16 roots_moved=0 cred_changed=0 detached=1`.
This establishes rejection before credential/root mutation. It does not identify
which stop predicate failed: credential references, stable snapshot fields,
private filedesc, and suspended thread counts are checked together. The log
does not prove the app was multithreaded or that a particular preload caused it.

The optional `PROSPEROTUBE_LAPY_STOP_DIAGNOSTICS=1` build adds a snapshot-field
record at that check using only already-captured snapshots. It builds in a
separate source copy, leaves the pinned checkout untouched, retains the exact
upstream return predicate and records upstream/compiled daemon hashes in the
packaged manifest. It requires the existing diagnostic result-file option.
It changes no stop, credential, root-transfer or retry policy. Console acceptance
of this new diagnostic variant remains pending.

The diagnostic helper compiled with the pinned PS5 SDK and upstream `-Werror`
flags: build `8d7cb08a28bac6d657bba735f959c0c1ca1cc1149da3055e83a9586f03a4ed6a`,
ELF SHA-256 `d4a67ecfe222cff5972f3555e76a6046b6b908096a4fccc474bd8b50be39031b`.
The verification case proved that removing the single inserted log block
reconstructs the exact original source, that the original stop predicate remains
once, that the pinned checkout is clean, and that packaged ELF/protocol/source
hashes match. Evidence: `.deps/lapy-stop-diagnostic-build.log` and
`.deps/lapy-stop-diagnostic-verify.result` (0). Python syntax and shell parsing
checks passed. The initial shell wrapper's result capture was malformed; its
exit status is not used as build evidence. The separately verified compiler
artifact and manifest establish helper build success, not console acceptance.

## SponsorBlock timeline markers and playback lifetime — 2026-10-10

Source `58390ef6` adds independent persisted timeline-color toggles for the nine
pinned SmartTube categories. Legacy settings default all marker colors on,
including filler while its automatic skip action remains disabled. Fetches
include the union of action and marker categories. Category RGB values match
the pinned SmartTube color resources.

The companion explicitly requests `skip` and, when selected, `poi` action types.
Highlight points must have equal start/end times and `poi` action type; ordinary
intervals must have positive duration and `skip` action type. This follows the
[API types](https://wiki.sponsor.ajay.app/w/Types) and
[hashed-prefix API](https://wiki.sponsor.ajay.app/w/API_Docs). Malformed top-level
payloads/entries, mismatched action/category pairs, boolean or out-of-range
timestamps and invalid point intervals are ignored. Highlight points do not
become automatic skip intervals; their jump/action behavior remains open.

The shipping PS5 UI-kit OSD and RML fallback draw the same normalized markers.
Intervals clip to known duration, and point markers remain visible inside both
ends of the track. Marker reads require the selected playing instance, profile,
source, master switch and exclusion policy; invalid/unknown duration and live
playback produce no markers. Source/profile changes invalidate old data, even
when switching back. Ordinary browsing-request cancellation no longer discards
the playing video's valid metadata, including a pending matching response.

The three actual native provider/recovery/SponsorBlock ASan/UBSan executables
passed: `.deps/sponsor-marker-native.log` / `.result` (0). The additional pending-
response cancellation assertion passed in the focused actual-provider run:
`.deps/sponsor-marker-policy.log` / `.result` (0). Tests cover default marker
fetches, disabled actions with visible markers, all nine settings rows, persisted
toggles/reload, point validation, clipping/capacity, playing-instance ownership,
profile switching and retained data across browsing cancellation. Extracted
shipping UI-kit and RML draw blocks passed ASan/UBSan with mocked draw-list/DOM
functions: `.deps/sponsor-marker-render.log` / `.result` (0). Point/interval
geometry, right-end containment, unchanged DOM reuse and clearing were checked.
The companion module passed 43 tests, with its inherited Starlette deprecation
warning: `.deps/sponsor-marker-companion.log` / `.result` (0).

All 211 PS5 objects, linking, signed containers, package and immutable freeze
passed: build `prosperotube-58390ef6_1010-0550`, 126 files, eboot prefix
`23101bbd1b1a7f4d`. Evidence: `.deps/sponsor-marker-ps5-build.log` / `.result` (0)
and `.deps/sponsor-marker-freeze.log` / `.result` (0). Bundled attribution matches
the source bytes. Its subsequent reserved-console launch failed at storage
initialization as recorded above; physical marker rendering, controller behavior
and playback remain unverified.

Separately, the prior `c758351c` candidate passed the first idle check on the
primary console, full 126-file stored-byte upload/readback, 130-second settling,
second full readback and title registration. A title started before the final
launch check, so no launch/input/close was sent. The owned lock was released.
Evidence: `.deps/sponsor-end-storage-primary-console.log` / `.result` (1).
This proves installation of those earlier bytes, not Lapy execution or playback.

## SponsorBlock skips to the known end — 2026-10-10

Source `c758351c` handles acknowledged automatic or confirmed skips reaching or
overrunning a known video duration. The player completes deliberately, stops
playback and returns to browsing; normal queue autoplay can select the next
item. It does not submit a demux seek to an unavailable endpoint frame. Natural
EOF still requires the existing decoder/output retirement proof. A subsequent
stop report cannot restore a resume position inside the skipped outro.

Completion authorization is bound to the resolved/playing instance, active
profile, optional-service scope and exact acknowledged target. Unknown/invalid
duration, stale resolutions, another playing video, live sources, declined
prompts and policy/exclusion changes cannot complete the item. Minimum duration
uses the playable remainder when segment metadata extends beyond the known end.

The three actual provider/recovery/SponsorBlock ASan/UBSan executables passed:
`.deps/sponsor-end-native.log` / `.result` (0). Tests cover end/overrun targets,
one-time completion with autoplay off, accepted/declined confirmation, queue
transition, watched/resume state after stop, stale contexts, exclusion changes
and clipped minimum duration. The generated PS5 `PlayerScreen` tick/input paths
were extracted and compiled under ASan/UBSan, with mocked playback/navigation/
pad functions: `.deps/sponsor-end-player.log` / `.result` (0). Those paths cover
silent/message actions, Cross/Circle precedence, stop/return versus in-video
seek, rejected endpoint completion, and pause/scrub/live/seek/buffering gates.
These tests do not establish physical controller behavior or native teardown.

All 211 PS5 objects, linking, signed containers, package and immutable freeze
passed: build `prosperotube-c758351c_1010-0527`, 126 files, eboot SHA-256 prefix
`fbaa644e95bda5b7`. Evidence: `.deps/sponsor-end-ps5-build.log` / `.result` (0)
and `.deps/sponsor-end-freeze.log` / `.result` (0). The frozen package's
`assets/prosperotube-notices/THIRD_PARTY_NOTICES.md` matches the committed source
byte for byte, including the SponsorBlock database/API attribution added after
the prior candidate. Older frozen candidates were not modified.

The reserved-console startup/storage case stopped at its first FTP title check
because a title was running. No other service probe, upload, launch, input or
close followed, and the owned lock was released. Evidence:
`.deps/sponsor-end-storage-reserved-console.log` / `.result` (1). The candidate
is undeployed/unlaunched. Lapy PR50 execution, durable storage and real YouTube
playback on PS5 remain unverified. SponsorBlock timeline colors/highlights,
paid-content notices and full settings/action/default equivalence remain open.

## SponsorBlock category policy — 2026-10-10

Source `1b0a0dca` adds eight skip-category actions: silent skip, skip with a
message, ask before skipping, and do nothing. Same-action overlapping intervals
merge; disabled categories and different actions do not extend that merge.
The category defaults follow pinned SmartTube `SponsorBlockData`: message skips,
filler disabled, five seconds minimum remaining skip, and repeat skipping allowed.
The existing opt-in master switch is retained. Persisted version-2 data gains
optional named category actions, minimum duration, no-repeat and excluded-channel
settings. Legacy files load defaults without losing their existing master flag.

The native player presents confirmation through its existing toast and Cross/
Circle controls; Circle wins if both buttons arrive together. Expired, canceled
and out-of-scope prompts cannot submit a seek. Buffering, seek preroll and live
sources do not trigger actions. Fetches use a distinct resolve serial plus
generation/profile guards, so delayed responses cannot replace another video's
segments. Exclusions are bounded to 128 channels; current/followed channels and
previous exclusions can be paged, toggled and persisted.

All three shipping native provider/recovery/SponsorBlock executables passed
ASan/UBSan: `.deps/sponsor-policy-native.log` / `.result` (0). The dedicated
SponsorBlock test drives actual catalog/settings, queued HTTP resolution and
callbacks, persistence/reload, mixed-category merging, minimum durations,
confirmation acceptance/decline, repeat guards, exclusions, live suppression,
invalid metadata and delayed responses. The companion test module passed 38
tests, including every requested category and boolean timestamp rejection:
`.deps/sponsor-policy-companion.log` / `.result` (0). Its inherited Starlette
deprecation warning remains.

A read-only live host case exercised the shipping companion endpoint against
the real [SponsorBlock API](https://wiki.sponsor.ajay.app/w/API_Docs). Two
four-character-prefix GETs returned HTTP 200 and JSON arrays for all eight
requested categories. A public record present in that response was selected
after fixed sample IDs returned no matching skips; the shipping filter returned
one valid segment. No account, mutation, media download or console was used.
Evidence: `.deps/sponsor-policy-public.json`, `.log` / `.result` (0). Empty sample
and invalid-ID diagnostics are retained separately; they were not accepted as
proof of nonempty filtering.

The complete PS5 build/link/package/freeze passed: build
`prosperotube-1b0a0dca_1010-0504`, 126 files, eboot prefix `b297f87e76c87752`.
Evidence: `.deps/sponsor-policy-ps5-build.log` / `.result` (0) and
`.deps/sponsor-policy-freeze.log` / `.result` (0). The reserved-console case
stopped at its first FTP running-title check. No other service probe, upload,
launch, input or close followed; the owned lock was released. Evidence:
`.deps/sponsor-policy-storage-reserved-console.log` / `.result` (1).

The candidate remains undeployed/unlaunched. On-console prompt behavior, timeline
colors/highlights, paid-content notices, precise outro-to-end behavior and full
settings/default equivalence remain incomplete or unverified. The new database
attribution notice was added after this build and must be bundled in the next
package; the `1b0a0dca` freeze remains immutable.

## Audio seek and EOF lifecycle — 2026-10-10

Sources `bf6852cd` and `c2779195` fix in-place audio-only seeks acquiring a
video presentation hold that no video frame could release. Audio discard also
covers zero and sub-50ms targets. Floating-point uncertainty at exact PCM sample
boundaries no longer trims an extra sample. Invalid native PCM byte counts,
channel counts and frame allocations mark audio failure; the player presents an
audio error before leaving playback.

EOF with an unreached audio seek target is no longer retired as successful
audio output. A video may nevertheless continue silently after an indexed audio
track has ended: known stream start/duration metadata is compared in the common
media timeline before allowing that case. Unknown duration/timing cannot excuse
an unreached seek. This is a bounded failure rule, not proof of parity for all
unknown-duration sources.

The actual generated EVO audio decoder loop, resampler, ring, EOF drain and
output code passed ASan/UBSan. Scripted native-decoder PCM proves all preroll
packets reach the decoder, retained sample contents are correct, counted output
excludes padding, invalid byte counts become fatal, and unreached EOF cannot
become drained. Sony Audiodec and AudioOut are mocked; native codec priming and
audible output remain unverified. Real host AAC decode/EOF and known-duration
audio-tail metadata checks also passed. Evidence:
`.deps/audio-seek-lifecycle.log` / `.result` (0).

The shipping demux request/process methods passed ASan/UBSan with actual FFmpeg
AAC/Opus/HLS seeking and the real `pp_playback` hold/clock. Tests cover audio-only
playing/paused seeks, zero/sub-50ms targets, nonzero origins, video discard and
failed-seek base retention. The presentation frames in this process test are
synthetic; actual AVC/AAC/Opus decode remains covered by the existing resume
tests in the same suite. Evidence: `.deps/audio-only-seek-process.log` / `.result`
(0). The Opus fixture retains its previously documented seek header warning.

The final `c2779195` PS5 build, link, packaging and freeze passed: 126 files,
eboot SHA-256 prefix `c09f0aff45b24d48`. Evidence:
`.deps/audio-seek-lifecycle-ps5-build.log` / `.result` (0) and
`.deps/audio-seek-lifecycle-freeze.log` / `.result` (0). The preceding
`bf6852cd` package also passed; its evidence is preserved under
`.deps/audio-seek-lifecycle-bf6852cd-*`. Neither candidate was deployed or
launched. This milestone made no console connection.

## Accurate resume — 2026-10-10

Source `c6ec8804` preserves decoder preroll while suppressing presentation
before the requested resume position. The initial video discard gate is armed
after `pp_playback_on_file_open`, before consumers start; audio-only playback
does not acquire a video hold. Audio packets reach the decoder normally, and
decoded PCM is discarded or trimmed before resampling. Both normal decode and
FFmpeg EOF drain use the same acceptance helper. Unknown initial audio timing
is bounded; malformed frames mark the existing audio failure state, which is
now included in the player's fatal query.

The comparison uses the pinned SmartTube source: `VideoStateController` restores
saved positions through `ExoPlayerController.setPositionMs`; ExoPlayer defaults
to `SeekParameters.EXACT`. Its optional TextureView sync-seek workaround is a
separate behavior and is not implemented by this change.

Shipping resume/UI/request methods and the audio acceptance helper passed
ASan/UBSan tests. Actual host AVC decode starts at the first frame at or after
4.25 seconds; actual AAC and Opus decode starts within one input sample of
4.250001 seconds. Interleaved/native-style S16 and planar S16 tests verify sample
contents, copy-on-write behavior, timestamp inference and bounded missing timing.
Audio-only resume, refused seeks and nonzero container origins are covered.
The Opus fixture emits a demux/decode header warning during seeking; the asserted
decoded-frame checks still pass. Actual AVC/VP9 playback/drain regressions also
pass. Evidence: `.deps/accurate-resume-test.log` / `.result` (0) and
`.deps/accurate-resume-drain.log` / `.result` (0). Host FFmpeg is 8.x; the PS5
recipe remains 7.1.1. These results do not prove native Audiodec timing or sound.

The complete PS5 build, link, packaging and freeze passed: build
`prosperotube-c6ec8804_1010-0423`, 126 files, eboot SHA-256 prefix
`370b64741f7917ae`. Evidence: `.deps/accurate-resume-ps5-build.log` / `.result`
(0) and `.deps/accurate-resume-freeze.log` / `.result` (0).

The reserved-console case stopped at its first minimal FTP running-title check
because a title was running. No other service probe, upload, launch, input or
close followed; the owned lock was released. Evidence:
`.deps/accurate-resume-storage-reserved-console.log` / `.result` (1). The frozen
candidate remains undeployed and unlaunched. Lapy PR50 execution, durable storage
and real YouTube playback on PS5 remain pending.

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

- 2026-10-09 | startup log gate / PS5 build | bb5672a0 | pass: 211 objects, link, package and immutable freeze; 126 files, eboot prefix f9aca772ad4cf6e9 | .deps/startup-gate-ps5-build.log; .deps/startup-gate-freeze.log | storage acceptance
- 2026-10-09 | primary storage/startup | bb5672a0 | PS5, firmware unreported in this case | failed: helper apply_failed (7), startup stopped, exit(1) followed by SIGSYS | .deps/startup-gate-console.log; .deps/console/storage-startup-gate-bb5672a-primary/klog.txt | helper-stage diagnostics offline

Exact build `prosperotube-bb5672a0_1010-0206` reached main, 3840x2160 display,
IME and browser preloads, then returned helper status 7. The logging gate alone
does not resolve startup. Full upload/readback, the 130-second settle with a
second full readback, exact registration and a fresh idle check passed before
one launch. The harness stopped on the matching storage-failure breadcrumb;
no input, close payload, second launch or subsequent console connection was
sent. Case result 1; process terminal; owned lock absent. No storage-ready,
persistence, graceful teardown, post-failure service-health or playback claim.
The case metadata retained the earlier storage-integration wording; the source
change being tested here is the boot-logger gate. Cross-console comparison does
not isolate a root cause.

An earlier separate idle-first read-only discovery found the primary idle and
its existing global ps5log endpoint configured for console loopback. No logging
configuration was changed. The helper's stage logs still were not captured;
the next diagnostic must expose them without disturbing shared configuration.
Evidence: `.deps/lapy-log-discovery.json` and `.deps/lapy-log-discovery.result`
(0). Do not repeat this frozen candidate unchanged.

### Lapy PR #50 integration (2026-10-09)

Pinned `57fcc19b7f57e46c823d6f2a6c6233ebf3288079`, the GitHub API-verified head
of https://github.com/mpereiraesaa/PS5-Lapy-JB-Daemon/pull/50. The upstream fix
stops targets before raw kernel-pointer snapshots, checks their title identity
around the stop, and requires exact post-donor root-reference balance before
detach. Its unmodified ELV1 protocol still matches the client hash. The prior
pin and failed frozen candidates remain separate comparison artifacts.

Upstream six native ASan/UBSan cases and 53 Python tests passed; both existing
ProsperoTube elevation client regression cases passed. Evidence:
`.deps/lapy-pr50-native.log`, `.deps/lapy-pr50-python.log`,
`.deps/lapy-pr50-client.log` and corresponding `.result` files (all 0).

Both exact-title PS5 helper builds passed `-Wall -Wextra -Werror` and manifest,
protocol and ELF validation. Default helper SHA-256:
`3f26c687c0cc3a7d5047f2a0b7f0d80886e92ac49c59c301d03eeed8488e1640`.
Diagnostic helper SHA-256:
`bf1d7b1017a6c078b81c5267152dfc985389f4836d288c34351243de98a2b0e1`.
Diagnostic records use the upstream result-file option at
`/data/prosperotube/lapy-helper.log`; normal builds omit it. The full upstream
manifest is checked for the exact optional path and absence of a test-only
snapshot delay. The packaged manifest records the source and logging path.
Evidence: `.deps/lapy-pr50-default-build.log` / `.result` (0),
`.deps/lapy-pr50-diagnostic-build.json` and upstream helper build logs/manifests.

This is offline helper integration evidence. No console traffic occurred during
this update. A new complete PS5 build/freeze and guarded application case are
still required; PR #50 does not itself prove the cause or resolution of our
captured status 7 failures. Diagnostic installation must create the app data
directory before launch and retain helper logs. Shared logging settings remain
unchanged. No custom logging adapter is incorporated.

- 2026-10-09 | Lapy PR50 / complete PS5 package | ec358d07 | pass: 211 objects, link, package and immutable freeze; 126 files, eboot prefix 08748ab3bd47c1c2 | .deps/lapy-pr50-ps5-build.log; .deps/lapy-pr50-freeze.log | guarded application acceptance
- 2026-10-09 | Lapy PR50 / primary console | ec358d07 | no-run: first FTP connection refused before running-title check | .deps/lapy-pr50-console.log; .deps/console/storage-lapy-pr50-ec358d07-primary/milestones.txt | continue offline

Exact build `prosperotube-ec358d07_1010-0225`; package helper SHA-256
`bf1d7b1017a6c078b81c5267152dfc985389f4836d288c34351243de98a2b0e1`.
Build and freeze returned 0. The console wrapper returned 1 and is terminal.
The first console operation, FTP connection on port 2121, was refused; it did
not establish idle or busy state. No other service probe, app directory creation,
upload, launch, close, input or retry occurred. The owned lock was released.
The frozen candidate remains undeployed; helper-stage, storage, persistence,
teardown and playback acceptance remain pending. No fallback case was started
after this transport failure.

### Offline fixture chat sending (2026-10-09)

Generated HLS chat now accepts selected offline-account messages through the
shipping library and job endpoints. API tests confirm literal markup and Unicode
content, a single polling copy, identical direct/job repeat confirmations, and
rejection of Guest, foreign-profile, changed-intent and closed contexts. Injected
clock tests cover 200-character limits, invalid controls/surrogates, rolling
polls, 256 retained intents, expiry and profile-removal cleanup.

Focused fixture suite: six passed. Full companion Python suite: 174 passed,
with the existing Starlette TestClient deprecation warning. Evidence:
`.deps/fixture-chat-send-focused.log` / `.result` and
`.deps/fixture-chat-send-full.log` / `.result` (both 0). No console traffic or
real YouTube posting occurred. Native source and the undeployed PR50 package
remain unchanged; PS5 compose/review/send acceptance is still required.

### Buffered HLS seek after origin expiry (2026-10-09)

The shipping relay now retains bounded, session-owned copies of completely
downloaded HLS resources. HTTP tests cover exact full/range/suffix reads and HEAD
metadata after origin expiry, continued fresh root fetching, session isolation,
expiry, account-removal callback cleanup, size/count/staging limits, eviction,
incomplete lengths, encoded/partial responses and interrupted streams. Buffered
bodies revalidate session ownership and profile availability between 64-KiB
chunks, preserving account-removal retirement behavior.

The generated finite-HLS host probe decoded 8.0 seconds with audio and video,
retained four resources (977224 bytes), then used output seeking to 3 seconds and decoded
2.0 seconds after synthetic origin segments began returning 403. It made zero
origin segment requests during the second decode. Evidence:
`.deps/hls-buffer-decode.json`, `.log` and `.result` (0).

Full companion suite: 182 passed, with the existing Starlette TestClient warning.
Evidence: `.deps/hls-buffer-full.log` / `.result` (0). Focused relay suite:
26 passed (`.deps/hls-buffer-focused.log` / `.result`, 0). No YouTube or console
traffic occurred. Native source and the frozen PR50 package are unchanged.
Never-downloaded/evicted expired resources, real rolling live/DVR windows,
long-session continuity and PS5 seek/playback acceptance remain incomplete.

HLS sequence numbers alone cannot establish matching bytes across different
playlists ([RFC 8216 section 4.3.3.2](https://www.rfc-editor.org/rfc/rfc8216.html#section-4.3.3.2)).
This buffer reuses actual complete downloads rather than inferring replacement
media identity from matching sequence numbers.

### Input-seek proof and native seek ownership (2026-10-09)

The initial buffered-HLS probe used output `-ss`, which decodes/discards from
the beginning and did not establish a demuxer input seek. The probe now places
`-ss` before the input ([FFmpeg seeking options](https://ffmpeg.org/ffmpeg.html#Main-options)).
After eight seconds of initial decode and synthetic origin expiry, a 3-second
input seek decoded two seconds with zero origin segment requests. Its 59 video
frame and 95 audio-block fingerprints exactly matched a local input-seek
reference; the video differs from playback at the beginning. Clearing the
buffer caused the same request to fail with one rejected origin request.
Evidence: `.deps/hls-buffer-decode.json`, `.log` and `.result` (0).

The native UI previously updated the playback clock base before the seek was
accepted or completed, then announced seek begin after publishing the request.
A fast demux completion could therefore be followed by a late UI begin,
re-arming the hold without a remaining request. The generated overlay now
leaves clock commits and begin/end ownership on the demux thread. Both the UI
and direct request path reject nonfinite and excessive targets before side
effects; the existing duration clamp remains authoritative.

The extracted shipping UI method and demux request passed ASan/UBSan tests for
refusal, pending requests, fast completion before UI return, duration clamping,
inactive playback and invalid targets. Evidence: `.deps/seek-handoff.log` /
`.result` (0). Existing provider/playback-recovery and exact generated pthread
handoff regressions also passed (`.deps/seek-provider-regression.log` /
`.result` and `.deps/seek-recovery-regression.log` / `.result`, both 0).
A new full PS5 build remains pending at this source milestone. The prior frozen PR50 package remains
unchanged and does not include this native fix. No console traffic occurred.

### Seek candidate packaging and bounded installation cases (2026-10-09)

Source `00372796` completed the full PS5 build, packaging and immutable freeze:
build `prosperotube-00372796_1010-0258`, 126 files, eboot SHA-256 prefix
`ce25ebbfc8158d28`. Build and freeze evidence: `.deps/seek-ps5-build.log`
and `.deps/seek-freeze.log`, with both result files containing 0.

Both initial console cases verified all installed bytes twice, including the
130-second settling period, and established exact title registration. Each
final running-title check found a title running, so neither sent a launch,
input or close command. Evidence: `.deps/seek-console.log` and
`.deps/seek-console-primary.log` (terminal result 1).

After the user reserved the fallback console, a separate idle-first case
established idle state but found that the installed bytes no longer matched
the frozen candidate. It stopped without uploading or launching. The case
does not identify which file differed or why; its adapter also omitted the
FTP SELF command used by the installation verifier, so no corruption cause
is established. A separate fresh installation case then stopped on its very
first running-title check because a title was running. It made no uploads,
service probes, launch, input or close requests. Both owned locks were released.
Evidence: `.deps/seek-console-reserved.log` and
`.deps/seek-console-reserved-install.log` (terminal result 1 each).

This frozen build has never been launched. PR50 helper execution, storage,
persistence, native seek behavior and actual YouTube playback on PS5 remain
unverified. A future idle-first installation must restore and verify the
exact candidate before launch.

### Relative seek positions and absolute HLS timestamps (2026-10-09)

The generated finite HLS fixture begins at 1.445333 seconds. A host FFmpeg
`av_seek_frame`/packet-read reproduction of the previous native conversion
admitted the first video packet at relative position 1.854667 for a requested
3.25-second seek. Adding the common container origin to the seek and video
discard target admitted a packet at 3.354667 instead; packet decode order
permits this small forward difference. Audio passed its corresponding bound.
This establishes packet gating, not decoded frame timing or audible A/V sync.

The shipping helper now converts relative positions into absolute stream
timestamps and video discard targets. Audio discard comparisons subtract the
same common origin; separate stream start times must not become separate
clocks. Invalid conversion leaves the prior clock base intact and resumes a
previously playing scrub. The generated overlay applies this to the actual
demux and audio paths without modifying the upstream checkout.

Real HLS packet reads plus boundary tests passed ASan/UBSan, including
fractional targets, distinct stream starts, negative/missing origins,
invalid inputs/time bases and addition overflow. Evidence:
`.deps/seek-timeline.log` / `.result` (0). Existing extracted shipping UI and
request handoff also passed (`.deps/seek-timeline-handoff.log` / `.result`, 0).
Native provider and playback-recovery ASan/UBSan regressions passed
(`.deps/seek-timeline-native.log` / `.result`, 0).
The host uses libavformat 62.3.100; PS5 uses the existing FFmpeg 7.1.1 recipe.
Full PS5 packaging and hardware acceptance of this change remain pending.
No console or YouTube traffic occurred for this offline change.

Source `dd34eb58` subsequently passed the full PS5 build and immutable freeze:
`prosperotube-dd34eb58_1010-0325`, 126 files, eboot SHA-256 prefix
`ffde9983f34493af`. Evidence: `.deps/seek-timeline-ps5-build.log` and
`.deps/seek-timeline-freeze.log` / result files (both 0). No console deployment
or launch occurred. This package corrects in-place seeks, not initial resume.

### Initial resume origin and clock anchoring (2026-10-09)

Initial resume used another relative-to-stream conversion that omitted the
container origin. Its audio re-anchor also treated absolute first-packet PTS
as a relative UI position. The overlay now uses the common shipping timestamp
helper for initial seeking and subtracts the same origin when re-anchoring.
The existing intentional keyframe run-up is preserved; resume position follows
the actual landing rather than claiming an exact decoded start at the saved
position. Invalid conversions take the existing failed-resume path.

The extracted shipping initial-resume block and audio re-anchor passed a real
generated HLS seek under ASan/UBSan. Checks cover the requested absolute stream
timestamp, relative first-audio landing, one-shot re-anchor, refused seeking,
nonfinite positions and overflow rejection without a seek call. Existing
shipping UI/request handoff passed in the same case. Evidence:
`.deps/resume-timeline.log` / `.result` (0). Header linkage was also compiled
as C++ against host FFmpeg. Full PS5 packaging of this follow-up remains
pending at the source milestone; console resume/audio acceptance is unverified.

Source `3c3eb818` subsequently passed all 211 PS5 object builds, link,
containers, folder/PFS/ZIP packaging and immutable freeze:
`prosperotube-3c3eb818_1010-0333`, 126 files, eboot SHA-256 prefix
`7c3e112076128376`. Evidence: `.deps/resume-timeline-ps5-build.log` and
`.deps/resume-timeline-freeze.log` / result files (both 0). No deployment or
launch occurred. This package includes both seek and initial-resume origin
fixes. Actual decoded resume position, video-only resume anchoring, PS5 sound
and synchronization remain acceptance requirements.

### Stored SELF verification and current console installation (2026-10-09)

The first `3c3eb818` installation case established idle state but stopped at
`sce_module/libc.prx` upload comparison. Its adapter mistakenly toggled FTP
SELF mode twice on the upload connection. SELF selects signed stored bytes
versus their ELF view; it is not a filesystem elevation command. The adapter
therefore requested the wrong comparison view. This does not establish file
corruption. No launch, input or close command followed; the lock was released.
Evidence: `.deps/resume-storage-console.log` / `.result` (1).

The corrected adapter selects stored bytes exactly once per data connection,
after its separate minimal running-title checks. Extracted FTP-function and
command-sequence checks passed without console traffic:
`.deps/check-ftp-self-mode.log` / `.result` (0). A new bounded case then
verified all 126 uploaded files and compared them all again after the
130-second settling period. Registration passed, but the final running-title
check found a title had started. No launch, input or close was sent, and the
lock was released. Evidence: `.deps/resume-storage-stored-self-console.log`
/ `.result` (1). These installed bytes are verified; app execution, helper
diagnostics, persistence, clean teardown and YouTube playback remain pending.

### Video-only resume clock ownership (2026-10-09)

Initial resume now arms its anchor for video-only sources as well as A/V.
The video decoder's existing valid/non-discard frame branch re-anchors the
relative UI position when no audio stream is selected. A/V retains audio
ownership. Missing timestamps and discarded frames leave the pending anchor
intact; unreasonable or nonfinite positions cannot change the clock base.

Extracted shipping resume, video-anchor and video-clock code passed ASan/UBSan,
including real host AVC decoding of the generated video-only MP4. Checks cover
the actual decoded frame position, one-shot anchoring, A/V ownership, missing
timestamps, seek discard and unrelated origins. Evidence:
`.deps/video-resume-timeline.log` / `.result` (0). PS5 build and hardware
acceptance of this follow-up remain pending at the source milestone.

### Video-only PS5 package and fresh public YouTube check (2026-10-10)

Source `e52315dd` completed all 211 PS5 object builds, link, container conversion,
folder/PFS/ZIP packaging and immutable freeze: build
`prosperotube-e52315dd_1010-0354`, 126 files, eboot SHA-256 prefix
`60967ac1c01c1852`. Evidence: `.deps/video-resume-ps5-build.log` and
`.deps/video-resume-freeze.log` / result files (both 0). This package has
never been deployed or launched. The previous `3c3eb818` installation remains
verified but unlaunched by its bounded case.

A fresh Guest check resolved public video `jNQXAC9IVRw` through the shipping
companion and DASH relay, then host FFmpeg decoded AVC/AAC for 8.0 seconds
at input offset zero and 8.0 seconds at input offset four. Pairing, resolution,
audio/video probing, both duration checks and temporary-server cleanup passed.
Evidence: `.deps/first-video-current-public.json`, `.log` and `.result` (0).
No account profile, mutation, simulated expiry or console traffic occurred.
The probe now records attempted input offsets explicitly and labels VOD cases
without a positive offset as `vod_decode_without_seek`, avoiding a false seek
claim when metadata provides no usable duration. The updated probe was executed
successfully; observed offsets were exactly `[0, 4]`.

This is host service compatibility and build evidence. PR50 helper execution,
PS5 storage/persistence, real YouTube playback, decoded position, physical
controller behavior, audible A/V synchronization and teardown remain pending.

### Settled-install startup control and primary availability (2026-10-10)

A separate `3c3eb818` startup control reused the already-settled installation
without uploading. Fresh idle checks and declared services passed, followed by
an exact readback of every installed file and exact title registration. The
final prelaunch check found an unrelated title had started; no launch, input,
close or installation upload was sent. Evidence:
`.deps/resume-storage-reuse-control-console.log` / `.result` (1). The saved
ShadowMount delta confirms the unrelated start. The case used this committed
control intentionally; it does not test the newer video-only resume change.

A separate primary-console case for frozen source `e52315dd` stopped on its
first minimal FTP running-title check because a title was running. No other
service probe, directory preparation, upload, launch, input, close or retry
occurred. Evidence: `.deps/video-resume-storage-primary-console.log` /
`.result` (1). Both owned locks were released. These are no-run results;
neither establishes PR50 helper execution or native startup.
