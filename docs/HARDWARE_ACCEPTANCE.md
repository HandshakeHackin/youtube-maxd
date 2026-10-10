# Hardware acceptance — pending

- 2026-10-10 | accurate resume | c6ec8804 | reserved console, firmware unreported | no-run: busy at first FTP check; no upload or launch | .deps/accurate-resume-storage-reserved-console.log | Lapy storage/startup, then real YouTube playback

The accurate-resume candidate passed the PS5 build and host decoded-frame/sample
tests. It has not been deployed or launched; see [validation](VALIDATION.md).

The host companion's real public YouTube path passed credential-free AVC/AAC
and VP9/AAC cases on 2026-10-09 with video `jNQXAC9IVRw` at a 240p limit:
actual LAN-API pairing, resolution, separate-track DASH relay, host FFmpeg probe,
two-second audio/video decode and seek decode. Node 24.10.0, yt-dlp 2026.8.19
and yt-dlp-ejs 0.8.0 were used. Private result metadata is in
`.deps/public-avc-probe.json` and `.deps/public-vp9-probe.json`. No media URLs
or account credentials were saved in those reports. An unavailable public test
video returned the new allowlisted `video_unavailable` code with HTTP 404 through
the same real API. Resolver-error privacy/IPC regressions passed as part of the
67-test Python suite. Run `scripts/probe_youtube.py` for an explicit repeat.

These are host results. Real YouTube PS5 playback, pacing, sound, long-video
behavior, other qualities and live streams remain unverified. The native Lapy
candidate below is unchanged by this companion-only error-handling work.

Lapy storage candidate `1b74462d` passed native ASan/UBSan regressions, the
existing/resident/helper and mounted-data listing tests, and PS5 folder/ZIP/PFS
build inspection. Its frozen folder has 126 files, eboot SHA-256 prefix
`02317fceecfb2f30`, and exact-title Lapy helper prefix `c894f5eb1084c5a3`.
App state, captions, caches and diagnostics use `/data/prosperotube`; verified
Lapy access and app-directory write/read/close/delete are required before app
services start. Transient persistence fallback and render-loop elevation
retries are removed. GPU/decoder preload ordering remains part of acceptance.

- 2026-10-09 | Lapy storage | 1b74462d | PS5 6.02 | no-run: a title was running at the first console operation; no probes, upload, launch or input followed | .deps/console/storage-1b74462/ | verified storage, relaunch persistence and playback

The candidate has not run on PS5. Verify native startup and saved state under
the exact durable directory across two clean launches, then rerun playback,
caption, controller and teardown cases. Earlier accepted sandbox builds do not
prove this new storage/elevation contract.

Standing user instruction: the first console operation of each new session is
a read-only running-title check, after checking the shared lock locally. Any
running title blocks the session, including this app. Continue offline; do not
close it or proceed with service probes, uploads, launch or input. Recheck idle
state before launch after an install.

- 2026-10-09 | Lapy | a07e1733 | PS5 6.02 | no-run: first title check busy; no probes/upload/launch/close/input; owned lock released | .deps/console/storage-a07e1733-second/ | storage acceptance

Candidate `800160c` passed the bounded startup and teardown case on PS5 firmware
6.02. Evidence identifies the matching build, the `youtube` document with eight
root entries, first-frame presentation checkpoints, `parked=1` after the app's
quit command, exact-title closure, runtime-layer release and healthy development
services. The shared console lock was released. This establishes those lifecycle
checkpoints; it does not verify video decoding, audible output, motion quality,
controller feel or live YouTube behavior.

The earlier candidate's absolute diagnostic paths were unavailable inside the
closed sandbox. The fix uses EVO's resolved data root and was verified while the
app remained sandboxed. An upload-only runner error interpreting FTP's successful
delete response was corrected before the accepted run; no title was launched by
that stopped upload attempt.

- 2026-10-09 | startup | bc3a686 | PS5 6.02 | inconclusive: app checkpoint; title cleanup passed | .deps/console/first-start-bc3a686/ | validate sandbox-aware paths
- 2026-10-09 | startup | 800160c | PS5 6.02 | pass: browser, frame checkpoint, graceful GPU park, release | .deps/console/sandbox-start-800160c-retry/ | fixture playback
- 2026-10-09 | provider | fe4a935 | PS5 6.02 | partial-pass: pairing, search, stream choices, teardown | .deps/console/fixture-avc-windows-fe4a935/ | direct playback
- 2026-10-09 | AVC | 59d68dd | PS5 6.02 | partial-pass: DASH opened, hardware AVC active, clock advances | .deps/console/direct-avc-59d68dd/ | full-duration playback
- 2026-10-09 | AVC | 675856b | PS5 6.02 | pass: bounded fixture video, scanout, 7.98/8.00s, teardown | .deps/console/complete-avc-675856b/ | captions, timing, VP9/HLS
- 2026-10-09 | captions | a42d689 | PS5 6.02 | no-run: initial check found a running title; no further console operations | .deps/console/caption-avc-a42d689/ | verify caption rendering

