# Feed controls and playback health

Development build features; these are not included in the published v1.4.2 download. Console acceptance is still required.

## Feed controls

Open **Settings → Feed controls** in Classic or Afterglow.

- **Mute a title phrase:** keep up to 16 literal phrases per local/account profile, each up to 96 UTF-8 bytes. Matching checks the full fetched original title and the available DeArrow title, even when the visible card title is shortened. ASCII matching ignores case; other UTF-8 text matches exactly. This is title filtering, not semantic topic recognition.
- **Remove a phrase:** select it in Feed controls. Reopen a browsing feed to apply the change.
- **Followed channels first:** prioritize locally followed/imported channels within each loaded Home/Discover page. Relative order within each group and folder headings are preserved. It does not fetch or reorder the entire YouTube feed, change search relevance, or infer all remote Google subscriptions.
- **Blocked channels:** links to the existing profile-owned channel block manager. A video's actions menu provides the existing Block channel action.

Title muting applies to browsing feeds, search, channel videos and subscription feeds. Saved local history, favorites and queues, plus YouTube history, Watch later and saved playlist pages, remain available. It changes local display preferences and sends no YouTube feedback. Preferences remain separate even when general account settings are shared.

If title muting empties a Home page, the app follows continuation cursors, with at most eight extra page requests. A fully muted final page links back to Feed controls. After the scan limit, the normal next-page cursor is retained. Old rule-action rows and prompts cannot change a different profile.

## Playback health and rescue

While a YouTube video is playing, open **R2 → Playback health & rescue**.

The panel shows the current resolution, frame rate and codec, Auto Max state, and recent playback samples: collecting, stable, sustained buffering or dropped frames. Pauses, seeks, live playback, background playback and counter resets do not count as performance evidence. These observations describe symptoms; they do not diagnose your router or Internet connection. The panel is a snapshot when opened, not a live network speed test.

Two explicit rescue actions are available:

1. **Try a lower quality:** choose an available lower resolution within saved format limits.
2. **Try AVC compatibility format:** choose an available AVC stream up to 1080p30 SDR, within the saved resolution ceiling. This may override the saved codec/frame-rate preference for this video only.

Review the action and press Cross to confirm; Circle cancels. The existing scoped reopen path retains the position, selected audio and captions. The rescue pins the session's format so Auto Max does not immediately undo it. Saved preferences stay unchanged. Rescue requires a current video on demand and a suitable resolved alternative; live streams, stale selections and phone-owned playback are refused. If no suitable stream exists, the app reports that instead of inventing one.

Health observations are session-only and are not uploaded. Existing automatic stream refresh, bounded decoder/error recovery and optional Auto Max remain in place.

## Software validation

- Four new checks cover provider feed controls, muted-page pagination, bounded preference persistence, and playback health/rescue.
- Existing Auto Max, channel blocking, stream recovery and independent library paging regressions pass.
- Native provider checks use ASan/UBSan and queued HTTP fixtures. These do not establish live Google/YouTube or PS5 hardware acceptance.
- PS5 interface compilation and package validation are recorded in the pull request.

## Console acceptance

Verify both interfaces; create/remove phrases in two profiles; browse multiple Home pages; keep muted videos accessible from saved lists. During a video, open R2, cancel a rescue once, then confirm each available rescue and check position, audio and captions. Confirm that saved quality preferences still apply to the next explicit video, and that Auto Max does not override the manually rescued format.
