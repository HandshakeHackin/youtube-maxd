# YouTube Max’d 1.4.0

First official release, adding a PS5-hosted Phone remote, private bug-report
export and optional support on Ko-fi. Intended setup: **PS5 firmware 11.00 with
ShadowMount+**. Console acceptance testing is still pending; the release label
is not a claim of hardware certification.

## What’s new

- **Phone remote:** scan a one-use QR, type YouTube searches, navigate the TV,
  pause/resume and seek videos, and add/remove/reorder upcoming queue entries
  from a phone browser. Hosted entirely on the PS5, with no companion PC or
  phone app. Off by default; one controlling phone per session.
- **Private bug-report export:** capture and review a fixed set of technical
  playback fields. Download JSON on the paired phone or retrieve
  `/data/youtube-maxd/bug-report.json`. Excludes accounts, tokens, IP addresses,
  video IDs/titles, searches, URLs and raw logs. Nothing uploads automatically.
- **Optional support:** Settings → About → Support YouTube Max’d shows
  HandshakeHackin’s Ko-fi page and a phone QR, matching Nuvio Max’d.
- **Official release identity:** removes “preview”; content version increases
  to `01.004.000`. Title ID `PPSA99991` and the app data folder remain the same.

Transcript search/bookmarks, voice search/commands, session builder, local
family controls, party queue, Smart playlists, optional Auto Max, Google
sign-in, SponsorBlock, DeArrow and red/white DualSense effects are retained.
**Picture-in-picture remains off by default**, enabled explicitly per session.
Linked Google child-profile discovery remains research.

## Use the new features

Open **Settings → Phone remote → Start Phone remote → Show pairing QR code**.
Use the same trusted LAN as the PS5. On your phone, tap Back to leave the TV’s
QR/settings page before searching. QR codes last five minutes and are single
use; paired sessions last two hours. Stop/restart, changing profile/source or
closing the app revokes control. Restricted family profiles cannot host it.
Local HTTP uses port 8767; it is intended for a trusted home network.

On the phone, select **Capture & preview report**, review it and tap
**Download report (.json)**. Local capture/review/delete is also available in
**Settings → About → Bug-report export**. Each capture replaces one report.
[Full instructions and limits](https://github.com/HandshakeHackin/youtube-maxd/blob/v1.4.0/docs/PHONE_REMOTE.md).

## Install or update

Download **PPSA99991.zip** from this release. Close the app and back up
`/data/youtube-maxd`. Extract the ZIP and replace the complete `PPSA99991` folder
in your ShadowMount+ app location, commonly `/data/homebrew/PPSA99991`. Keep
all included assets, helpers and the local voice model.

Startup needs a compatible console-side ELF loader on TCP 9021 or resident
Lapy service. The icon and GitHub’s automatic “Source code” archives are not
installable apps. **YouTube-Maxd-source.zip** contains the complete matching
sources. **SHA256SUMS** verifies every release attachment. Automatic in-app
updates remain disabled; store updates follow catalog review and deployment.

## Validation and remaining tests

See **TEST_RESULTS.txt**, **native-validation.json** and **catalog-scan.txt** for
recorded software/build/archive checks. Host HTTP and player-adapter fixtures
cover pairing, origin/authentication/framing checks, command deduplication,
selection guards, live/casting/keyboard/voice refusal, queue/profile isolation
and the report allowlist/private-file export. UI pictures are fixture renders.

No PS5 is attached to the build environment. This exact build still needs
console boot, real YouTube/Google sign-in and playback, LAN phone-browser
pairing/controls/downloads, microphone, lighting, background/PiP, 4K/HDR and
long-session testing. Available formats depend on YouTube and actual codec
performance. See **TESTING.txt** for a practical console checklist.

## Credits and licenses

Based on **[ProsperoTube](https://github.com/LedyBacer/ProsperoTube)** by
BlackBearReloaded and contributors, and
**[EVO Player](https://github.com/sainsaji/EVO-PLAYER-PS5)**. Their client,
service, account and player foundations remain credited in the bundled notices
and corresponding source. GPL-3.0-or-later; see the source package for dependency
licenses and pins. Not affiliated with YouTube, Google or Sony.
