# Hardware acceptance — pending

Standing user instruction: the first console operation of each new session is
a read-only running-title check, after checking the shared lock locally. Any
running title blocks the session, including this app. Continue offline; do not
close it or proceed with service probes, uploads, launch or input. Recheck idle
state before launch after an install.

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

Device authorization, comment posting, casting, HDR, AV1 and standalone
SABR/token execution remain unfinished. The companion's cookie-profile account
APIs have offline tests but no live account acceptance. These gaps remain part
of the full-parity goal and require implementation and explicit acceptance cases.
