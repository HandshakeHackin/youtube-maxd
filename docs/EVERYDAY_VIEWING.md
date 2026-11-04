# Everyday viewing improvements

Included in v1.6.0. Physical PS5 and phone testing is still required.

## Continue Watching

The Classic main menu has a dedicated **Continue Watching** list. Afterglow's existing Home shelf now reads this list, so completed history entries cannot crowd unfinished videos off the first page. Entries show saved progress and resume through the existing player path. Finished/live videos are excluded, and each profile has its own positions. Existing incognito/history-off behavior and the setting for remembering short videos are respected. This is local PS5 history, not cross-device Google history sync.

## Subscription catch-up

Open **Settings → Subscription catch-up**, create a queue, and edit its rules. Choose up to eight channels from local follows and subscription video metadata already loaded on the PS5. With no explicit selection, all subscription channels are eligible. Follow or browse a channel first if it is missing from the picker.

Rules start with unwatched videos, known Shorts hidden, live/upcoming videos excluded, and a 30-minute budget. Adjust the budget (15–120 minutes or no limit), Shorts option, and existing title/duration filters. **Open playlist** previews matches; **Append matching videos to queue** keeps existing entries and adds each new video once without starting playback. The budget covers newly selected matches, not time already in the play queue.

This reuses the signed-in subscription feed. A Google/YouTube sign-in is needed for a real subscription feed. It examines at most eight pages and 128 matches; it does not promise to scan all uploads. Shorts filtering requires an explicit Shorts marker. Channel selections, rules and queued videos remain profile-owned, including while ordinary preferences are shared. Changing profile or rules during a scan cancels its result.

## Phone keyboard

The existing Phone remote now labels its search form **Phone keyboard**, offers the phone's search/submit key, and explains the send flow. Blank and oversized UTF-8 searches get useful feedback before being sent. Drafts survive failed requests and are cleared only when their search is accepted. Newer drafts remain intact. It uses the existing opt-in, paired LAN remote and does not expose account passwords or replace arbitrary PS5 system keyboard dialogs.

## Sleep timer

Use **Settings → Sleep timer** or the R2 playback options. Choose an elapsed-time timer (15/30/45/60/90/120 minutes) or **When this video ends**. Input does not restart the elapsed-time timer. Expiry pauses playback; end-of-video sleep suppresses repeat, autoplay and the next queued video once. Changing the video cancels an end-of-video timer. Changing profile or restarting the app clears session timers. Timers do not suspend or shut down the PS5.

The existing inactivity timer remains available separately. Selecting one timer type cancels the other. **Turn sleep timer off** cancels both. The saved inactivity preference is turned off when selecting a session timer. Pausing rather than discarding playback preserves the current media and queue.

## Playback presets

**Settings → Playback presets** has Movies (up to 4K/30), Gaming (up to 1080p/60), and Listening (up to 720p/30) slots. **Save current preferences** stores quality ceiling, FPS ceiling, codec, speed, audio/caption languages, caption preferences and Auto Max. **Use this preset** applies those preferences to the next resolved video. Available formats still depend on the video; Listening is not an audio-only stream mode. Reset removes the saved override for that slot. Unsaved defaults retain current caption/language/Auto Max choices. Saved slots belong to the actual profile even with shared ordinary settings; applying a slot changes ordinary playback preferences under the existing sharing setting. The current video is not reopened automatically.

## Accessibility

**Settings → Accessibility** groups interface text size (100/115/120%), high contrast, reduced motion, caption backgrounds (transparent/translucent/opaque black) and the existing caption-size selector. Classic uses the existing scalable layout; Afterglow scales shaped text with consistent measurement, fitting and wrapping, capped at 120%. Its baked-font fallback retains its existing sizing if the text engine cannot start. Reduced motion disables Classic decorative CSS motion and uses Afterglow's existing reduced-motion path. Accessibility follows the existing separate/shared account-settings choice. Caption preferences remain adjustable independently.

## Validation

Provider regressions exercise actual progress, completion, profile isolation, preset persistence, stale actions, caption controls, subscription scanning, channel/Shorts/watched filters, bounded sessions, duplicate-free queue append and cancelled callbacks. Timer tests use a deterministic clock; phone tests execute the actual page script. Cross-compilation and package checks are separate from physical console acceptance.
