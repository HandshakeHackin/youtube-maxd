Add YouTube Max’d 1.2.0-preview.1 as a new native PS5 app listing (PPSA99991, video).

The publisher is HandshakeHackin, which owns the public source/release repository. This is a ProsperoTube/EVO derivative, with original BlackBearReloaded and dependency credits retained. User-facing additions include bundled on-console voice recognition, profile-owned rule-based Smart playlists and optional guarded Auto Max quality.

- Published ZIP: PPSA99991.zip, 99,483,205 bytes. SHA-256: 6b40cce43ff9c70caae8ce308a1875ac8f55427d51f1b2579e5728021b48afb9.
- Matching complete source, icon, checksums and testing evidence are attached to the published prerelease. Packaged contentVersion is 01.002.000; the pinned icon has matching param.json beside it.
- Official offline catalog and online release/icon/contentVersion checks pass with zero errors or warnings.
- Static archive scan: zero errors, six warnings and one summary notice. It reports existing sandbox elevation, the loopback loader address and bundled Lapy/self-updater payloads. Both helper hashes need maintainer review; this PR does not edit helpers/approved.json. Automatic updates are disabled.
- This exact integrated build has not been tested on a PS5. Boot, live playback/account consent, 4K/HDR/AV1 throughput, microphone routing/speed and physical controller effects remain unverified. The release is explicitly experimental.

Detailed provenance, user-facing differences, helper hashes/source and validation limits:
https://github.com/HandshakeHackin/youtube-maxd/blob/main/store/SUBMISSION.md

Release:
https://github.com/HandshakeHackin/youtube-maxd/releases/tag/v1.2.0-preview.1

Only apps/PPSA99991.json changes. Maintainers decide whether the derivative meets the distinct-app policy; no upstream refusal or endorsement is claimed.
