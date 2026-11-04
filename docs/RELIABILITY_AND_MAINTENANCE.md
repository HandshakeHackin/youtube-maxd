# Recovery, channel preferences and maintenance

These additions extend the viewing features in [Everyday viewing](EVERYDAY_VIEWING.md). Included in v1.6.0. Open **Settings → Playback and maintenance** for the new controls. Family-profile PIN gates still apply.

## Channel-specific preferences

Open **Channel preferences**, then choose a channel already browsed or followed in this profile. Set its quality ceiling, playback speed, captions and volume leveling. Each choice starts at **Inherit**. Reset removes these overrides while keeping existing profile settings and legacy caption/speed memories.

Overrides belong to the actual profile, including when ordinary settings are shared. They affect future video resolves. A channel ceiling can lower, but never raise, the profile ceiling. Explicit channel caption/speed choices take precedence over the existing caption/speed memories; Inherit returns to their ordinary behavior. Volume leveling uses YouTube's loudness metadata when available and does not alter the profile's stored volume preference. It does not analyze audio or boost quiet tracks.

Up to 256 channel entries are retained. No Google/YouTube account settings are changed.

## Interrupted-session and queue recovery

The existing queue remains saved in its original order. Normal video progress now also records a profile-owned interruption checkpoint in the same atomic state file. A checkpoint left without a stop report creates a **Recover interrupted session** entry in the main menu and can be reviewed under **Interrupted session**.

Review the remaining queue or choose **Resume interrupted video**, then select the saved video to play it. Resume resolves fresh streams; it does not reuse saved signed media URLs. There is no automatic playback. The offer remains available until playback starts or you dismiss it. Dismiss keeps the queue, history and bookmarks.

Recovery uses the last successful save, ordinarily within the existing ten-second progress-save interval, with an immediate start checkpoint. Missing stop reports can result from a crash, forced close or restoring an active-session backup; the app does not claim to diagnose a crash. Normal stop reports clear the offer. History-disabled/incognito playback does not create checkpoints, and the existing short-video resume preference is respected. Live/DVR sessions are not offered as VOD resumes. Store failures are reported through the existing storage notices.

## Playback diagnostics

**Playback diagnostics** and **R2 → Playback health & rescue** show the last classified failure observed for the selected profile in the current app session: source/resolve request, video decoder, audio decoder, sustained buffering, or no supported resolved formats. The record is session-only and contains no displayed media URLs, credentials or raw resolver responses. Switching profiles does not show another profile's failure.

Suggestions distinguish retrying a source, selecting a different audio track, trying a lower quality, and using an available AVC format up to 1080p30 SDR. Actual rescue still requires reviewing and confirming the existing one-video recovery action. A category is an observation, not proof of a particular network, ISP or URL-expiry cause. Available alternatives depend on the formats already resolved.

## Quick Switch

While browsing videos, **hold Touchpad for about 0.65 seconds** to open Quick Switch. Short Touchpad taps keep their existing actions. Recording, typing, sign-in, modal and playback input keep their existing ownership; this shortcut does not replace the player's Touchpad controls.

Jump to Continue Watching, subscriptions, search, or the play queue. Afterglow opens its search screen. Classic opens recent searches; press Square there to enter a new query. Quick Switch is also available under Playback and maintenance. Family restrictions remain in force.

## Storage and cache

Storage shows the Afterglow thumbnail **RAM** footprint and ready-picture count. Clearing it releases that cache; visible artwork reloads on demand. This is not a disk thumbnail cache. It is unavailable before an Afterglow frame exists.

The separate temporary-caption control shows the size of `active.srt` and can remove that one derived file only while playback and requests are idle. It does not sweep directories. Profiles, credentials, bookmarks, queue records, backups, fonts and the voice model are kept. No device-wide storage cleanup or cache deletion outside this app is performed.

## Offline backup and restore preview

The existing **Backup and restore** page creates a dated JSON copy under `/data/youtube-maxd/backup`. The copy includes local profile preferences, family configuration, bookmarks, queue/history records, channel overrides and playback presets. The restore review now displays counts of the stored preference sets, records, favorites, queue entries, bookmarks, channel rules and presets before you choose Restore.

To move an exported backup, copy the complete dated JSON file from that folder using your normal file-transfer method. To import one, put the original, unchanged `youtube-YYYYMMDD-HHMMSS.json` file in the app's backup folder, reopen the list, review its preview, then select Restore. A safety copy of the replaced state is retained. Invalid, oversized or unsupported backups are rejected by the existing bounded parser. No upload or cloud service is involved.

Standalone YouTube sign-ins are separate and are not exported; sign in on another console as needed. A companion-based installation's full state backup includes its pairing token. Keep full backups private. The preview shows counts, not credentials or account names.

## Validation

Tests exercise real provider resolution ceilings, speed/caption selection, effective volume gain, clean-stop/interruption behavior, profile isolation, no-autoplay recovery, saved queue order, history privacy, cache controls and backup previews. The shipping Quick Switch input block is compiled and executed with UI stubs in addition to gesture tests. Menu fixtures are exported from actual provider responses. Physical PS5/phone/live YouTube acceptance remains separate from automated and build verification.
