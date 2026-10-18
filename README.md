<p align="center">
  <img src="native/branding/icon0.png" width="128" alt="YouTube Max’d icon">
</p>

<h1 align="center">YouTube Max’d</h1>

<p align="center">
  <img src="https://img.shields.io/badge/Platform-PS5-003791?logo=playstation&amp;logoColor=white" alt="PlayStation 5">
  <a href="https://github.com/HandshakeHackin/youtube-maxd/releases"><img src="https://img.shields.io/github/v/release/HandshakeHackin/youtube-maxd?include_prereleases&amp;label=Preview&amp;color=FF304F" alt="Latest preview release"></a>
  <img src="https://img.shields.io/badge/Service-On--console-FFFFFF" alt="On-console YouTube service">
  <img src="https://img.shields.io/badge/UI-Classic%20%7C%20Afterglow-FFA5AA" alt="Classic and Afterglow interfaces">
  <a href="LICENSE"><img src="https://img.shields.io/badge/License-GPL--3.0--or--later-FFFFFF" alt="GPL-3.0-or-later license"></a>
</p>

<p align="center">
  YouTube for jailbroken PS5s. Runs on the console—no companion PC or Google Cloud setup.
</p>

<p align="center">
  <a href="https://github.com/HandshakeHackin/youtube-maxd/releases/download/v1.3.0-preview.1/PPSA99991.zip"><img src="https://img.shields.io/badge/Download-PS5_App_ZIP-FF304F?style=for-the-badge" alt="Download the PS5 app ZIP"></a>
  <a href="https://github.com/HandshakeHackin/youtube-maxd/releases/tag/v1.3.0-preview.1"><img src="https://img.shields.io/badge/Release_Notes-%26_Source-FFFFFF?style=for-the-badge&amp;logo=github&amp;logoColor=181717" alt="Release notes and corresponding source"></a>
  <br><a href="docs/APP_GUIDE.md">Full guide</a>
</p>

**Experimental preview · v1.3.0** — intended for **firmware 11.00 with ShadowMount+**. PS5 testing is still needed, including playback, sign-in, microphone, lighting and 4K/HDR performance.

## How YouTube Max’d stands out

**Also included:** local voice search, Smart playlists, optional Auto Max quality adjustment, and matching red-to-white DualSense effects.

Also includes Google sign-in, subscriptions, playlists, captions, SponsorBlock, DeArrow, and video/audio selection from **144p to 4K**, where available.

**New in this preview:**

- **Transcript search & bookmarks:** find and save video moments.
- **In-app picture-in-picture:** optional viewing while browsing in Afterglow; off by default.
- **Voice controls:** pause, resume, seek and change playback options.
- **Session builder:** fit unwatched videos into a chosen time budget.
- **Local family profiles:** PIN, approved-channel and viewing-time controls.
- **Party queue:** phone QR joining, suggestions, votes and host approval.

[Feature instructions and limits](docs/MAXD_NEXT.md) · [Changelog](CHANGELOG.md). Linked Google child-profile import remains research.

## Install on PS5

1. Download the **PS5 app ZIP** above.
2. Extract it and copy the complete **PPSA99991** folder to your ShadowMount+ app location, commonly `/data/homebrew/PPSA99991`.
3. Start your compatible ELF loader on **TCP 9021** or resident **Lapy** service, then launch **YouTube Max’d**.

Keep all included files. [Full installation instructions](docs/APP_GUIDE.md#install-on-ps5).

**Controls:** Cross selects/pauses · Circle goes back · Square searches · Hold L3 for voice search · R2 opens playback options.

## Help & documentation

[Google sign-in](docs/GOOGLE_LOGIN.md) · [Build from source](docs/BUILD.md) · [Testing & limits](docs/APP_GUIDE.md#testing-and-current-limits) · [Report an issue](https://github.com/HandshakeHackin/youtube-maxd/issues)

## Credits and licenses

Built on **[ProsperoTube](https://github.com/LedyBacer/ProsperoTube)** by **BlackBearReloaded and contributors**, and **[EVO Player](https://github.com/sainsaji/EVO-PLAYER-PS5)**. Their work provides the YouTube client and player foundation.

[Full credits](docs/APP_GUIDE.md#credits-and-licenses) · [Third-party notices](THIRD_PARTY_NOTICES.md) · [GPL-3.0-or-later](LICENSE)
