# Hardware acceptance — pending

The first PS5 candidate reached the native browser and reported frame
presentation. Acceptance remains incomplete: its absolute diagnostic paths
were unavailable inside the closed sandbox, so automated graceful shutdown
could not be verified. The title-aware controller closed the test instance;
runtime release and declared service health were confirmed. The next candidate
uses EVO's resolved data root for diagnostics and development commands.

- 2026-10-09 | startup | bc3a686 | PS5 6.02 | inconclusive: app checkpoint; title cleanup passed | .deps/console/first-start-bc3a686/ | validate sandbox-aware paths

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

Device authorization, native account-action UI, casting, HDR, AV1 and standalone
SABR/token execution remain unfinished. The companion's cookie-profile account
APIs have offline tests but no live account acceptance. These gaps remain part
of the full-parity goal and require implementation and explicit acceptance cases.
