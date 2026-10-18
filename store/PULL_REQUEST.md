**Title ID:** PPSA99991
**New listing, update or reservation:** update
**What the app does:** Native standalone PS5 YouTube client based on ProsperoTube/EVO. This preview adds six viewing tools and keeps PiP off by default.
**Category:** video
**Fork or derivative of an app in the catalog?** Yes: ProsperoTube by BlackBearReloaded and contributors. Existing derivative listing approved in #397; source and licenses remain credited.

## Changes

- Transcript search, timestamp seeking and local per-profile bookmarks.
- Optional Afterglow in-app PiP, **off by default** and explicitly selected per session.
- Local push-to-talk playback commands and time-budget session builder.
- Four signed-out family profiles with PIN/channel/search/time controls.
- PS5-hosted LAN party queue with QR joining, suggestions/votes and host approval.
- Content version raised from `01.002.000` to `01.003.000`, retaining title/data identity.

Release/changelog: https://github.com/HandshakeHackin/youtube-maxd/releases/tag/v1.3.0-preview.1

Immutable ZIP: `PPSA99991.zip`, 99,519,618 bytes.
SHA-256: `22b4bf2c8b7fa67f3e11198645fab9746f7381170b12dfc82ce709814792e593`.
The tag's `store/sce_sys` metadata/icon match the packaged app. Complete sources,
checksums, testing instructions/results and the official static scan accompany
the release. The old release assets remain untouched.

This exact build has **not** been tested on PS5 hardware or a live Google account.
Boot, real playback/sign-in, PiP codecs/HDR, microphone latency/routing and
physical controller behavior still need acceptance. Linked Google child-profile
import remains research. Local family limits are not YouTube Kids certification;
the party page uses HTTP on a trusted LAN.

The app retains its disclosed Lapy/elevation and self-updater payloads; automatic
updating remains disabled. Scan: 0 errors, 6 warnings and 1 summary notice; full findings and helper hashes are attached.
This request does not change `helpers/approved.json` or claim helper approval.

- [x] Native PS5 app with its own eboot.bin and sce_sys; not a payload or PS4 package.
- [x] Submitted by HandshakeHackin, owner of source_repo.
- [x] Artifact is the app-folder ZIP for PPSA99991.
- [x] Published experimental GitHub release, matching tag/version and exact ZIP digest.
- [x] Asset remains immutable; original release remains available.
- [x] Higher, matching contentVersion is committed at the release tag.
- [x] Video category, original license and app identity retained.
- [x] Distribution rights, corresponding sources and dependency notices retained.
- [x] Existing distinct-app listing; review policy read, no upstream endorsement claimed.

Only `apps/PPSA99991.json` changes, limited to the four release-bump fields.
