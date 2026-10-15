# Current implementation status

Latest installed candidate: `6fc6d58`, the production VP9 completion-identity
fix, also retaining the font-upload fix and physical PS5 keyboard support.
All 185 files matched after the 120-second stored-byte check; at installation
the title was idle and error records were unchanged. The diagnostic version completed the
same-input 36-second 4K24 sample through EOF: all 864 packets and sampled
visible pictures match the software reference, with zero association errors.
On 2026-10-15 the owner reported no noticeable hitching in the subsequent
manual replay viewed via RDP. Audio seemed in sync, but remote-viewing delay
made that difficult to judge. This is positive remote-viewing evidence, not
direct-TV or precise A/V-sync acceptance. App closure was not confirmed.
Sustained playback, physical keyboard input and post-playback glyph recovery
remain unconfirmed.

Follow-up `c1b67a5` adds decoder-renewal failure quarantine and bounded
refresh/HDR retries. Host sanitizer and mode-policy tests pass; production
and diagnostic ZIPs are built and verified but not installed. Reuse/control
and uninterrupted five-minute Afterglow network cases are prepared, not run.

## Earlier investigation evidence (superseded by the identity fix above)

The earlier `6599c48` Afterglow diagnostic matched all 1,576 sampled render
inputs to their decoded outputs; this establishes sampled source identity,
not complete rendered pixels or HDMI/Remote Play presentation. The subsequent `b9d3583` completed-buffer trace found no discordant sampled
source/output changes across 1,324 steady presentations, including 660 identical
same-picture repeats across the two buffers. This argues against steady stale
rendered content in that instrumented run, not full-frame or HDMI correctness.
See [handoff](HANDOFF.md).

The software reference now links all 63 distinguishably missing historical
pictures to hidden-plus-shown VP9 superframes. All 62 unambiguous backward
hash matches point seven reference pictures backward, usually recurring every
11?12 native pictures. The subsequent local replay verified all 813 accepted packet identities and
confirmed 59 stale same-buffer pictures; the decoder-association defect is real. Diagnostic `dee3753` adds packet fingerprints
and output metadata, but the bounded console replay failed to resolve the
resource before decoding. A local-input case then captured all three traces
with clean teardown and production restoration. No decoder fix or visual
acceptance is claimed. See [handoff](HANDOFF.md).

Returned metadata now isolates the FIFO error: the `77b77ee` identical-input
trace maps every real-timestamp published picture to correct software content,
while 61 stale outputs have the hidden-input timestamp sentinel and 61 real
pictures are incorrectly discarded as FIFO-hidden. The fix must use verified
completion identity. The tagged `3892445` replay verifies all 871 returned tags:
shown completions arrive after 3-4 calls, hidden completions after 15-16, and
all 811 shown completions preserve software display order. A public-picture-info
fix now passes generated-code sanitizer regressions, with duplicate/missing
timestamps preserved. Console build and full-sample validation are next; this
is not yet a visually accepted playback fix.

## Earlier playback evidence

The owner replayed the installed `5a65588` production candidate and reports
that the same 4K VP9 video still hitches. **Visual acceptance failed.** The
regular retirement intervals measured on the diagnostic build do not establish
regular visible content or explain this failure. The owner confirms smooth audio. An idle-only log read confirms
the production build, Afterglow interface, native 3840x2160 VP9/24, 608 published frames over
26.2 seconds, zero late drops and no decoder fatal. Earlier automated cadence
cases forced Classic and restored the original settings, so their workload
does not match this replay. The owner requires fixing forward with Afterglow;
no Classic comparison or interface change is requested. The next trace must retain Afterglow and record
all presentations, including repeated pictures omitted by the previous trace.
Reopen decode-to-display investigation;
do not release this candidate as a fix for the visible hitch.

Production candidate `5a65588` was installed with all 185 files verified
after 120 seconds and unchanged error records. Its later owner replay failed
visual acceptance as recorded above; it has since been replaced.

The final offline audit fixed background EOF return: only an automatically
issued end-of-video pause resumes on return, while deliberate pause and seek
commands retain control. The shared cancellation flag is atomic. Generated
regressions, sanitizer checks and independent review pass; see
[validation](VALIDATION.md) for the release-wide host-check record.

The `d26bfd0` presentation scheduler passed two independent .30 timing
replays of the owner's native 3840x2160/24 VP9 clip. Each steady 25-second
window changed from the baseline's 81 four-refresh / 440 five-refresh /
78 six-refresh intervals to 3 / 596 / 0. The remaining four-refresh interval
about every 8.34 seconds is expected for 24 fps on 119.88 Hz. Pictures remained
ordered, and raw VideoOut status corroborated CPU-observed retirement timing.
This fixes the measured cadence defect; physical TV smoothness and A/V sync
still require owner acceptance. Median fetch-to-retirement latency rose from
7.11 to 24.46 ms (about 17 ms).

