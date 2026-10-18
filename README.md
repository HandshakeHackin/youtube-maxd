<p align="center">
  <img src="native/branding/icon0.png" width="128" alt="YouTube Max’d icon">
</p>

<h1 align="center">YouTube Max’d</h1>

<p align="center">
  YouTube for jailbroken PS5s. Runs on the console—no companion PC or Google Cloud setup.
</p>

<p align="center">
  <a href="https://github.com/HandshakeHackin/youtube-maxd/releases/download/v1.2.0-preview.1/PPSA99991.zip"><strong>Download PS5 app</strong></a>
  · <a href="https://github.com/HandshakeHackin/youtube-maxd/releases/tag/v1.2.0-preview.1">Release notes & source</a>
  · <a href="docs/APP_GUIDE.md">Full guide</a>
</p>

**Experimental preview · v1.2.0** — intended for **firmware 11.00 with ShadowMount+**. PS5 testing is still needed, including playback, sign-in, microphone, lighting and 4K/HDR performance.

## How YouTube Max’d stands out

**Current preview:** local voice search, Smart playlists, optional Auto Max quality adjustment, and matching red-to-white DualSense effects.

Also includes Google sign-in, subscriptions, playlists, captions, SponsorBlock, DeArrow, and video/audio selection from **144p to 4K**, where available.

**Next build — implemented, awaiting PS5 testing:**

- **Transcript search & bookmarks:** find and save video moments.
- **In-app picture-in-picture:** keep watching while browsing in Afterglow.
- **Voice controls:** pause, resume, seek and change playback options.
- **Session builder:** fit unwatched videos into a chosen time budget.
- **Local family profiles:** PIN, approved-channel and viewing-time controls.
- **Party queue:** phone QR joining, suggestions, votes and host approval.

These six additions are in the [development build](https://github.com/HandshakeHackin/youtube-maxd/tree/feature/maxd-next-experience), outside the current download. [Details and limits](docs/MAXD_NEXT.md). Linked Google child-profile import remains research.

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