Candidate `a42d689` explicitly selects a downloaded caption sidecar as the
enabled primary track and disables inherited subtitle state when none is
available. Its real-engine host regression and EVO subtitle suite passed 72
checks with address/undefined-behavior sanitizers; the native provider suite
and PS5 build/package also passed. The frozen candidate has not been deployed:
the initial idle check blocked its console case. Caption rendering and timing
on PS5 remain unverified.

Candidate `675856b` passed the bounded AVC/AAC DASH fixture case: native UI
pairing/search, direct preferred-stream selection, a two-stream DASH open,
hardware AVC at 640x360, and progression to 7.98 of 8.00 seconds without a
reported decoder fatal. The inspected 3840x2160 scanout shows the generated
pattern and player UI. GPU park, exact-title closure, runtime release and
service health passed; the shared lock was released.

This is not audio/smoothness acceptance. The capture run's five-second timing
window reported 25.73 video frames/s against a 30 fps source and a 183.51 ms
maximum gap; a run without screenshot work is needed to isolate pacing. The
caption track loaded two cues but the OSD showed subtitles off, so activation
and timing need a separate check. VP9, HLS, seeking and live YouTube playback
remain unverified on this application.

The `59d68dd` runner printed a pass after observing raw EOF, but review found
that this is EVO's demux-exhausted flag. The clock had reached only 1.62 seconds
of an eight-second fixture; no scanout was captured. That output does not prove
full playback. The native overlay now also requires the clock to reach the
known duration before marking a video finished or advancing the queue. A
resolved duration is used if the player has none; unknown-duration completion
still needs a presentation-drain contract and must not be inferred from EOF.

Record firmware, loader/mounter versions, TV mode, source revision, companion
runtime/lock versions and artifact SHA-256 before testing.

1. Install the folder or `.ffpfsc` through your existing homebrew workflow.
   Verify title `PPSA99142` is free on your console before installation.
2. Launch once: verify launcher artwork, eight native root entries, focus,
   paging, Options keyboard, settings, Back and soft close. Close the parked
   app from the PS5 switcher.
3. Run offline fixtures, pair, search and verify three thumbnails. Restart both
   sides and verify pairing persists. Wrong/expired codes should fail cleanly.
4. Play AVC/AAC, VP9/Opus and HLS. Record frames, sound, synchronization,
   pause/resume, stop and resource use. HLS is finite media marked as live.
5. Seek non-live fixtures. Check chapters, captions at 0–2 and 4–6 seconds,
   caption toggle/picker/delay, aspect and stats controls.
6. Favorite, queue and follow. Check persistence, partial-watch resume, watched
   marking, explicitly enabled ordered autoplay and empty-queue behavior.
7. Disconnect/reconnect LAN while browsing/resolving/playing. Cancel/reselect
   without restarting. Expire a session and retry using the saved ID/position.
   Automatic renewal is not implemented; do not report uninterrupted recovery.
8. In real mode, test public short/long AVC and VP9 SDR videos, channel/playlist
   pages, captions, a supported HLS stream and both SponsorBlock switches.
   Start with 720p, increase only with measured frame/audio evidence. Extraction
   alone is not playback proof.
9. Check logs/storage/title isolation and credential redaction. Repeat
   launch/stop/soft-close and confirm renderer/audio resources are released.

For native library UI acceptance, fixture mode provides two synthetic accounts
without enabling account networking. Select **Offline account one**, browse its
25 playlists and verify the next page. On a fixture video, open Touchpad account
actions; verify explicit confirmation and the result for rating, Watch later,
subscription and playlist changes. Create a private fixture playlist, add/remove
videos, rename it from its Touchpad menu, then delete it. Switch to **Offline
account two** and verify the changes are absent. Cancel a name prompt or switch
profiles before confirming and verify no change is submitted. R3 opens generated
comments: test top/newest, paging, the long-text reader and its reply thread.
Restarting the fixture server resets these accounts. Record these as synthetic
UI results, separately from any real-account acceptance.