The scheduler applies only to low-rate video on the known 120 Hz output mode;
other modes and faster playback retain the existing path. It preserves frame
leases and GPU fences, handles rounded CFR timestamps, and resets or recovers
across pause, seek, stalls and clock drift. Host checks cover 119.88 and true
120 Hz, delayed GPU completion and audio drift. A separate .30 control case
passed pause/resume and seek with 29 confirmed pictures in the new epoch;
saved state was restored, the console left idle and error records unchanged.
Two preceding case failures were private harness errors, with clean teardown.
The extended control case then advanced 30.08 seconds in a 30.06-second
post-seek watch, with 744 confirmed new-epoch pictures and clean teardown.
Seeking included about five seconds of decoder preroll and two seconds of
queue refill before continuous playback resumed. After that refill, 743
consecutive pictures had 735 five-refresh and seven four-refresh intervals,
with no longer gaps or retirement failures.

A final control run gave resume its own 16-second watch, then watched another
30 seconds after seek. Position advanced 16.04 and 30.04 seconds respectively;
1,458 pictures were published with zero reported late drops. Transient long
refresh gaps occurred only in the first three seconds after each transition.
After five seconds, resume had 297 five-refresh / two four-refresh intervals,
and post-seek had 617 / three, with no six-refresh intervals or retirement
failures. This establishes bounded recovery, not an instantaneous transition
or physical A/V acceptance. The console again closed cleanly and restored state.

The same candidate then passed a 300-second public-network replay of that
4K/24 clip: position advanced 299.77 seconds in 300.4 seconds, with 7,250
published frames and zero reported late drops. Saved files were restored,
services remained healthy, the title closed and error records were unchanged.
The bounded timing trace covers only the first 2,048 pictures (about 85 seconds),
while playback progress and final health checks cover the full run. Across the
steady trace there were 1,916 five-refresh intervals, ten expected four-refresh
corrections and no six-refresh intervals. This is bounded endurance evidence,
not attribution of the historical GPU faults.

Production `d26bfd0` is installed on .30: all 185 files matched after the
120-second stored-byte check, with idle title state and unchanged error records.
The diagnostic build supplied the timing/endurance evidence; the final binary
has no development controls or timing capture and was not launched after this
install. [Handoff](HANDOFF.md) records the ZIP path and exact hashes. TV motion
and physical A/V acceptance remain pending; no public release was published.

Local ZIP builds now apply the same permission normalization as CI before
exporting the exact application archive. A real-candidate check preserved all
entry contents and CRCs while setting the required archive permissions.

Presentation timing update: candidate `de96f13` completed a bounded .30 replay
of the same native 3840x2160/24 VP9 clip. All 790 traced pictures were ordered;
PTS remained 41/42 ms. Of 668 steady first-picture retirement intervals, 94
spanned four refresh ticks, 483 five ticks and 91 six ticks at approximately
119.88 Hz. Raw VideoOut status word 1 followed that refresh lattice and tracked
CPU observation with a 92-microsecond offset spread across the full trace.
The field's ABI is still unnamed: it corroborates uneven retirement cadence,
but is not an authenticated scanout timestamp or a TV smoothness verdict.
The presentation scheduler above addresses this measured defect.

Saved state was restored, the title closed, services remained healthy and error
records were unchanged. The preceding attempt stopped on a truncated-title
selector mismatch and also restored state cleanly. The diagnostic is opt-in;
production builds do not collect the timing trace.

Verified replay of `289e1c2`: the owner still reports picture hitches with
smooth audio. The executable and all 185 installed files matched after the
run. Native 3840x2160 VP9/24 playback published 590 frames in 25.3 seconds,
with zero reported late drops or decoder fatal. The title was idle and error
records were unchanged. These counters do not establish presentation cadence;
the picture-order corrections are retained, but they did not resolve the
reported symptom. The subsequent bounded timing run is described above.

First-release persistence fix: failures opening, writing, flushing, closing or
renaming the local state file now produce a visible notice through the shared
application toast. Failed saves preserve the prior file and repeated failures
coalesce until a successful save. Guest failure/retry and phone-provider tests
pass; this does not replace console acceptance of the notice.

VP9 packet-boundary update: the `f4aaba6` candidate was withdrawn before
console deployment. One split packet could return multiple older visible
pictures and overflow the five-slot reorder queue. Current source processes
split packets incrementally and resumes after receive frees a slot. VP9
output also preserves FIFO order for visible subframes sharing a timestamp.
The integration regression checks actual generated send/receive/decode/drain
and seek with the real queue limit: both the original overflow and the later
equal-timestamp ordering defect reproduce, and the combined fix passes.
Frame-lease tests still pass. The replacement production ZIP from `289e1c2`
built and passed package and frozen-manifest checks. After owner confirmation
that the prior title was closed, it was installed and all 185 files matched
after the 120-second settle check. The console remained idle; the requested
same-clip replay result is recorded above.

4K picture-order update, 2026-10-15: offline regression now reproduces a
VP9 pipeline bug that timing counters missed. At decoder depth four, the old
code filtered a returned picture using the current input's show flag, although
that output belongs to an earlier coded frame. Hidden alternate-reference
inputs could therefore suppress visible pictures and later expose hidden
pictures with otherwise regular visible timestamps. The regression compiles
the generated decode/drain code and checks picture identity as well as PTS;
the old code returns picture 10 where picture 9 / PTS 9000 is required.
The fix queues every successful VP9 coded input's PTS and display flag and
consumes that metadata for every output, including hidden pictures and EOF
drain. Seek clears the queue; ambiguous errors, overflow and incomplete drain
fail explicitly. The new regression and existing frame-lease checks pass.
AVC/HEVC association is unchanged. This was initially offline-only; the later
`289e1c2` console replay exercised the combined fixes and still visibly hitched.

