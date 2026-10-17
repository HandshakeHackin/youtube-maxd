<p align="center">
  <img src="native/branding/icon0.png" width="128" alt="ProsperoTube icon">
</p>

<h1 align="center">ProsperoTube</h1>

<p align="center">
  <strong>A native YouTube client for PlayStation 5 homebrew.</strong><br>
  Browse, search and watch YouTube on the console with no computer involved.
</p>

<p align="center">
  <img src="https://img.shields.io/badge/platform-PlayStation%205-003791?logo=playstation&amp;logoColor=white" alt="PlayStation 5">
  <img src="https://img.shields.io/badge/video-AVC%20%7C%20VP9%20%7C%20HEVC%20%7C%20AV1-70E1DC" alt="AVC, VP9, HEVC and AV1">
  <img src="https://img.shields.io/badge/service-built%20in-5BBEFF" alt="Built-in YouTube service">
  <img src="https://img.shields.io/badge/UI-Afterglow%20%7C%20Classic-7DD3FC" alt="Afterglow and Classic interfaces">
  <img src="https://img.shields.io/badge/languages-22-5DDFA4" alt="22 interface languages">
  <a href="LICENSE"><img src="https://img.shields.io/badge/license-GPL--3.0--or--later-blue" alt="GPL-3.0-or-later"></a>
</p>

> [!NOTE]
> **In testing.** Version 1.000.000 (content version `01.000.000`) is being tested on consoles and has not been released yet.