With **Sync YouTube history** enabled, play a fixture for a positive duration,
stop it and reopen YouTube history for the same synthetic account. Verify its
resume position and isolation from the other account. Test zero-progress failure,
pause/resume, a run over 30 seconds with longer generated media, rapid video
changes, disabling sync and companion failure. Confirm an unconfirmed report
does not repeat automatically. Abrupt app termination may lose a final pending
report; measure the last confirmed position and lifecycle behavior. Real-account
history display and cross-device resume need a separate authorized live case.

Verify local profile isolation separately: favorite/follow/queue a fixture and
stop at a partial position in account one; switch accounts and confirm its local
lists are absent. Clearing account two's history/queue must preserve account
one's state. Switch accounts during playback and verify progress/completion
updates the original local profile without starting another profile's queue.
Restart the app and verify each profile's persisted lists. A version-1 state
fixture must migrate to Guest with its original local data preserved; unsupported
or malformed state files must remain unchanged. Keep copies of the test files
and distinguish this console-local state from YouTube account history.

The device authorization backend and native sign-in view are implemented with
offline coverage. Real authorization, TV account catalogs, account removal and OAuth
playback acceptance remain pending. Comment posting, casting, HDR, AV1 and standalone
SABR/token execution remain unfinished. The companion's cookie-profile account
APIs have offline tests but no live account acceptance. These gaps remain part
of the full-parity goal and require implementation and explicit acceptance cases.

Account removal now has offline implementation/coverage. On PS5, verify the
selected app-signed-in account is reviewed, submitted once, removed from the
refreshed profile list and replaced by Guest. Circle before confirmation must
retain it. Old playback links and authorization/job results must no longer work;
another account and Guest must remain usable, and the removed sign-in must stay
absent after restarting the companion. Check profile/source changes during the
review and pending job. Externally configured cookie accounts remain managed by
the companion configuration; console-local lists retain their previous owner.

The audio-tail revision `0af336d` needs a bounded hardware case with short AAC
media, a non-block-aligned audio tail, pause/resume near EOF, and seek after EOF.
Check audible tail output, natural completion, and failure behavior separately.
Host tests exercise the actual decoder/resampler and clock but mock AudioOut;
two silence submissions are an output-retirement assumption to verify on PS5.
Revision `5b98769b` implements explicit decoder and final-frame retirement gates
and completion without metadata duration. Verify `eof=1 complete=0` while buffered
media remains, followed by `complete=1` only after the final audio and video tail.
Test AVC and VP9, audio ending before video, pause near EOF, failed input and seek
after EOF. Native flush errors or GPU fence/flip timeouts must not advance the
queue. Host tests inject render retirement and mock Sony flush; these are not
hardware proofs of the new gates.

Device sign-in acceptance requires an idle-first console case using the paired
companion with its library worker enabled. Open Accounts & library > Sign in to
YouTube; verify the code and URL, automatic polling and stable focus. Test Circle
and explicit Cancel, expiry, source change during initiation and closing the
browser while a poll is pending. No canceled/late response may select a profile.
For successful authorization, the user must enter the displayed code on their
phone/computer, verify the authorized profile becomes selectable and restart the
companion to check persistence/refresh. Distinguish public initiation from real
account authorization. OAuth playback now has a TV-only adapter with offline
request/format/refresh coverage. Verify real account-only playback, captions,
TV signature/n challenges, profile isolation and post-refresh resolution before
claiming it works. A public TV probe returning LOGIN_REQUIRED proves no playback.

Comment-rating acceptance remains pending. In the offline fixture account, open
comments or a reply thread, select a comment, and press Touchpad. Check available
like/dislike/remove-rating choices, the full literal-text review, Cross submission
and Circle cancellation. A result screen must not resubmit, and changing profile
or companion source must invalidate old reviews. Guest and malformed capabilities
must produce no rating request. Repeat with a real designated test comment after
account authorization, then reload/read back its rating before treating transport
acceptance as a verified live outcome. Exercise expired references, timeout and
restarting the companion during a pending request without automatic resubmission.

Split live HLS now passes a public host case with AVC/AAC on the current NASA
Space Station stream. Verify the same transport on PS5 after the idle-first
guard: both audio and video, capture-free pacing, pause/resume, live-edge
recovery and a session longer than 30 minutes. Exercise rolling playlists and
seeking within the available live window separately. The host two-second decode
does not prove PS5 audio output, long-session signed-URL validity or DVR support.