The earlier audio-clock interpolation removed the 2048-sample stepping from
video scheduling but did not resolve the owner's visible hitch by itself.
The verified .30 production replay (eboot `90ed17f247d5bbd8`) ran native
3840x2160 VP9/24 for 34.8 seconds, publishing 807 frames with zero late drops;
the owner still saw hitches with audio in sync. Error records stayed unchanged.
Counters establish throughput, not correct picture identity or smooth TV
motion. Per-frame timing logging also perturbed an earlier diagnostic run;
CPU-observed flip completion is not a hardware scanout timestamp. The newer
bounded trace above supplies evidence for further presentation investigation.
The owner already confirmed audible audio,
TV-reported HDR and playback-menu transitions without blackouts on `b8d57c0`.

4K endurance update, 2026-10-15: the latest instrumented `964bb5c` candidate
passed a 600-second direct-LAN 3840x2160/30 VP9/Opus watch on .30, with two
-170-second rewinds landing within a second and three distinct nonblank 4K
captures. No playback stall, GPU fault signature or new error record appeared.
The title stopped, parked and closed cleanly; all ten saved-state files and the
owner's Afterglow override were restored. This strengthens bounded 4K evidence
but does not launch-test the ZIP-only production binary or identify the older
GPU fault address owner. See [hardware acceptance](HARDWARE_ACCEPTANCE.md).

Public live DVR update, 2026-10-15: candidate `964bb5c` passed a bounded
Guest playback of Bloomberg Business News Live (`QB5BNdBFujE`) on .30.
Native AVC 1280x720/29.97 opened, a -120-second command rewound the live
position 3591.58 to 3472.61 seconds, and frames resumed. After stop and an
18.4-second playlist-window advance, reopening the same public item restored
the saved UTC point within 2.61 seconds and frames resumed. The candidate
accepts timezone offsets in program-date-time, distinguishes discontinuity
metadata from a discontinuity, calibrates AAC packets with absent PTS, and
resets HLS timestamp-wrap references across rewind. Its one-launch teardown
restored saved state, left .30 idle with unchanged error records, and released
ShadowMount. This does not establish long sessions, high-resolution live DVR,
or physically audible sound. See [hardware acceptance](HARDWARE_ACCEPTANCE.md).

Playback update, 2026-10-14: a synthetic split-HLS feed passed live DVR
rewind, UTC position restore across a sliding window, and stale-point
rejection on .30. A held 4K HTTP 403 startup passed the provider
cancellation/join close fix without the earlier CPU crash. Automatic
frame-rate switching uses actual VideoOut state: first 30 fps HDR10 playback
changed the output from 60 to 120 Hz, Stop returned it to 60 Hz, and a direct
switch from active 30 fps playback to active 25 fps playback returned it to
60 Hz on .40. PQ, HLG and SDR selection each produced native frames and
nonflat scanout captures in bounded .40 runs.

A direct-LAN 3840x2160 VP9/Opus fixture ran 304 seconds to natural EOF with
4K seeks and no new GPU fault on .40. The revised HUI fence candidate repeated
304 seconds on .30 with rewind, forward seek, 9,421 published frames and one
late drop, then passed 26 Afterglow checks at 4K including artwork/navigation
churn and 12 seconds of idle. The full app and kernel logs showed no GPU page
fault, fence timeout or AGC error. A held artwork request on .30 no longer
blocked playback control: the job returned in 51 ms and playback began before
artwork release. Both consoles were restored, healthy and unlocked after their
final tests. See [hardware acceptance](HARDWARE_ACCEPTANCE.md) for candidate
hashes and exact limits.

The later public BBC Earth 4K/25 test on .30 advanced 150.14 seconds during
its first 150-second interval and landed a -60-second seek within 0.5 seconds
of the requested position. In the next 150-second interval it advanced only
24.07 seconds, with presentation falling to about 3.5 frames/s. The video
packet queue stayed near 120 while audio packets ran dry. A token-safe
diagnostic retest did not reproduce the sustained collapse; it showed about
6–7 seconds of seek preroll before audio/PCM resumed and clocks stabilized.
Both runs restored the nine saved-state files and left .30 healthy and idle.

The selected 4K DASH/WebM track reported a bogus 2 kbit/s rate, limiting its
video read-ahead to 8 MB. Commit `cd45171` keeps 4K video read-ahead at 48 MB
when that rate is implausibly low, while the unrelated core item still used
8 MB. Two separate public BBC 3840x2160 VP9/25 sessions on .30 then passed
the same 150-second playback, -60-second seek, 150-second playback plan.
Their first intervals advanced 149.97 and 149.84 seconds; post-seek intervals
advanced 138.88 and 144.85 seconds. Start and post-seek 4K scanouts showed
different footage. Neither run logged HTTP 403, `DEMUX_OVERSHOOT`, a GPU fault
or a new error record. Both completed exact nine-file restoration, healthy
service/idle checks and lock release. This addresses the reproduced bounded
post-seek slowdown, but two passes cannot establish reliability under every
public CDN/timing condition. See [hardware acceptance](HARDWARE_ACCEPTANCE.md)
for build hash, counts and evidence paths.

