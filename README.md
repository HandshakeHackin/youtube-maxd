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

Implemented: paginated search, topic discovery, channel/playlist browsing,
thumbnails, local follows, favorites, history/resume, ordered queue and optional
queue autoplay; quality/codec/audio-language preferences; AVC/AAC and VP9/Opus
adaptive playback, progressive video, HLS, chapters, timed captions, optional
SponsorBlock, native player controls and diagnostics. The optional
[library service](docs/LIBRARY_SERVICE.md) adds explicitly configured account
profiles, subscriptions/history/watch-later and account playlist browsing.
Native comments/replies and basic ratings, Watch later and subscription controls
and playlist editing have offline coverage; PS5/live acceptance remains
pending. Device sign-in, casting, HDR selection, AV1 and standalone SABR/token
execution remain unfinished.

Selected primary-account profiles now route search and playback through separate
credential/cache contexts on the companion. Secondary-account and brand-channel
playback mapping and live account acceptance remain unfinished. Playback history
reports now use the selected account, with a saved sync toggle, throttled position
updates and no retry after an unconfirmed report; see the library service.

**Validation boundary:** host tests and generated-media decoding verify offline
behavior. A bounded PS5 run passed native browser startup, pairing/search,
hardware AVC playback of an eight-second DASH fixture, an inspected scanout,
and graceful GPU shutdown. Broader format support, pacing, audio synchronization,
caption activation and live YouTube behavior still need the
[hardware checklist](docs/HARDWARE_ACCEPTANCE.md).
Format support is not a claim that arbitrary current YouTube videos or every
resolution will play on a PS5.

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
`/data/prosperotube` (fallback `/download0/prosperotube`). Packaging produces a
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
| Touchpad click | Account actions for a video; edits for an account playlist |
| Options | Companion address and pairing code |

Search also accepts YouTube video/playlist URLs, `video:VIDEO_ID`,
`channel:CHANNEL_ID` and `playlist:PLAYLIST_ID` for direct browsing.

In comments, Cross opens replies, L2/R2 scroll the full text, L1/R1 page, R3
switches top/newest sorting, and Circle returns. Enable the companion library
worker for discussion browsing. Comment posting and live/PS5 acceptance remain
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
Up toggles captions, Down opens the caption picker, R2 opens audio tracks,
L2 / touchpad halves adjusts caption timing, Triangle changes aspect ratio,
Square shows media information, Options toggles stats, and Circle confirms stop.
Choose an audio/caption language before opening a video; this version resolves
one selected audio track and downloads one preferred caption sidecar.
Live sources have no seekable timeline. Soft close releases resources and parks
the app; close it from the PS5 switcher afterward.

## Storage and privacy

Local lists are independent of Google, bounded to 384 cached records. State uses
an atomic JSON replacement and is saved periodically during playback. The
companion bearer token is stored in PS5 state and the PC token file; PS5 storage
encryption is not provided. Cookies, resolver headers and signed YouTube URLs
stay on the companion. Media capabilities expire after 30 minutes; selecting
the video again refreshes them. Automatic mid-playback renewal is not
implemented. Access logs are disabled and native diagnostics redact
capabilities. See [protocol/security notes](docs/PROTOCOL.md).

GPL-3.0-or-later. See [LICENSE](LICENSE) and
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