Native live-chat acceptance requires an idle-first bounded case. Start with the
offline HLS fixture, open R3 chat, switch top/live mode, page and scroll a long
message while rolling updates arrive, then open ordinary comments and return.
Check selection/scroll retention when the message remains and fresh selection
when it leaves the snapshot. Leaving chat, setup, search, source/profile changes
and browser closure must stop polling and release the companion context. Repeat
with an available public live chat, reporting public service and PS5 rendering
separately. No posting, account authorization or replay is part of this reader
case; those require their own implementation and acceptance.

VOD URL renewal now needs PS5 acceptance through unchanged relay capabilities.
The host AVC/AAC and VP9/AAC probes use real public media with one local injected
403, fresh same-profile resolution, byte-identity checks, decode and seek. Test
the same bounded injection through PS5, then naturally expired links during a
long session. Check audio/video continuity, seeking and graceful failure when
identity changes. Live HLS paired-root renewal and controlled native restart
are implemented but require PS5 acceptance. Expired idle capabilities and
missing validators use the controlled reopen path rather than byte renewal.

The native input classifier distinguishes retry exhaustion and read/allocation
failures from natural EOF. Repeated premature `AVERROR_EOF` replies after the
demux retry budget must leave the video unwatched and the queue unchanged, even
after buffered audio and video have drained. Test this on PS5 with an interrupted
VOD and live stream, followed by stop/reopen and a clean natural ending. Host
classification and renderer tests cannot establish console recovery or sound.

Controlled restart now uses the native stop/resolve/launch lifecycle. Validate
the same video/profile/format and fractional resume position after a bounded
VOD interruption or expired idle capability. Validate live-edge restart
separately. Check caption visibility, audible audio and A/V synchronization,
pause until explicitly resumed, seek cancellation, browser cancel, account/source
changes, all three retry attempts, and that recovery leaves the queue entry
unconsumed. Missing/ambiguous formats and failed reopen must return control
without opening another video or replaying account mutations. All of these
remain hardware acceptance requirements.

- 2026-10-09 | recovery / Lapy startup | 17f1b904 | PS5 6.02 | no-run: first title check busy; no probes/upload/launch/close/input; owned lock released | .deps/recovery-console.log | continue offline, await idle console

Live HLS paired-root renewal passed a real public host decode after one locally
injected root expiry. Validate unchanged relay manifests, both audio and video,
and continuity on PS5 separately. Expired segments/keys/init objects require
controlled live-edge restart; full DVR and naturally expired long-session
playback remain unverified. No console traffic occurred for the host case.

- 2026-10-09 | recovery / Lapy startup | 17f1b904 | PS5 6.02 | no-run: current first title check busy; no probes/upload/launch/close/input; owned lock released | .deps/storage-current-console.log | continue offline

Native live-chat composition is implemented. With an explicitly selected account
and active chat, check L3 keyboard open/cancel, Unicode limits, full literal text
in review, Cross submission once, confirmation and Circle return. R3 mode change
and Touchpad comments must still work. Repeat scope changes, delayed callbacks,
ended chat, uncertain response and cancellation; none may automatically resend.
Use a fixture first. Actual account posting requires a separately reviewed
message and acceptance; offline tests have sent no real messages.

- 2026-10-09 | fallback availability | read-only | pass: idle title list; no other console probes or changes; owned lock released | .deps/fallback-idle.json | build frozen candidate, recheck idle before deployment

- 2026-10-09 | fallback storage/startup | 9a7609c8 | PS5, firmware unreported | failed: exact build entered main, Lapy helper apply_failed (7), startup stopped; system captured a coredump | .deps/chat-compose-fallback-console.log; .deps/console/storage-chat-compose-9a7609c8-fallback/klog.txt | offline Lapy/startup diagnosis

Installation, full byte readback after settling, registration and idle recheck
passed. One launch was sent. No input, close payload or second launch followed;
the owned lock was released. Writable app storage, persistence and post-failure
service health were not accepted. Do not retry these identical bytes. Diagnose
the helper apply failure and startup-failure lifecycle before another bounded
case; the fallback's firmware has not been established.

- 2026-10-09 | primary storage/startup with log gate | bb5672a0 | PS5, firmware unreported in this case | failed: exact build reached main/display/IME, helper apply_failed (7), startup stopped, exit(1) then SIGSYS; no input/close/relaunch, owned lock released | .deps/startup-gate-console.log; .deps/console/storage-startup-gate-bb5672a-primary/klog.txt | expose helper-stage logs offline before a changed candidate

