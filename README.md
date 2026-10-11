# ProsperoTube 0.1.0

A native PS5 homebrew YouTube client inspired by
[SmartTube](https://github.com/yuliskov/SmartTube), with an original TV interface
and service provider built on [EVO Player](https://github.com/sainsaji/EVO-PLAYER-PS5).

The first version uses a **required user-controlled LAN companion**. This is a
native-client implementation of the SmartTube experience, not a binary port of
the Android application. No SmartTube or MediaServiceCore source is incorporated.
See [implementation decisions](docs/DECISIONS.md) and the
[original plan](IMPLEMENTATION_PLAN.md).

The active [goal](GOAL.md) is full SmartTube equivalence on **PS5**. This
prototype does not yet satisfy it. The player uses the pinned EVO PS5 foundation;
ProsperoLight and ProsperoTV are integration references.

Hardware status: the PS5 build has played the real public video "Me at the zoo"
through the LAN companion, with native AVC, rendered English captions and
saved Guest history. Lapy PR50 storage access and graceful GPU park/close passed.
Earlier PS5 packages also reloaded Guest history and pairing, paused/resumed,
and completed a ten-second seek with the native engine reporting success.
The user also watched Big Buck Bunny at native 1080p60 AVC/AAC for over two
minutes and confirmed smooth video and synchronized sound; zero late drops and
clean teardown passed. Controller feel, broader formats and long sessions still
need acceptance. See
[validation](docs/VALIDATION.md) for the captured frame and retained verifier
failure caused by a too-strict final-frame threshold.

Implemented: paginated search, topic discovery, channel/playlist browsing,
thumbnails, local follows, favorites, history/resume, ordered queue and optional
queue autoplay with a cancellable five-second advance past unavailable videos;
phone playlists use the same delay, retaining sparse occurrence indices and
receiver ownership, with stop/new-playlist interruption and bounded exhaustion;
quality/codec/audio-language preferences; AVC/AAC and VP9/Opus
adaptive playback, progressive video, HLS, chapters, timed captions, optional
SponsorBlock category actions, optional paid-promotion notices (off by default),
minimum skip duration, channel exclusions and
rewind policy; nine-category timeline colors and highlight-point markers on the
PS5 UI kit and RML fallback; native player controls and diagnostics. R3 in the
player explicitly jumps to the next highlight, using silent/message/ask/disabled
actions; L3 retains screenshot capture. Highlight and pause-preserving seek paths
have offline coverage, with console acceptance pending. Cross/Circle SponsorBlock
confirmation and known-end skips have offline coverage, including queue/watched
state and extracted player paths; console acceptance remains pending. The optional
[library service](docs/LIBRARY_SERVICE.md) adds explicitly configured account
profiles, subscriptions/history/watch-later and account playlist browsing.
Native comments/replies and basic ratings, Watch later and subscription controls
and playlist editing have offline coverage; PS5/live acceptance remains
pending. R3 on a live video opens a recent-message chat reader, with top/live
mode switching, timed updates and literal full message text. L3 opens account
chat composition with full-text review and one-time confirmation. The reader
and send flow have offline coverage, including generated account fixtures;
real-account posting, complete scrollback and PS5 acceptance remain pending.
The player sidebar now supports playback-clock chat replay, including pause,
seek and rewind; service/native host checks and live Guest replay checks passed.
PS5 replay acceptance remains pending. The stationary replay reader now pages
through dense historical intervals: Triangle loads the next message page at the
selected time, and L1 / R1 seek by one minute and restart paging. Each response
keeps at most 256 messages and a bounded byte size; upstream continuations remain
private to the companion. Live chat and playback-synchronized chat keep their
existing rolling behavior.
The [phone receiver backend](docs/REMOTE_SERVICE.md) now supports authentic
YouTube TV-code pairing, typed commands and playback-state feedback. Offline
tests and a live temporary-receiver check passed. Native pairing settings and
Lapy-backed identity persistence now have host coverage. The native player adapter
implements queued launch, play/pause, fractional seek, next/previous, stop, volume,
D-pad input and caption language selection. The subtitle picker can switch
YouTube languages during playback using staged downloads and subtitle-engine
validation. Startup acknowledgments and queue/EOF
ownership have sanitizer coverage. Real phone/PS5 acceptance remains pending.
The optional [local speech-recognition service](docs/VOICE_SEARCH.md) passes real
whisper.cpp inference and API tests. PS5 microphone/search integration is pending.
The companion device-code backend now supports bounded polling,
cancellation and private account/refresh persistence. Native Accounts & library >
Sign in to YouTube now displays the device code/address and polls automatically;
Circle cancels pending sign-in. Its provider flow and layout have offline coverage,
with PS5/real-account acceptance pending. OAuth profiles now use a TV-only resolver
adapter and private token-refresh bridge, with offline format/pagination coverage.
Live account playback and TV signature challenges remain unverified.
Accounts & library > Remove selected account reviews a one-time removal of an
app-signed-in account. It clears the companion's saved credentials and account
requests/playback links, then returns to Guest. Local profile lists stay on the
PS5; externally configured cookie profiles are managed in companion configuration.
Optional DeArrow title/thumbnail replacement now has independent native preferences,
retains original metadata, and falls back to original images on service failure;
PS5 acceptance remains pending. AV1 SDR selection now uses the existing PS5 libdav1d software decoder and passes
offline relay/decode/seek tests; PS5 performance and presentation remain unverified.
Casting acceptance, HDR selection and standalone SABR/token execution remain unfinished.

Selected primary-account profiles now route search and playback through separate
credential/cache contexts on the companion. Secondary-account and brand-channel
playback mapping and live account acceptance remain unfinished. Playback history
reports now use the selected account, with a saved sync toggle, throttled position
updates and no retry after an unconfirmed report; see the library service.
Local favorites, queue, follows and resume/history state are now stored by profile.
Existing global state migrates to Guest. Hidden-Cross behavior, controls timeout and seeking mode
are saved per profile. Quality/codec, caption and audio languages, caption preferences,
speed policy and last-speed toggle, SponsorBlock policy/exclusions, DeArrow, paid-content
notifications, autoplay, stream prompt, history sync, chat and sleep timer now have
independent saved profile settings. Existing known profiles retain legacy preferences;
new profiles start with defaults. Companion pairing and phone identity remain device-wide.

**Validation boundary:** host tests and generated-media decoding verify offline
behavior. A bounded PS5 run passed native browser startup, pairing/search,
hardware AVC playback of an eight-second DASH fixture, an inspected scanout,
and graceful GPU shutdown. Broader format support, pacing, audio synchronization,
broader caption behavior and live YouTube playback still need the
[hardware checklist](docs/HARDWARE_ACCEPTANCE.md).
Format support is not a claim that arbitrary current YouTube videos or every
resolution will play on a PS5.

YouTube preferences and in-player playback options now offer four Cross behaviors
when controls are hidden: show controls, show and pause, pause only, or toggle
normal/last speed. When controls are visible, Cross retains play/pause. Each
profile can select Never or a 1–15 second controls timeout. Existing profiles
retain the previous Cross pause and 3.5 second timeout until changed; new
profiles default to show controls and three seconds. Buffering, seek, end and
stats overlays remain visible when needed. Speed selection still uses the
existing scoped memories and console-wide last-speed toggle. PS5 acceptance of
these preferences remains pending.

Seeking offers Regular, confirmation while paused, and confirmation while playing.
Regular seeking commits on D-pad release, with an idle fallback for lost release
input. Confirmation keeps the preview open until Cross commits or Circle cancels.
Commit, cancel and refused requests retain the previous pause state. Phone, voice
and sleep-timer pause requests retire the preview and enter the requested state.
Chat replay, phone position feedback and saved progress use the actual playback
clock while the timeline shows a preview target. Queue EOF processing waits for
preview/seek ownership to finish. These paths have offline coverage; physical
PS5 seeking, sound/motion and phone acceptance remain pending.

Seek intervals are saved per profile: 1, 2, 3, 5, 7, 10, 15, 20, 30 or 60 seconds.
Choose an interval in settings or the in-player options menu. Held seeking scales
the existing acceleration from that interval; existing profiles default to 10 seconds.

Maximum resolution is selectable per profile: 144p, 240p, 360p, 480p, 720p,
1080p, 1440p or 2160p. Frame rate can be Automatic, up to 30 fps or up to 60 fps.
An explicit frame-rate limit requires known stream metadata; Automatic also
permits streams that do not report their frame rate. These settings apply when
opening the next video. Stream labels show frame rate when available. Automatic
recovery keeps the playing video's original quality preferences and frame rate;
new settings take effect on the next video.

Time display is saved per profile. Choose total duration, remaining time (the
default), or estimated finish time in settings or the in-player options menu.
Remaining/finish estimates use the actual playback speed and seek-preview
position. Finish time uses the platform's local clock and updates while paused;
live streams show **LIVE**. Unknown durations show **--:--**.

Selecting a video plays the preferred quality directly. Enable **Choose stream
before playback** in YouTube preferences to show the available choices first;
this preference is saved independently of EVO's live-TV settings.

## Run the companion

In WSL/Linux, from this repository:

```sh
python3 -m venv .venv
.venv/bin/python -m pip install -r companion/requirements.lock
PYTHONPATH=companion .venv/bin/python -m prosperotube \
  --host 0.0.0.0 --public-base http://YOUR-PC-LAN-IP:8765
```

Install a supported yt-dlp JavaScript runtime on the companion. Deno is its
default; `--js-runtime node --js-runtime-path /path/to/node` selects Node.
The dependency lock includes EJS support. Extraction may additionally need an
explicitly configured proof-token provider; see
[yt-dlp's YouTube guide](https://github.com/yt-dlp/yt-dlp/wiki/Extractors#youtube)
and [PO token guide](https://github.com/yt-dlp/yt-dlp/wiki/PO-Token-Guide).
Pass extractor arguments using `--extractor-args settings.json`.
No browser cookies are read automatically. `--cookies file.txt` is explicit.

The companion prints a six-digit code, valid for ten minutes. In ProsperoTube,
press **Options**, enter `http://YOUR-PC-LAN-IP:8765#123456`, then search or open
a video to pair. Use the actual printed code. Pairing persists across restarts
while the companion token file remains intact. For WSL, configure LAN
reachability/firewall; a WSL NAT address may not be reachable by PS5.
Use a trusted private LAN. HTTP pairing and media are plaintext.

`--sponsorblock` enables optional lookup; also enable skipping in YouTube
preferences on the PS5. Both are off by default. Skipped categories are sponsor
and self promotion.

## Build and verify

See [BUILD.md](docs/BUILD.md) for dependencies, packaging, checks and offline
fixture mode. The application identity is `PPSA99142`; data lives under
`/data/prosperotube`, with verified Lapy access required at startup. Packaging produces a
folder title, ZIP and optional verified `.ffpfsc`. No console deployment runs.

## Controls

| Browser | Action |
| --- | --- |
| D-pad / Cross | Focus / open or watch |
| Square | Search |
| Circle | Back; cancel a pending resolve |
| Triangle | Toggle favorite |
| R2 / L2 | Toggle queue / follow the video's channel |
| L1 / R1 | Previous / next page |
| R3 | Open comments on a video |
| Touchpad click | Account actions, playlist edits, or ratings for a displayed comment |
| Options | Companion address and pairing code |

Search also accepts YouTube video/playlist URLs, `video:VIDEO_ID`,
`channel:CHANNEL_ID` and `playlist:PLAYLIST_ID` for direct browsing.

In comments, Cross opens replies, L2/R2 scroll the full text, L1/R1 page, R3
switches top/newest sorting, and Circle returns. Enable the companion library
worker for discussion browsing. With an account profile, Touchpad opens available
comment like/dislike/remove-rating controls. Review the comment and press Cross
to submit once; reload comments before another rating change. Comment posting and live/PS5 acceptance remain
unfinished.

With an account profile selected, touchpad click opens video ratings, YouTube
Watch later, and channel subscribe/unsubscribe controls. Each change shows a
confirmation for that profile. Reopening an operation screen does not submit it
again; an unconfirmed result requires checking the account before repeating the
change. These controls have offline coverage; live account acceptance is pending.

Video actions include adding/removing the video from an account playlist and
creating a private playlist with it. Touchpad click on an account playlist opens
rename/delete controls. Accounts & library also creates an empty private playlist.
Names are entered with the keyboard and reviewed before submission. L2/R2 scroll
the confirmation details; Circle returns without submitting.

During playback: Cross pauses, Left/Right scrubs, L1/R1 changes chapter,
Up toggles captions, Down opens the caption picker, R2 opens playback options
(quality, audio tracks and 0.25×–4× speed for on-demand YouTube videos),
L2 / touchpad halves adjusts caption timing, Triangle changes aspect ratio,
Square shows media information, Options toggles stats, and Circle confirms stop.
Settings can remember speed for all videos, each channel, each video, or only
the current playback. Channel/video memories are separate for each profile.
R2 focuses a normal/last-speed toggle when one is available; Cross applies it.
The remembered-speed list retains the 1,024 most recently changed overrides.
Choose an audio/caption language before opening a video; this version resolves
one selected audio track and downloads one preferred caption sidecar. During
playback, Down opens the subtitle picker with available YouTube languages.
Switching captions preserves playback position and pause; failed downloads
retain the previous track. Secondary YouTube captions remain to be implemented.
Live sources have no seekable timeline. Soft close releases resources and parks
the app; close it from the PS5 switcher afterward.

## Storage and privacy

Local lists are independent of Google, bounded to 384 cached records. State uses
an atomic JSON replacement and is saved periodically during playback. The
companion bearer token is stored in PS5 state and the PC token file; PS5 storage
encryption is not provided. Cookies, resolver headers and signed YouTube URLs
stay on the companion. Media capabilities expire after 30 minutes of inactivity;
delivered media and manifests renew this idle lease. Upstream signed URLs can
still expire independently. VOD HTTP/DASH tracks now resolve fresh links once
under the same profile after an expired-link response, preserving local relay
URLs and validating resumed byte identity. Terminal native input failures now
trigger up to three fresh resolutions/reopens per explicit video selection,
preserving the profile, format, fractional VOD position and caption visibility;
live playback restarts at the live edge. Paused playback waits until resumed.
Changed/ambiguous formats or an exhausted budget require selecting the video
again. This controlled restart still needs PS5 acceptance. Live HLS playlist
roots now renew together under the same profile, preserving relay URLs and
checking the audio-group pairing. Complete HLS segments, keys and initialization
objects are retained in a bounded session-owned RAM buffer, so revisiting their
exact bytes does not require an unexpired origin URL. Unbuffered expired objects
still use controlled live-edge restart; full DVR recovery is pending.
Access logs are disabled and native diagnostics redact
capabilities. See [protocol/security notes](docs/PROTOCOL.md).

GPL-3.0-or-later. See [LICENSE](LICENSE) and
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