Historical unmapped GPU read addresses remain unattributed. The old 64 MB UI
direct-memory allocator could fall back to ordinary malloc for GPU descriptors
when fragmented, outside its usage counters. That fits the later fault address
range but is not proof of ownership; decoder-internal mappings also remain
possible. Current source rejects the UI GPU fallback and retains HUI textures
until GPU fences retire. The bounded runs do not establish reliability across
every public CDN/timing condition, long or high-resolution public live DVR,
TV-reported HDMI HDR metadata, or audible output. A ZIP-only production
package from `964bb5c` passed signed-container, Lapy helper, ZIP CRC and
embedded-eboot checks. Its 185-file folder was installed on .30 solely to
restore the title after the public DVR test; all 185 files matched after
settling, and the console remained idle with unchanged errors and its lock
released. This did not launch or playback-test the production build. Artifact
path and hashes are in [the handoff](HANDOFF.md).

Earlier 2026-10-14 investigation (superseded by the update above):
indexed WebM is enabled again after fixing FFmpeg's zero-byte
range-boundary read. A bounded PS5 Guest session passed
native VP9 1080p with AAC, seeking, pause/resume, history and Afterglow Home,
search and playback; [hardware acceptance](HARDWARE_ACCEPTANCE.md) records its
scope. Opus fixtures pass on the host; the offline VP9/Opus console result is
described below. Longer live 4K and physical sound remained open at that point.
The combined 93b2e0c7 candidate repeated the 1080p pass, but a second video
opened native 4K VP9 and hit a GPU unmapped-read page fault after its first
valid frame. Console state was restored and the title closed; 4K was unaccepted
at that point, and theatre/picker calibration was not reached in that case.
The native frame-pool lease fix and strict output checks pass offline. Three
subsequent bounded 2160p retests each passed native VP9 1080p and seeking, but
faulted before 2160p playback advanced. Diagnostic maps placed two faults in
retired 1080p decoder allocations even after queue-order and bounded close
drain fixes. A VP9-only resident 4K-capable decoder then reused one frame pool
for 1080p and full 3840x2160, eliminating that teardown path. Its first full
width frame reached the presenter, then a different GPU unmapped read outside
the decoder pools stopped playback. Every run restored saved state and left
the console idle; sound was not enabled after stalled frames. A subsequent
address-tracing build with unchanged playback logic opened full 3840x2160 VP9,
advanced position, and published 52 frames with no late drops or GPU fault.
It did not show a direct-pool fallback. The altered timing left the earlier
first-display fault unexplained. Later AGC fence, strict GPU allocation and
frame-boundary changes passed host checks; the no-swap frame-boundary fix also
eliminated command-buffer exhaustion on .40. An offline generated 64-second
3840x2160 VP9/Opus stream then played on .40 through native `sceVideodec2`:
1,920 frames entered and were published, with zero late/seek drops, decode
fatal or GPU faults. It reached 15/30/45/55-second checkpoints and the final
63.99/64-second terminal pause with natural EOF, audio drain, video decode and
display retirement gates true. Stage 6 parked and closed the app, restored
saved state, left services healthy and released the .40 lock. This proves one
bounded offline 4K/30 path, not HDR, physical audio, 4K seek/repeat or very
long sessions. A later bounded .30 Guest run opened the public BBC Earth item
through the built-in service and native `sceVideodec2` at 3840x2160 VP9/25,
advanced playback position by 11 seconds and published 315 frames with four
late drops (1.3%). The first Classic 1080p VP9 video and its +10-second seek
also passed. Afterglow returned after Stop; stage 6 parked and closed the app,
restored saved state byte for byte, found unchanged error records and healthy
services, and released the .30 lock. No 4K playback scanout image was captured;
the Home capture does not show playback, and presenter counters are the only
frame-display evidence. FFmpeg logged premature-end and earliest-resync
warnings in the WebM media stream and recovered. Transport and demux robustness
remain to validate. The 4K file discarded 253 frames as seek preroll while
settling at its saved 600-second position; these were not late presentation
drops. This single 24.3-second live session does not prove long live playback,
audible output, HDR or 4K seeking. The earlier
network first-frame faults remain part of the hardware history; their exact
address owner was not isolated.
Missing storage access now posts an explanatory system notification before
startup stops (package and startup checks passed; PS5 build passed; failure
notification not exercised on hardware). The owner selected homebrew.page's
signed catalog and helper for updates; the integration passes host checks
and a complete PS5 package build. Console acceptance remains pending. The dated
snapshot below has not been revalidated in full.

Afterglow's profile-password sheet now uses the shared typed-answer API rather
than discarding the input and calling a placeholder. All account boards pass
in the four looks; real profile-password use on the console remains pending.