![Afterglow's Home for a signed-in profile: a continue-watching hero above a shelf of video cards, with the navigation rail on the left](docs/images/afterglow-home.png)

<p align="center"><sub>Afterglow's Home, drawn on a PC by the interface's test harness with invented sample videos and channels.</sub></p>

## Highlights

- **Self-sufficient.** The YouTube service runs inside the app (YouTube.js on QuickJS-NG), with no computer, companion or Google API key.
- **Native playback.** Video streams straight from YouTube's servers to the PS5 hardware decoders: AVC, VP9 and HEVC up to 2160p, HDR, and AV1 in software.
- **Two interfaces.** Afterglow, a new console-first interface in four looks, and the complete Classic browser.
- **Guest or signed in.** Everything browses without an account; YouTube accounts sign in on the console with a device code.
- **More.** Phone casting, SponsorBlock, DeArrow, local profiles and 21 interface languages besides English.

> [!IMPORTANT]
> ProsperoTube does not run on an unmodified retail console. It is intended for consoles you own with an already configured, compatible homebrew loader. This repository does not include an exploit, proprietary Sony SDK, system module, encryption key, firmware file, or game asset.

## Project foundation

> [!IMPORTANT]
> **Follows the approach of [SmartTube](https://github.com/yuliskov/SmartTube).** SmartTube's TV-first browsing and settings are the behaviour reference ([parity](docs/PARITY.md)). No SmartTube source is incorporated beyond its list of blocked live-chat author names.

> [!IMPORTANT]
> **Follows the approach of [FreeTube](https://github.com/FreeTubeApp/FreeTube).** Like FreeTube, ProsperoTube reads YouTube without an account or API key through YouTube.js. None of FreeTube's code or assets is included.

> [!IMPORTANT]
> **Built on [EVO Player](https://github.com/sainsaji/EVO-PLAYER-PS5).** The pinned PS5 media player supplies playback, hardware decoding, audio and packaging. The EVO layer (`scripts/prepare_native.py` and its overlays) applies ProsperoTube's changes to a generated copy at build time.

> [!IMPORTANT]
> **The YouTube service is [YouTube.js](https://github.com/LuanRT/YouTube.js) on [QuickJS-NG](https://github.com/quickjs-ng/quickjs).** It runs inside `eboot.bin`, with libcurl and OpenSSL for HTTPS; see [`standalone/`](standalone/README.md).

> [!IMPORTANT]
> **Afterglow is drawn with [ps5-homebrew-ui](https://github.com/blackbearreloaded/ps5-homebrew-ui).** The UI kit, its fonts and sound sets are a pinned build input, not copied into this repository.

> [!IMPORTANT]
> **Native foundations from [PS5 Native App Boilerplate](https://github.com/blackbearreloaded/ps5-native-app-boilerplate) and the PS5 [video](https://github.com/blackbearreloaded/ps5-hardware-video-decoding-research) and [audio](https://github.com/blackbearreloaded/ps5-audio-decoding-research) research.** The update check, self-update helper and console libcurl support come from the boilerplate; the research repositories were the decoder references.

> [!IMPORTANT]
> **Uses [SponsorBlock](https://sponsor.ajay.app/) and [DeArrow](https://dearrow.ajay.app/).** When turned on, the app asks these community services by Ajay Ramachandran and contributors for segments, titles and thumbnails. Nothing is submitted.

The elevation client follows [ProsperoEden](https://github.com/blackbearreloaded/ProsperoEden) and [ProsperoTV](https://github.com/blackbearreloaded/ProsperoTV), and the entropy reader and self-exit [ProsperoLight](https://github.com/blackbearreloaded/ProsperoLight). Every source, version and licence is listed in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

| Identity | Value |
| --- | --- |
| Shell title | `ProsperoTube` |
| Title ID | `PPSA99142` |
| Category | Media |
| Version / content version | `1.000.000` / `01.000.000` (not released) |
| Writable data | `/data/prosperotube` |

## Features

**Browsing and search**

- Search with suggestions, history and filters; video, playlist and channel links are accepted.
- Home, Discover (topics, Gaming, News, Music, Sports, Live, Movies, Trending, Shorts, Kids) and a Live now shelf.
- Channels with their sections, sort orders and in-channel search; playlists and related videos.
- Comments with replies, live chat and chat replay.

**Playback**

- AVC, VP9 and HEVC through the PS5 hardware decoders up to 2160p, and AV1 in software.
- HDR output for PQ and HLG videos, and automatic frame-rate switching to match the video.
- Live streams with rewind, including a remembered position in the live window.
- Per-video quality and audio track (dubs, original, described audio) in the playback options.
- Captions, including YouTube's automatic and auto-translated tracks, a second language at once and caption styles.
- Speed from 0.25x to 4x, chapters, storyboard seek preview, sleep timer, zoom, aspect and audio delay.
- Up next, seven end-of-video modes and recovery after stream failures.
- Keep a video playing as audio while browsing in Afterglow (experimental).

**Accounts**

- Sign-in with a device code on the console; up to eight account profiles next to Guest.
- Subscriptions, notifications, YouTube history, Watch later, playlists and own videos.
- Like, subscribe, Watch later and playlist edits, each reviewed before it is sent.

**Casting and integrations**

- Phone casting from the YouTube app with a TV code (experimental).
- SponsorBlock with per-category actions, skip review and Undo; DeArrow titles and thumbnails.
- Optional dislike counts from Return YouTube Dislike (off by default).

**Interface and your data**

- Afterglow (preview) and Classic interfaces; Afterglow, Onyx, Daybreak and High contrast themes.
- The interface in 21 languages besides English, following the PS5 system language.
- Local profiles with history, resume, favorites, followed channels and an ordered queue.
- Backups, automatic backup, and subscriptions import from NewPipe, PocketTube or GrayJay.
- Physical keyboard typing in Afterglow's search and text fields.
- Update offers and in-place self-update through homebrew.page.

![Afterglow's Search: the field with typed text, suggestions below it, the on-screen keyboard on the right and best matches underneath](docs/images/afterglow-search.png)

![Afterglow's player controls over a paused video: title, chaptered timeline with SponsorBlock marks, and the control row](docs/images/afterglow-player.png)

<p align="center"><sub>Afterglow's Search and player controls, drawn by the same PC test harness with invented sample data.</sub></p>

## Project status

Guest browsing and playback, 4K VP9, HDR output, automatic frame-rate switching and live rewind have passed bounded sessions on PS5 consoles. Accounts in daily use, a complete update from a published release and the remaining Afterglow screens are still being validated, and Classic stays the default interface until then. Videos that need an account (age-restricted, private, members-only) do not play yet, and the player's own controls stay in English. [Status](docs/STATUS.md) and [hardware acceptance](docs/HARDWARE_ACCEPTANCE.md) record what was tested and where.

ProsperoTube talks to YouTube as one client identity. When YouTube changes its service, playback can stop until an app update arrives.

## Roadmap

- Console acceptance of every Afterglow screen, then Afterglow as the default interface.
- Longer public 4K and live playback across varied network conditions.
- Accounts in daily use, phone casting and self-update validated from a published release.
- The player's controls and menus in the interface languages.
- Playback of videos that need the signed-in account.
- Voice search without a computer.
- A web proxy setting, backup to Google Drive and video previews on focused cards.

## Getting started

### Requirements

- A PS5 that runs homebrew, with a title loader that registers apps from `/data/homebrew` (such as ShadowMountPlus).
- Its ELF loader listening on TCP port 9021, or a resident Lapy service: ProsperoTube uses Lapy to reach its data folder and stops at startup without it.
- An internet connection.

### Install or update

1. Download `PPSA99142.zip` from the release and check it against `SHA256SUMS`.
2. Fully close ProsperoTube if it is running.
3. Extract the ZIP and copy the complete `PPSA99142` folder to `/data/homebrew/`, so that `/data/homebrew/PPSA99142/eboot.bin` exists.
4. Start ProsperoTube.

Once a release is listed on homebrew.page, the app checks for updates at launch and offers **Update now**, **What's new** or **Skip**; accepting downloads, verifies and replaces the app after it closes. Settings, profiles and accounts in `/data/prosperotube` are kept. See [Updates](docs/UPDATES.md).

### First use: Guest or signed in

ProsperoTube opens as **Guest**: search, Home, Discover, channels, playlists, comments and playback work without an account, and history, favorites and the queue stay on the console.

To sign in, choose **Sign in to YouTube** on the main menu (also first under **Accounts & library** and in the account settings while you are a Guest):

1. The app shows an address and a code, also as a QR code.
2. Open the address on your phone or computer and enter the code.
3. The app notices the authorization by itself; the account becomes a profile next to Guest.

Playback stays Guest's; a signed-in profile reports watch history unless **History** is off. The sign-in is stored as plain text in `/data/prosperotube/accounts/oauth-profiles.json` (the PS5 has no encrypted storage) and is not part of backups. **Remove selected account** deletes it; to revoke it at Google too, remove "YouTube on TV" under <https://myaccount.google.com/permissions>.

### Casting from a phone

1. Open **YouTube preferences > Phone pairing** and choose **Link a phone** (in Afterglow, also under Accounts). A TV code appears.
2. In the YouTube app on the phone, open *Watch on TV > Enter TV code*.
3. Start a video or a queue from the phone; play, pause, seek, skip, volume, captions and stop work from there.

**Reset phone pairing** forgets this console. See [the remote service](docs/REMOTE_SERVICE.md).

### Choosing the interface and theme

- **YouTube preferences > Interface > Interface** chooses **Classic** (the default) or **Afterglow (preview)**.
- **Theme**, per profile: Afterglow, Onyx on true black, the light Daybreak or High contrast; nine earlier schemes are under **Classic colours**.
- **YouTube preferences > Interface language** overrides the PS5 system language.

## Controls

![Touchpad][touchpad] is the touchpad button: press it until it clicks. Afterglow shows its buttons at the foot of every screen; its full map is in the [design notes](docs/afterglow/design-notes.md#10-controller-map).

### Classic browser

| Input | Action |
| --- | --- |
| ![D-pad][dpad] / ![Cross][cross] | Move focus / open or watch |
| ![Square][square] | Search |
| ![Circle][circle] | Back; cancel a pending request |
| ![Triangle][triangle] | Toggle favorite |
| R2 / L2 | Toggle queue / follow the video's channel |
| ![L1][l1] / ![R1][r1] | Previous / next page |
| R3 | Comments, or live chat on a live video |
| ![Touchpad][touchpad] | Video, section, playlist and comment actions |
| ![Options][options] | Retry the built-in service if it did not start |

### Player

| Input | Action |
| --- | --- |
| ![Cross][cross] | Pause or resume |
| ![D-pad][dpad] left / right | Seek |
| ![D-pad][dpad] up / down | Toggle captions / caption picker |
| ![L1][l1] / ![R1][r1] | Previous / next chapter |
| ![Triangle][triangle] | Up next |
| R2 | Playback options: quality, audio track, speed, video size and video actions |
| ![Square][square] | Media information (Afterglow: caption picker) |
| ![Options][options] | Statistics (Afterglow: playback options) |
| L2 / ![Touchpad][touchpad] halves | Caption timing |
| R3 | Jump to the next SponsorBlock highlight |
| ![Circle][circle] | Stop (confirmed) |

In Afterglow, media information and statistics are in the playback options as **Details** and **Playback details**. The D-pad directions can be reassigned under **Player controls, keys and Shorts**. Closing ProsperoTube from its menu releases its resources; then close it from the PS5 switcher.

[cross]: docs/images/buttons/cross.svg "Cross"
[circle]: docs/images/buttons/circle.svg "Circle"
[square]: docs/images/buttons/square.svg "Square"
[triangle]: docs/images/buttons/triangle.svg "Triangle"
[options]: docs/images/buttons/options.svg "Options"
[l1]: docs/images/buttons/l1.svg "L1"
[r1]: docs/images/buttons/r1.svg "R1"
[dpad]: docs/images/buttons/dpad.svg "D-pad"
[touchpad]: docs/images/buttons/touchpad.svg "Touchpad button"

## Files on the console

Everything the app writes is in one folder that an update or reinstall does not touch:

| Path | Contents |
| --- | --- |
| `/data/prosperotube/youtube.json` | Settings, profiles, history, favorites and queues |
| `/data/prosperotube/accounts/oauth-profiles.json` | Signed-in accounts, as plain text |
| `/data/prosperotube/backup/` | Manual and automatic backups |
| `/data/prosperotube/import/` | Place a subscriptions export here to import it |
| `/data/prosperotube/cache/` | The built-in service's cache; safe to delete |
| `/data/prosperotube/providers/youtube/art/` | Thumbnails kept between launches |

To start over, delete `/data/prosperotube` while the app is closed. Whoever can read that folder can use the signed-in accounts. The built-in service listens on `127.0.0.1` only, behind a random token, and writes no log; see [the protocol and security notes](docs/PROTOCOL.md) and [`native/elevation`](native/elevation/README.md) for how storage access works.

## Build

Use x86_64 Ubuntu 24.04, natively or in WSL. No proprietary SDK is required.

```bash
git clone https://github.com/blackbearreloaded/ProsperoTube.git
cd ProsperoTube
make host-deps  # once: system packages (uses sudo)
make zip        # or make: fetch pinned dependencies, build and verify the ZIP
make test       # offline checks
```

The first build compiles a large shader toolchain and takes a while; later builds reuse its cache. Outputs:

```text
dist/PPSA99142/        complete title folder
dist/PPSA99142.zip     installable ZIP
dist/SHA256SUMS        ZIP checksum
```

[Build](docs/BUILD.md) has the pins, cache paths, overrides and release steps.

## GitHub Actions and releases

The [Build workflow](.github/workflows/build.yml) runs for version tags (`v` + the content version, for example `v01.000.000`), pull requests and manual runs; pushes to `main` build nothing. It builds with `make zip` and runs the offline checks beside it, then checks the ZIP (one `PPSA99142` folder, every entry stored 0777, `eboot.bin`, `sce_sys`, the helpers, the version), runs `make verify`, attests the ZIP and uploads `PPSA99142.zip` with `SHA256SUMS`.

- A pull request's build is named `ProsperoTube-PR<number>-<commit>` and carries `build-label.txt` in the app folder.
- A tag publishes a pre-release with exactly those two files.
- The PS5 shader compiler and the compiler cache come from the cache or the `build-cache` snapshot; a manual run on `main` with **snapshot** ticked refreshes them.

Verify a downloaded release with `gh attestation verify PPSA99142.zip -R blackbearreloaded/ProsperoTube`. See [Build](docs/BUILD.md) for details.

## Documentation

| Document | Purpose |
| --- | --- |
| [Changelog](CHANGELOG.md) | User-visible changes in each version |
| [Status](docs/STATUS.md) | What is implemented, what was tested and where |
| [Hardware acceptance](docs/HARDWARE_ACCEPTANCE.md) | Every console session and its result |
| [Validation](docs/VALIDATION.md) | Host and package validation record |
| [Handoff notes](docs/HANDOFF.md) | Where the work stands and what comes next |
| [Build](docs/BUILD.md) | Dependencies, the PS5 build, checks and releasing |
| [Updates](docs/UPDATES.md) | Update check and self-update |
| [Localization](docs/LOCALIZATION.md) | Interface languages and what stays English |
| [Decisions](docs/DECISIONS.md) | Design decisions and their reasons |
| [Parity](docs/PARITY.md) | Comparison with SmartTube |
| [Afterglow design notes](docs/afterglow/design-notes.md) | The interface design and controller map |
| [Afterglow architecture](docs/afterglow/ARCHITECTURE.md) | How Afterglow is built and tested |
| [Built-in service](standalone/README.md) | The YouTube service on the host and the console |
| [Notices](THIRD_PARTY_NOTICES.md) | Dependency, asset and licence attribution |

### Elevation credits

Filesystem elevation uses [PS5-Lapy-JB-Daemon](https://github.com/ArkSama/PS5-Lapy-JB-Daemon),
created by ArkSama, with the cooperative owned-root helper from
[mpereiraesaa's PS5-Lapy-JB-Daemon](https://github.com/mpereiraesaa/PS5-Lapy-JB-Daemon)
and contributors. The build pins commit `57fcc19` of the
[BlackBearReloaded fork](https://github.com/blackbearreloaded/PS5-Lapy-JB-Daemon), which
carries the donor-release and firmware 13.60 fixes of
[PR #48](https://github.com/mpereiraesaa/PS5-Lapy-JB-Daemon/pull/48) and
[PR #49](https://github.com/mpereiraesaa/PS5-Lapy-JB-Daemon/pull/49) and the guarded
target-lifetime changes of [PR #50](https://github.com/mpereiraesaa/PS5-Lapy-JB-Daemon/pull/50),
runs upstream's own exact-title helper build for `PPSA99142` without changing it, verifies
the generated manifest and ships the helper (MIT; its protocol header is LGPL-2.1-or-later)
with its licence, as [ProsperoEden](https://github.com/blackbearreloaded/ProsperoEden) does.

<!-- bbr-footer:start -->
<!-- Generated by ps5-homebrew-dev-protocol/scripts/readme-footer. Edit the template there, not here. -->

## Credits

Built with the [PS5 Payload SDK](https://github.com/ps5-payload-dev/sdk) by John Törnblom (ps5-payload-dev).
Third-party components, authors and licenses are listed in
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

## License

Copyright © 2026 BlackBearReloaded. Licensed under GPL-3.0-or-later; see [LICENSE](LICENSE). Third-party components keep their own licenses. Binary releases are built from the tagged source in this repository.

## Disclaimer

- **No affiliation.** This is an independent homebrew project. It is not
  affiliated with, endorsed by, or sponsored by Sony Interactive Entertainment.
  "PlayStation", "PS5" and related marks are trademarks of Sony Interactive
  Entertainment Inc. This project is not affiliated with or endorsed by YouTube, Google or the EVO Player project. YouTube is a trademark of Google LLC.
- **No proprietary material.** No Sony SDK, firmware, encryption keys or
  decrypted system modules are included.
- **No warranty.** This project is provided "as is", without warranty of any
  kind, to the extent permitted by law. See sections 15 and 16 of the GPL.
- **Use at your own risk.** Running homebrew requires a modified console, which
  may void its warranty, breach the platform's terms of service, or cause data
  loss.
- **Legal use only.** Use it only with hardware, accounts and content you own.
  This project does not support or enable piracy.

## AI assistance

This project was developed with AI assistance from OpenAI and/or Anthropic tools.
<!-- bbr-footer:end -->
