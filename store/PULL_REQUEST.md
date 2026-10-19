**Title ID:** PPSA99991
**Type:** existing app update, 1.3.0-preview.1 → 1.4.0
**Category:** video

## Changes

- First official release label; add a PS5-hosted Phone remote for typed search,
  TV navigation, pause/resume, seek and local queue add/remove/play-next.
- Opt-in local HTTP, one controlling phone, expiring one-use QR/session and
  profile/source/shutdown revocation. Restricted profiles cannot host it.
- Add user-requested bug-report capture, review, download and deletion with a
  closed technical allowlist; no accounts, tokens, IPs, video identities, URLs
  or raw logs, and no automatic uploads.
- Include optional Ko-fi support for HandshakeHackin, matching Nuvio Max’d.
- Keep PiP **off by default**. Preserve title ID/data and raise contentVersion
  from `01.003.000` to `01.004.000`.

Release/changelog: https://github.com/HandshakeHackin/youtube-maxd/releases/tag/v1.4.0

Immutable ZIP: `PPSA99991.zip`, 99,530,570 bytes.
SHA-256: `16a4b5960f1369fd593cd17852bd04c940d2febfa535094a8a647a40e0ca741e`.
Native application source: `1eb9b8b358087738bc8a331ee2e612d73391a14b`.
The release tag's `store/sce_sys` metadata/icon match the archive. Complete
matching sources, checksums, software test evidence and static scan accompany it.
Original public release assets remain untouched.

Based on **ProsperoTube** by BlackBearReloaded/contributors and **EVO Player**;
existing derivative listing approved in #397, previous update merged in #417.
GPL source, dependency pins and notices are retained. No upstream endorsement
is claimed.

The native build (314 objects) and host HTTP/provider/player/UI tests pass.
This exact build has **not** been tested on PS5 hardware or with a live Google
account. Firmware 11.00 / ShadowMount+ is the intended target; console boot,
real playback/sign-in, phone-browser LAN/download behavior, microphone, lighting,
PiP/4K/HDR and long-session acceptance remain pending. Linked Google child
profiles remain research. Local family controls do not imply YouTube Kids
certification.

The disclosed Lapy/elevation and self-updater helpers are retained; automatic
in-app updating remains disabled. This request does not modify helper approvals
or claim that static scanning establishes hardware functionality.

Only `apps/PPSA99991.json` changes, limited to version, artifact_url, sha256 and
icon_url. Record supplied by HandshakeHackin, owner of source_repo.
