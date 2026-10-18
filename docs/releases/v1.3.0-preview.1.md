# YouTube Max’d 1.3.0 — viewing features preview

Adds transcript tools, optional in-app PiP, voice controls, session building,
local family profiles and a PS5-hosted party queue. Intended setup: PS5 firmware
11.00 with ShadowMount+. **Experimental prerelease; PS5 testing is still needed.**

## Changes since v1.2.0-preview.1

- **Transcript search and bookmarks:** search available captions, jump to a
  line and save named timestamps separately for each local profile.
- **Optional in-app picture-in-picture:** watch in the bottom-right corner
  while browsing in Afterglow. **Off by default.** Select **R2 → Picture-in-picture**
  to activate it for the current session; tap L3 to return. The choice is not
  saved for future sessions. PiP stays inside the app.
- **Push-to-talk playback commands:** hold L3, speak after Listening and release
  to pause/resume, seek, change available quality/captions or add to the queue.
  Recognition runs locally; Circle cancels. No always-on microphone.
- **Session builder:** choose a source and a 15–120-minute budget to create a
  playlist of unwatched videos within that runtime.
- **Local family profiles:** four independent signed-out slots, parent PIN,
  approved-channel playback, optional search restriction and daily viewing
  limits. These are local app controls; Google child profiles are not imported.
- **Party queue:** optional LAN page hosted on the PS5, phone QR joining,
  expiring invites, guest suggestions/votes and host approval. Off at startup;
  no companion PC or Google Cloud project required.
- **Update identity:** content version increases from `01.002.000` to
  `01.003.000`. The title ID `PPSA99991` and `/data/youtube-maxd` data location
  remain the same. Automatic in-app updating remains disabled.

Existing local voice search, Smart playlists, optional Auto Max, red-to-white
DualSense effects, Google TV-code sign-in, SponsorBlock and DeArrow remain included.
See [feature instructions and limits](https://github.com/HandshakeHackin/youtube-maxd/blob/v1.3.0-preview.1/docs/MAXD_NEXT.md).

## Install or update

Download **PPSA99991.zip** from this release. Close the app, back up
`/data/youtube-maxd`, extract the ZIP and replace the complete `PPSA99991` app
folder in your ShadowMount+ location, commonly `/data/homebrew/PPSA99991`.
Keep all assets and helpers, including the bundled voice model.

Startup needs a compatible console-side ELF loader on TCP 9021 or resident
Lapy service. The app icon and GitHub's automatic Source code ZIP are not
installable app bundles. **YouTube-Maxd-source.zip** contains the matching
complete corresponding sources. Use **SHA256SUMS** to verify the attachments.

## Validation and limits

Native PS5 compilation, signed-container integrity, package metadata/permissions,
source manifest and host tests are checked for this revision. Recorded checks
cover transcript/bookmark handling, session selection, voice commands, PiP
defaults/ownership, family restrictions/isolation, party HTTP/queue behavior
and background playback. Fixture checks do not establish live service behavior.

No PS5 is attached to the build environment. Boot, actual playback/sign-in,
PiP codecs/HDR, microphone routing/latency and physical controller behavior
still need console testing. Linked Google supervised-profile import remains
research. Family controls are local, not YouTube Kids certification. Party
rooms use HTTP on a trusted LAN. Ad removal is not guaranteed.

## Credits

Built on **ProsperoTube by BlackBearReloaded and contributors** and **EVO Player**.
Their player, embedded service and account/integration work remain credited.
Additional acknowledgments and licenses ship with the app and matching sources.
GPL-3.0-or-later; independent homebrew, unaffiliated with Google, YouTube or Sony.