The player now connects the existing next-video card to a five-second offer:
previewing preserves the selected queue/playlist/recommendation, Cross plays
now and Circle cancels. Automatic SponsorBlock skips with messages expose
five seconds of Undo after landing; Undo preserves pause and prevents an
immediate repeat skip. Final-segment skips offer Undo before completion is
committed. Prompt marks use their category colours. Provider and generated
player checks pass under sanitizers; console acceptance remains pending.
The options now group available choices and show their current values;
captions use primary/secondary tabs with selection, sync and size controls.
Generated picker input tests, boards and touched PS5 objects pass.
Chat now sits beside the picture in theatre mode; the existing video quad
follows the Player layout and resets when leaving it. Geometry, spring and
lifecycle checks pass; console calibration remains pending.

All 28 interface sound cues now use 34 original procedural WAVs. Reproducible
source, provenance, format, headroom, mixer and packaging checks pass; human
listening and console acceptance remain pending.
All 21 catalogs now cover every interface text and required plural form,
including integer-count labels. Native-speaker review and console language
layouts remain to check.

The Afterglow controller map and verified Keep watching default in the stop
dialog pass generated input/driver checks. Background audio while browsing
passes its generated ownership and presentation tests. Library queue Move
up/down actions preserve persisted order and reject stale receipts in native
tests; 47 Library boards pass in four looks. These changes still need console
acceptance.

Home's Live now shelf reuses the existing Live topic, loads only when reached
and filters archived videos out. Provider checks and 62 Home boards in four
looks pass. The shared-screen migration also passes its host boards and fixes
typed Settings/comment/reply delivery. These changes await console acceptance.
The notification bell reads YouTube's account-wide unseen count rather than
counting a cached page. Unknown counts hide the badge, and unsupported remote
mark-read actions are unavailable. Service, provider and board checks pass;
real-account console acceptance remains pending.

First-release work targets the standalone Guest alpha. Full SmartTube
equivalence stays the long-term target ([goal](../GOAL.md)); optional account,
phone and posting features are not evidence that the Guest release is ready.
The YouTube service runs inside the app, and bounded PS5 sessions now cover
Guest browsing, native playback, live DVR and parts of Afterglow. The current
results and remaining limits are recorded above and in
[hardware acceptance](HARDWARE_ACCEPTANCE.md). Real-account and phone tests
remain outstanding; reading comments is in scope, writing them is not required
for the initial alpha.

Three levels of evidence are kept apart on this page:

- **host-tested**: covered by the offline suite on a PC (`bash scripts/test_all.sh`);
- **checked on a console**: ran on a PS5, in the bounded cases recorded in
  [hardware acceptance](HARDWARE_ACCEPTANCE.md);
- **inferred**: built from what the pinned libraries and the reference sources
  suggest about the live service, with synthetic responses in the tests.

## First-release scope

Owner decisions of 2026-10-11, from the [goal](../GOAL.md): self-sufficient on
the console is a hard requirement; the first MVP is Guest, standalone.

- **In the release:** Guest search with filters, Home (a notice for a Guest,
  with rows leading on), Discover sections, channels, playlists, comments and
  chat read-only, playback with captions, quality and speed, local history and
  resume, favorites, queue, followed channels, SponsorBlock, DeArrow, the
  settings already implemented.
- **Added 2026-10-13, host-tested only:** accounts in the built-in service:
  sign-in by device code, account profiles, subscriptions, subscribed
  channels, history, Watch later, playlists, notifications, own videos,
  ratings, subscriptions, playlist editing, and the experimental chat and
  comment posting, account-history controls and feedback actions. Offered
  whenever the service's feature list names `device_auth`.
