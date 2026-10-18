# YouTube Max’d — standalone PS5 YouTube client

Version 1.2.0 builds on pinned ProsperoTube/EVO source, with our own launcher
identity, icon, quality preferences and controller effects. Search, extraction,
account access and playback run on the PS5. A phone/computer approves a Google
activation code once; **no companion service or Google Cloud setup is needed**.

Experimental target: PS5 firmware 11.00 / ShadowMount+. **This integrated build
has not been tested on a PS5.** See [validation](docs/MAXD_VALIDATION.md).

[Download the app and complete source](https://github.com/HandshakeHackin/youtube-maxd/releases)
from the repository's prereleases. The installable ZIP, corresponding source,
launcher icon and SHA-256 checksums are supplied together.

## Included implementation

- Embedded YouTube.js 18.1.0 on QuickJS-NG 0.17.0 with verified HTTPS.
- Native AVC/H.264, VP9 and HEVC decoding, output up to 2160p, HDR and display
  mode handling from EVO/ProsperoTube. AV1 software decoding uses dav1d;
  decoder inclusion does not promise smooth 4K AV1 playback.
- Per-video quality/audio-track switching; saved per-profile maximum quality
  from 144p to 2160p, frame-rate limits and codec preferences.
- Optional Auto Max quality responds to sustained buffering or dropped frames,
  with guarded upgrades after stable playback. Manual format choices take priority.
- Profile-owned Smart playlists filter Home, subscriptions, favorites or the
  play queue by title, creator, duration, watched status and live status.
  See [Smart playlists and Auto Max](docs/MAXD_FEATURES.md).
- TV device-code login, account profiles, subscriptions, account-library
  integration and explicit confirmation of account writes.
- Suggestions, Home/Discover, channels, playlists, comments, local history,
  resume, favorites, queues and subscriptions import.
- Captions and translations, secondary captions, chapters, seek previews,
  speed, sleep timer, audio delay, live DVR and stream-recovery paths.
- Optional SponsorBlock categories/review/undo, DeArrow and dislike counts.
  Direct media playback does not request separate ad placements; it does not
  guarantee removal of server-inserted ads.
- Local push-to-talk voice search with a bundled multilingual Whisper model;
  hold L3, speak after Listening appears, then release. No speech server needed.
  See [voice search](docs/VOICE_SEARCH.md) for controls and hardware-test limits.
- Classic and Afterglow interfaces, themes, translations and experimental
  casting/background audio from the upstream foundation.
- Approved red-to-white YouTube Max’d artwork and native controller light-bar
  effects in red `#FF304F`, soft red `#FFA5AA` and white `#FFFFFF`.

Account-required playback (private, members-only and age-restricted videos)
remains disabled by default. Broader account feeds, casting, background audio,
HDR and recovery need console acceptance. Internal YouTube APIs can change.

## Install

1. Close the previous app. Back up its `PPSA99991` folder for rollback.
2. Extract `YouTube-Maxd-PS5.zip` and copy the complete `PPSA99991` folder to a
   location scanned by ShadowMount+, commonly `/data/homebrew/PPSA99991`.
3. Use your configured homebrew environment. Native startup needs the ELF
   loader on TCP 9021 or a compatible resident Lapy service. The packaged
   helper and manifest are bound to **PPSA99991**.
4. Start **YouTube Max’d**. Browse as Guest or choose **Sign in to YouTube**.

Keep all assets, runtime files and helper manifest. The development title ID
and prototype content ID are retained, with content version `01.002.000`.
Do not overwrite a different app that happens to use this development ID.

State now lives under `/data/youtube-maxd`. Old `/download0` prototype tokens
and settings are not automatically imported. Sign in again; no
`google-oauth.json` is needed. Tokens are local plain text with restricted
file permissions. See [Google sign-in](docs/GOOGLE_LOGIN.md).

## Controls and light bar

| Context | Control |
| --- | --- |
| Browse | D-pad / Cross select; Circle back; Square search |
| Voice search in browser | Hold L3 until Listening, speak, release; Circle cancels |
| Player | Cross pause; Left/Right seek; Circle stop |
| Playback options | R2: quality, audio track, speed and other options |
| Captions / chapters | Up/Down captions; L1/R1 chapters |
| Saved quality ceiling | YouTube preferences → Maximum quality |
| Auto Max | YouTube preferences → Auto Max quality (off by default) |
| Smart playlists | Smart playlists section; Afterglow Library → Smart playlists |
| Light bar | Player settings → controller feedback / light-bar switch |

Classic remains the default; Afterglow is selectable in Interface preferences.
Its button hints describe context-specific controls. The light bar cycles
through red and white while browsing, breathes while loading/signing in,
uses dim red while playing, dim white while paused, and a soft red error pulse. Writes are capped at
20 per second; exit/soft-close turns it off. Physical reconnect and native
system-overlay focus still need console validation.
These effects control the DualSense controller, using its existing native pad
handle. Custom console chassis lighting is not included.

## Build

Clone with the pinned EVO submodule and its nested dependencies:

```sh
git clone --recurse-submodules https://github.com/HandshakeHackin/youtube-maxd.git
cd youtube-maxd
```

Use x86-64 Linux/WSL and the prerequisites in [the build guide](docs/BUILD.md).
Pinned dependencies and build outputs stay in the project's `.deps` directory.
The default build verifies EVO's existing video shader headers against their
pinned source. Rotation and mirroring controls are hidden in this build.
Developers can opt into the additional shader path with
`MAXD_ENABLE_DISPLAY_TRANSFORMS=1 make zip`; building its compiler is substantial
and the resulting path needs separate console validation. No console is contacted.

```sh
make deps
make zip
bash scripts/test_maxd.sh
bash scripts/test_maxd_features.sh
bash scripts/test_native.sh maxd_smart maxd_auto maxd_quality stream_choice profile_settings
bash standalone/test_host.sh
python3 scripts/package_maxd_release.py
```

For the source ZIP, first run `python3 scripts/setup_source_checkout.py` to
restore pinned Git metadata without replacing the supplied source files.
Before distributing a binary, run `python3 scripts/collect_maxd_sources.py`
to populate the matching native-library source/notices used by the packager.

The optional shader compiler uses the pinned GPUOpen LLPC standalone target
and public gfx1013 patch, followed by shader metadata verification. Restricted runners
may use `ASAN_OPTIONS=detect_leaks=0` while retaining ASan/UBSan checks.

Downloads: `dist/YouTube-Maxd-PS5.zip`, `dist/YouTube-Maxd-source.zip`,
`dist/YouTube-Maxd-icon.png` and `SHA256SUMS`. Automatic updates are off until
an independent release feed is configured.

## Credits

Based on ProsperoTube, copyright (C) 2026 BlackBearReloaded, commit
`3446f364172c299a17cecf29a3efcf23c8463f68`, and its pinned EVO Player. Their
credits, GPL-3.0-or-later source and dependency notices are retained. See
[fork changes](MAXD_CHANGES.md), [provenance](UPSTREAM.json),
[notices](THIRD_PARTY_NOTICES.md) and [upstream README](docs/UPSTREAM_README.md).
Max’d's icon follows Nuvio Max’d's faceted-M design language; its master and
reference provenance are in `branding/`.

Independent homebrew, unaffiliated with Google, YouTube, Sony, SmartTube,
ProsperoTube or their developers. No private Sony SDK, firmware modules or
account credentials are distributed.