The log gate has host regression and PS5 build coverage, but it did not restore
startup in this bounded primary-console case. Both complete byte readbacks,
registration and idle checks passed before one launch. The case stopped on the
matching storage-failure breadcrumb. Storage/persistence, graceful teardown,
post-failure service health and YouTube playback remain unverified. No identical
candidate retry. See VALIDATION.md for hashes and logging-discovery limits.

- 2026-10-09 | Lapy PR50 app candidate | ec358d07 | no-run: first primary FTP connection refused; running-title state unestablished; no directory preparation/upload/launch/close/input/retry; owned lock released | .deps/lapy-pr50-console.log | continue offline, candidate undeployed

Complete PS5 build and freeze passed for `prosperotube-ec358d07_1010-0225`:
126 files, eboot SHA-256 prefix `08748ab3bd47c1c2`. These bytes have not yet
been tested on either console. Storage/startup, helper stage logs, persistence,
clean teardown, service health and actual YouTube playback remain pending.

- 2026-10-09 | seek / PR50 candidate | 00372796 | PS5 | no-run: both installs verified twice; prelaunch checks busy | .deps/seek-console.log; .deps/seek-console-primary.log | await idle
- 2026-10-09 | reserved installation reuse | 00372796 | PS5 | no-run: idle, installed bytes differ; no launch | .deps/seek-console-reserved.log | fresh verified install
- 2026-10-09 | reserved fresh installation | 00372796 | PS5 | no-run: first title check busy; no probes/upload/launch/input/close | .deps/seek-console-reserved-install.log | offline work

The full PS5 package and freeze passed for build
`prosperotube-00372796_1010-0258` (126 files, eboot SHA-256 prefix
`ce25ebbfc8158d28`). No case launched it. All owned locks were released.
Firmware is unreported. Installed-byte mismatch has no established cause;
see VALIDATION.md for the verifier limitation. Storage, helper execution,
persistence, teardown and real YouTube playback remain pending.

- 2026-10-09 | seek/resume origin | 3c3eb818 | offline PS5 | partial-pass: full build/package/freeze; host shipping-code regressions passed; no deployment | .deps/resume-timeline-ps5-build.log; .deps/resume-timeline.log | idle-first storage/startup case

Frozen build `prosperotube-3c3eb818_1010-0333` has 126 files, eboot SHA-256
prefix `7c3e112076128376`. Its in-place seeks and initial resume use the common
container origin; the audio re-anchor returns to relative UI positions.
Host packet/clock assertions do not establish PS5 decoded position or sound.
This candidate has never launched. Startup/storage, persistence, actual YouTube
playback, A/V synchronization and clean teardown remain pending.

- 2026-10-09 | stored SELF verifier | 3c3eb818 | PS5 | no-run: upload comparison requested ELF view after duplicate mode toggle; no launch | .deps/resume-storage-console.log | correct adapter offline
- 2026-10-09 | corrected install | 3c3eb818 | PS5 | no-run: all files verified twice after settling; registration passed; final title check busy | .deps/resume-storage-stored-self-console.log | offline work

Both cases released their owned locks. No launch, input or close was sent.
The corrected case establishes stored bytes, not app execution. Current PR50
startup/storage, persistence, playback/audio and teardown remain unverified.

- 2026-10-10 | video-only resume | e52315dd | offline PS5 | partial-pass: complete package/freeze and host decoded-frame regression; no deployment | .deps/video-resume-ps5-build.log; .deps/video-resume-timeline.log | idle-first startup

Build `prosperotube-e52315dd_1010-0354` has 126 frozen files and eboot SHA-256
prefix `60967ac1c01c1852`. Fresh Guest YouTube AVC/AAC DASH host decode passed
eight seconds at offsets zero and four. This candidate has never launched;
the host result does not establish console playback, motion or sound.

- 2026-10-10 | settled startup control | 3c3eb818 | PS5 | no-run: fresh exact readback and registration passed; prelaunch title check busy; no uploads/launch/input/close | .deps/resume-storage-reuse-control-console.log | offline work
- 2026-10-10 | current primary candidate | e52315dd | PS5 | no-run: first title check busy; no other probes/uploads/launch/input/close | .deps/video-resume-storage-primary-console.log | offline work

Both owned locks were released. The settled control used the earlier committed
installation and unchanged PR50 helper; the current `e52315dd` package remains
undeployed. No startup, persistence, playback or teardown acceptance was added.