- **Added 2026-10-13, host-tested only:** in the built-in service, the
  subscriptions import (`subscriptions_import`) and the phone receiver
  (`remote`: TV-code pairing, the phone's commands, player-state feedback).
  Their rows are offered whenever the service's feature list names them. The
  receiver has met a local stand-in for YouTube's phone service only: **no
  real phone has been linked to this build.**
- **In the tree, inactive in the release:** voice search (no speech recogniser
  on the console; [assessment](VOICE_SEARCH.md)) and signed-in playback of
  videos that need the account (`account_playback`, off). Voice search still
  works against the development companion.
- **Updates, host-tested:** the signed homebrew.page catalog check and
  loader-started replacement helper ([updates](UPDATES.md)). Until a public
  release ZIP has a signed catalog entry, the check reports the app as unlisted;
  it does not query private GitHub releases. A complete console update remains
  unverified.
- **Added 2026-10-13, host-tested only:** the interface in 21 languages
  besides English ([localization](LOCALIZATION.md)): browser, preferences
  and messages follow the PS5 system language or the **Interface language**
  preference. The player (controls, in-player menus, pickers, keyboard) and
  toasts stay English: their fonts hold no accented letters.
- **Added 2026-10-13, host-tested only:** interface preferences per profile
  (interface and grid scale, card text, themes and classic colour schemes,
  the video actions menu, Circle rules) and the drawn seek preview.
- **Afterglow, implemented with acceptance remaining:** every screen factory
  now has an implementation, including search/results, channels, video details,
  Library, comments, Settings and account/phone screens. Host boards cover the
  four looks; bounded PS5 runs include Home, search, playback, navigation and
  artwork/texture churn. It is offered as **Interface: Afterglow (preview)**;
  Classic remains the default. Remaining player-layout, sound and screen
  calibration checks are identified above. The earlier all-screen acceptance
  [decision](DECISIONS.md) remains recorded; these bounded runs do not establish
  acceptance of every proposed screen or make Afterglow the default. The
  classic browser's Theme preference is separate from the interface choice.
- **Deferred:** the remaining interface options (buttons at the top of the
  main window, card previews, playlists as rows), a translated player,
  languages that need other fonts, shaping or right-to-left layout.

## The built-in service

- `standalone/`: QuickJS-NG 0.17.0 running YouTube.js 18.1.0 and the
  companion's library modules; a loopback listener with the companion's API;
  libcurl + OpenSSL with an embedded root list. Host-tested (20 engine and 131
  HTTP checks, plain and under sanitizers) and checked live as Guest.
- Subscriptions import: `POST /v1/subscriptions/import` takes the export file
  as its body, up to 1 MiB, on this one route (the others keep 8 KiB); the
  parser is the companion's, ported, with its test cases. Host-tested: 51 HTTP
  checks, plain and under sanitizers, and the provider end to end.
- Phone receiver: `POST /v1/remote` with the companion's request and answer
  shapes ([details](REMOTE_SERVICE.md)). The back channel is a long poll read
  as it arrives, which the HTTPS client gained for it (a streaming request
  with a silence limit; 114 more checks of the bindings on plain HTTP, the
  console's way of waiting and TLS). Host-tested: 272 HTTP checks against a
  stand-in for YouTube's phone service with a streamed back channel, plain and
  under sanitizers, and the provider with its driver end to end (TV code,
  commands in order, state report, off). Voice commands from the phone stay
  unsupported: the receiver never advertises a microphone.
- The provider starts it after storage is ready, uses its address and token in
  memory only, never pairs, retries from a visible row when it did not start,
  and stops it on the first soft-close frame. Host-tested against a stand-in
  and, end to end, against the real service in one process.
- Accounts ([decision of 2026-10-13](DECISIONS.md)): device-code sign-in with
  the companion's bounds, credentials in
  `/data/prosperotube/accounts/oauth-profiles.json` (plain text, atomic
  replace), token refresh, removal, every account list and change of the
  library modules behind one-attempt guards, history reports. Host-tested: 201
  HTTP checks against canned Google and YouTube answers, plain and under
  sanitizers, and the provider end to end (sign-in, profile, subscriptions, one
  change, removal). Every profile resolves videos as Guest; health says
  `account_playback: false`.
- The PS5 package compiled, linked and packed with the Guest-only service. An
  instrumented build of `6b3b1c41`, a tree that has the account code, **ran on
  a console once, as Guest** (2026-10-13,
  [hardware acceptance](HARDWARE_ACCEPTANCE.md)): the service's start,
  Discover, one resolve and direct playback. Accounts, the subscriptions
  import and the phone receiver have not run on a console.

## Updates

- `native/tube_update*.c` uses the pinned homebrew.page signed-catalog kit.
  It checks once per launch and on request, verifies the signed entry and
  artifact metadata, and offers notification, release-note and skip controls
  in About. GitHub hosts the ZIP but does not authorize an update.
- The packaged `self-updater.elf` starts through the loopback payload loader.
  It verifies and stages the ZIP, waits for the app to close, replaces entries
  in the existing folder installation and refreshes home-screen metadata.
  Cancellation before applying leaves the installed files untouched. Downloads
  restart on retry; the removed in-process updater's resumable-download,
  startup-count and rollback-journal behavior no longer applies.
- Host checks cover the pinned kit/helper, signatures, hashes, sequence replay,
  cancellation, installation refusals and replacement recovery, plus About
  integration and packaging. [Updates](UPDATES.md) describes the current
  recovery contract and console limitations. A public signed catalog entry
  and a complete console update/relaunch remain required for end-to-end
  acceptance; publication is the owner's decision.
- `.github/workflows/build.yml` builds version tags and manual runs and publishes
  tags as prereleases with `PPSA99142.zip` and `SHA256SUMS`. Local builds and CI
  both normalize ZIP permissions. Publishing a GitHub release alone does not
  create an in-app update offer.

## Implemented and tested

Everything below is in the tree and host-tested. "Implemented" does not mean
accepted on hardware or equivalent to SmartTube in every edge case.

- **Application:** native PS5 EVO playback, interface and decoder integration,
  Lapy storage under `/data/prosperotube`, packaging with the elevation helper.
- **Browsing:** search with options, suggestions and history; Home rows and
  filters; Discover sections including Trending, Shorts and Kids; channel
  sections, sort orders and search in channel; playlists; related videos;
  description, video details and share links with QR codes; menu sections and
  pins; channel blocks; hide-content switches; content language and country.
- **Playback:** AVC, VP9, HEVC and AV1 in SDR; adaptive, progressive and HLS;
  captions in two languages, caption picker, style, translated tracks and
  per-channel memory; quality, frame-rate and codec limits and presets; speed
  memories; seeking modes and intervals; chapters and chapter notification;
  sizing, zoom, aspect, rotation and mirror; volume, auto volume and audio
  delay; sleep timer; screen dimming and screen off; in-player video actions,
  button switches and a suggestions list; seven end-of-video modes; Next and
  Previous with playlist position; recovery and the error fixer; a video
  buffer choice (how far ahead a stream is read).
- **Local state:** profiles with favorites, history, resume, queue, follows and
  preferences (shared or per account); backups, automatic backup and
  subscriptions import; About with links, credits and changes.
- **Integrations:** SponsorBlock actions, review, exclusions and timeline
  colours; DeArrow titles and thumbnails; optional Return YouTube Dislike relay
  (off by default).
- **Accounts:** device-code sign-in and account removal; subscriptions,
  subscribed channels with new-content marks, notifications, history, Watch
  later, playlists and own videos; ratings, subscribe with notification level,
  playlist edits and order, stream reminders; comments with replies and rating;
  live chat and chat replay.
- **Experimental:** phone casting, voice search (development companion only),
  chat and comment posting, account-history pause/resume/clear, feedback
  actions and the profile password. All are host-tested, labelled **(experimental)** in the interface
  and inactive until the user turns them on or selects them.

On 2026-10-11 `bash scripts/test_all.sh` passed all 58 of its steps on the
frozen tree: fixture generation; the library worker build with its 189 Node
tests; 680 Python tests of the companion and tools; the 54 native provider
suites of `scripts/test_native.sh` under AddressSanitizer and
UndefinedBehaviorSanitizer with mocked HTTP; 44 further native, player-overlay
and interface scripts; and 10 Python source checks. The host interface preview
(`scripts/preview_native.sh`, not one of those steps) also passes.

### Parity tracker

[PARITY.md](PARITY.md) (generated from `native/parity/status/*.json`) compares
every audited SmartTube surface. **Done** there means implemented with offline
evidence, not console acceptance.

| Area | Done | Partial | Missing | Not applicable | Total |
| --- | ---: | ---: | ---: | ---: | ---: |
| Player and playback | 41 | 24 | 8 | 5 | 78 |
| Browsing, search and menus | 63 | 15 | 1 | 1 | 80 |
| Accounts, remote control and integrations | 20 | 7 | 1 | 1 | 29 |
| General, interface and maintenance | 26 | 16 | 10 | 6 | 58 |
| **All** | **150** | **62** | **20** | **13** | **245** |

Voice search moved from done to partial on 2026-10-13: the release has no
speech recogniser, so it exists only with the development companion. The
SponsorBlock alternative server had moved to partial for the same reason and
is done again: with the built-in service its address is typed on the console
and sent with the segments request. The built-in service also has the Return
YouTube Dislike relay now, asked only for a profile that turned **Show
dislikes** on; the dislike counts stay partial for where they are shown. The
web proxy stays partial: the built-in service has none, and nothing in its
menus offers one.

The following tracker counts are a 2026-10-13 snapshot and were not re-scored
after the 2026-10-14 playback work. The tracker's files were brought up to date
on 2026-10-13 for the built-in service, accounts on the console, the update rows,
the interface languages, the interface preferences and the seek preview. Of the 20 missing items, ten
need hardware work (auto frame rate, background playback, a remembered live
position, card previews), six need a Google sign-in with Drive access (cloud
backup) and four are not built: the player's interface switches, the network
settings group (it would be empty), buttons at the top of the main window and
playlists as rows (the browser has no shelf layout). The video buffer is a
setting now and partial: it sets how far ahead a stream is read, without
SmartTube's back buffer. The 62 partial items are mostly the English player and
the languages that need other fonts, shaping or right-to-left layout, states
and entry points the in-player list of actions lacks, interface preferences
that reach the browser but not the player, and what only the development
companion offers.

## Verified only on the host

The built-in service ran on a console for the Guest core path only (next
section); everything else that depends on it (which is every request the app
makes) is in this section. None of the following has run on a
console. Where an earlier check against the
live service exists it is named; everything else has seen fixtures only.

- Every account feature, now served by the built-in service. Sign-in runs
  against canned answers of Google's device-code endpoints; the account lists
  against hand-made answers in the shapes the pinned parsers expect, which may
  not be what the TV client really returns. No real account has been used with
  this tree.
- VP9, HEVC and AV1 playback: relay, decode and seek with host FFmpeg. PS5
  performance and presentation are open.
- Live chat and chat posting. A Guest chat-replay check against the live
  service passed during development; posting has only met fixtures.
- The in-player actions, suggestions list, chapter notification, screen
  dimming, presets and the error fixer.
- Backups, restore, subscriptions import, About and the QR pages.
- Kids, stream reminders, playlist order, notification levels, video details,
  likes and dislikes, comment posting and new-content marks.
- Phone casting with a real phone. The companion's receiver registered against
  the live service once, with a temporary receiver; the built-in receiver
  speaks the same protocol but has met only a local stand-in, and the cadence
  of YouTube's keep-alive frames against its 65-second silence limit is
  unobserved from this engine.
- Voice search with a real microphone (recognition was tested with real
  whisper.cpp inference on the host, through the development companion; the
  release has no recogniser).
- GPU presentation of zoom, rotation and mirror, long sessions and controller
  feel.
- Recovery when a media link expires during playback, the update check and
  self-update, the interface languages, the interface preferences and the
  drawn seek preview.

The PS5 package of the frozen tree compiles, links and packs (see the
2026-10-11 entry in [validation](VALIDATION.md)); that package has not been
installed or started. The build that ran on 2026-10-13 is a later,
instrumented one.

## Checked on a console

The self-sufficient build, in one bounded Guest session on 2026-10-13 with no
companion running: the built-in service's start, Discover and one topic
through it, a public video resolved on the console and played from YouTube
directly (H.264 1080p on the hardware decoder, AAC), a +10 s seek, pause, the
caption picker and playback options, Stop, the saved history record and a
clean park and close. Not assessed in it: sound and motion by a person,
captions on screen, VP9, AV1, HEVC and 4K, expiry recovery, accounts, phone
casting, updates, the interface languages, the interface preferences and the
seek preview.

Earlier development builds, with the companion, in bounded cases: startup with
storage access, pairing and search; a public video in hardware AVC with
captions and Guest history; pause, resume and seek; public 1080p60 AVC/AAC
confirmed smooth and in sync by the owner; and, with generated media and a
fixture service, resume, speed controls, the SponsorBlock review, the
unavailable-video queue advance, paused dual captions, the chat reader, artwork
decoding and phone commands from a test receiver.

## Inferred about the live service

These contracts in [the library service](LIBRARY_SERVICE.md) rest on layouts
that could not be checked offline. Each sends nothing and returns its stated
error when the layout is not recognised; each needs the live-service check.

- **Trending, Shorts, My videos:** the TV browse identities `FEtrending`,
  `FEshorts` and `FEmy_videos` come from the SmartTube service; their live
  layouts are unverified.
- **Channel sort orders:** recognised by position in a chip bar of two or three
  chips (newest, popular, oldest); any other layout reports no orders.
- **Account history pause, resume and clear:** the control is recognised by its
  menu icon (`PAUSE`, `PLAY`, `DELETE`); the icon names come from the web
  client's layout and were not verified against an account.
- **Hide a Subscriptions entry:** the token is recognised by the menu icon `HIDE`.
- **Stream reminder:** read from the raw `/player` response (one toggle button
  with add/remove upcoming-event reminder endpoints), which the pinned parser
  does not keep.
- **Playlist order:** read from the raw browse response; `playlistVideoOrder`
  1 to 5 is mapped to the five order names.
- **Comment replies:** confirmation expects `actionResult.status:
  "STATUS_SUCCEEDED"`.
- **New-content mark of subscribed channels:** read from the raw `/guide`
  response (`presentationStyle`).
- **Subscriptions import:** the NewPipe, PocketTube and GrayJay formats are
  taken from those applications' export files.
- **Dislikes:** Return YouTube Dislike as SmartTube's source is recalled, not
  read from the reference tree.

More generally, the third batch of contracts (Kids, reminders, playlist order,
details, comment posting, first search result, new-content marks, the proxy
option) has only run against the pinned parser and synthetic responses. Real
authorization, private-account feeds, live cookie validity, account playback
with TV signature challenges, and secondary-account or brand-channel mapping
are unverified.

## Remaining release gates

1. **Playback reliability:** `6fc6d58` fixes the proven VP9 completion-association
   defect: all 864 input packets and sampled visible pictures match the software
   reference through natural EOF. Longer runs and varied streams remain outside
   this bounded fix validation. The historical fault review attributes two
   addresses to retired allocations; later no-reset faults remain unattributed.
   These limits are not waived by the picture-identity result.
2. **Physical playback acceptance:** the owner confirmed audible sound,
   TV-reported HDR and menu transitions without blackouts on the earlier build.
   The presentation scheduler has bounded timing, endurance, resume and seek
   evidence above, but smooth TV motion and lip sync still need owner acceptance
   on the production candidate. The latest hitch report came through Chiaki;
   neither that observation nor diagnostic counters establish the TV result.
3. **Afterglow acceptance:** all screen factories are implemented and bounded
   console runs cover parts of the interface. The recorded all-screen console
   acceptance decision has not been satisfied or explicitly waived. Classic
   remains the default; screen calibration and physical sound checks remain.
4. **Publication:** the owner decides repository visibility and the first
   public release. A signed homebrew.page catalog entry must reference the
   released ZIP before in-app updates can be offered. An unlisted catalog
   result is expected before that entry exists; private GitHub API access is
   not part of the update check. A complete update/relaunch still needs console
   acceptance after publication.

Guest service and bounded core-path checks have passed, as have native VP9
4K endurance, public live DVR rewind/restore, automatic refresh switching and
HDR output checks recorded in [hardware acceptance](HARDWARE_ACCEPTANCE.md).
The frozen production ZIP and stored-byte installation are verified. Those
results do not establish every codec, CDN condition, long live session or
historical GPU fault's cause.

Experimental account use and phone casting still require an owner account or
real phone. They are not required engineering for the Guest alpha; writing
comments is also outside its required scope. Known limits include no playback
of account-required videos, no voice search, one YouTube client identity
(`VISIONOS`) with HLS fallback, an English player and unaccepted background
playback on the console. See the scope and evidence above before promoting
any experimental feature or changing Afterglow to the default.

See [validation](VALIDATION.md), [hardware acceptance](HARDWARE_ACCEPTANCE.md),
[parity](PARITY.md) and [goal](../GOAL.md).
