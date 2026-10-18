# Smart playlists and Auto Max quality

These features run entirely on the PS5. Neither needs a companion computer or
a developer Google Cloud project. This integrated build still needs acceptance
on firmware 11.00 / ShadowMount+.

## Smart playlists

Open **Smart playlists** from the main sections or YouTube preferences. In
Afterglow, use **Library → Smart playlists**, or the Smart playlists settings
tile. Choose **Create smart playlist**, enter a name and edit its rules:

| Rule | Choices |
| --- | --- |
| Source feed | Home, Subscriptions, Favorites or Play queue |
| Title contains | A phrase, or blank for any title |
| Creator contains | A creator-name phrase, or blank for any creator |
| Duration | Any, 0–10, 10–30, 30–60 or at least 60 minutes |
| Unwatched only | On by default; uses account and local watched markers |
| Hide live videos | On by default; upcoming videos are always excluded |

All rules must match. For example, a Favorites playlist with title **gaming**,
duration **10–30 minutes** and **Unwatched only** finds matching unwatched
favorites. It does not infer categories or identify every gaming video.
Phrase matching ignores ASCII letter case; other Unicode characters match
exactly. Duration limits include their endpoints. A duration rule excludes
live videos and videos whose duration is unknown.

Choose **Open playlist** to refresh its results. Each refresh examines up to
eight source-feed pages and retains up to 128 matching videos, without duplicate
video IDs. Results appear in pages of 24. This is a bounded view of that feed,
not an exhaustive scan of a channel or your whole YouTube library. Home and
Subscriptions use the existing account/feed availability; Favorites and Play
queue use the current profile's local records. Existing channel blocks apply.
Refresh can take longer than opening an ordinary local list; a scan expires
after 60 seconds and can be retried by reopening the playlist.

Rules are saved locally in `youtube.json` under `/data/youtube-maxd`. Each
profile owns its rules even when ordinary preferences are shared. The limits
are 16 playlists per profile and 64 across the device. Removing a saved account
also removes its local rules. Backup/restore includes them with the app's other
settings. Deleting a playlist asks for confirmation.

Smart playlists do not create or modify YouTube playlists. With the existing
**Use current section as playlist** setting enabled, playback uses a snapshot
containing only the matching videos. Refreshing results does not change the
snapshot currently playing. Switching profiles or leaving a loading screen
retires the pending result, so it cannot populate another profile's screen.

## Auto Max quality

Auto Max is **off by default**. Enable **Auto Max quality** in YouTube
preferences; Afterglow also exposes it in the Player settings tile. Keep
**Maximum quality**, frame-rate limits and codec preferences at your desired
limits. Auto Max uses the formats actually returned for each video.

It monitors playback after ten seconds of warm-up. It can choose a lower format
after three seconds of buffering within an eight-second measurement window,
or after at least eight percent of frames are dropped across eight seconds
with at least 48 frames observed. It favors a lower estimated decoding load
when frames are dropped. After 90 seconds of stable playback it may try a
higher format within the saved limits.

Successful changes have a 30-second cooldown, with at most six changes per
video. A format that exceeds the buffering or decoding limits learned for the
current video is not repeatedly retried. This learning resets for a new video;
it is not a saved console benchmark or a network speed estimate.

Manual per-video format selection and **Choose stream before playback** take
priority over Auto Max. Pause, seek, live streams, music mode, background
playback, phone-owned playback and an active recovery also suppress adaptation.
Changing profiles retires the old playback scope. Counter resets and gaps in
the app loop are discarded rather than treated as poor playback.

A change reopens the selected format through the existing recovery path at
the current position, retaining selected audio and caption state. A short
interruption or rebuffer is possible. This is not seamless segment-level ABR.
It cannot guarantee smooth 4K or software-decoded AV1 on a PS5.

## Artwork and lighting

The approved launcher icon and matching DualSense effects use red `#FF304F`,
soft red `#FFA5AA` and white `#FFFFFF`. Playing uses dim red; pause uses dim
white. Menu, loading, sign-in and voice states animate within that palette.
The existing controller feedback switch disables these effects.

These are controller effects. Console chassis LED control is not implemented.
Community documentation describes blue, white and amber console channels,
with mixing and effects, rather than a freely selectable RGB light:
[PS5 Suite](https://github.com/manos555555/PS5-Suite) and
[ps5-signal](https://github.com/Marice/ps5-signal#what-the-console-allows).
True red console output and firmware 11.00 compatibility have not been verified.

## Console acceptance

Test rule creation, editing, deletion, restart persistence and profile changes
in Classic and Afterglow. Verify that unwatched results update after a video
finishes, and that next/previous stay inside the playing filtered snapshot.
Check source feeds with and without a signed-in account, slow requests, leaving
the screen during a refresh and the 128-match limit.

For Auto Max, watch native frame/buffering diagnostics during sustained
network pressure and decoding load. Confirm position, audio and captions after
each switch, the saved quality/FPS/codec limits, manual overrides, live-stream
suppression, cooldown, profile changes and recovery failures. Validate the
physical controller colors and cleanup on exit separately.
