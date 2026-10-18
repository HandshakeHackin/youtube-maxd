# Catalog submission — YouTube Max’d

**Status:** approved and merged as [catalog PR #397](https://github.com/darkxex/ps5-homebrew-catalog/pull/397) on
2026-10-18. Submission validation, the release scan and catalog tests passed.
The [YouTube Max’d store listing](https://prosperostore.net/app/PPSA99991/) is live
and shows `1.2.0-preview.1`; verified on 2026-10-18.

This listing uses the original preview. The six newer experience features remain
in the development candidate and are not included in this store download.

**Title ID:** PPSA99991

**New listing, update or reservation:** New listing

**What the app does:** YouTube Max’d is a standalone native PS5 YouTube client
based on ProsperoTube. Search, extraction, playback and local voice recognition
run on the console. Intended setup: firmware 11.00 / ShadowMount+.

**Category:** video

**Fork or derivative of an app in the catalog?** Yes: ProsperoTube, original
copyright (C) 2026 BlackBearReloaded, base
`3446f364172c299a17cecf29a3efcf23c8463f68`, with EVO Player
`9715f6ee9bf9b53461d6fdf9a2e11b4c48357783`.

## User-facing differences

The source adds bundled, on-console multilingual Whisper voice search; local,
profile-owned Smart playlists with feed/title/creator/duration/watched/live
rules; and optional Auto Max adaptation to sustained buffering/dropped frames,
with guarded upgrades and manual-choice/quality/FPS/codec limits. It also adds
saved per-profile maximum resolution, separate state/identity, red-to-white
artwork and DualSense feedback.

The player, embedded YouTube.js service, account/device-code flow,
Classic/Afterglow foundation and integrations such as SponsorBlock/DeArrow
remain credited upstream work. The additional features are implemented code,
not merely artwork/default changes. Acceptance under the catalog's distinct-app
policy is for maintainers to determine; no claim is made that upstream refused
these features or endorsed this fork.

## Readiness

- [x] Native PS5 title with `eboot.bin` and `sce_sys/`; not a standalone payload,
  PS4 package or backport.
- [x] App bundle is a ZIP with only `PPSA99991/` at its top level.
- [x] Local byte integrity, 0777 permissions, packaged identity, model and icon
  are verified. Content version is `01.002.000`.
- [x] Proposed version `1.2.0-preview.1` matches the release tag without `v`.
- [x] Twelve-field record uses the `video` category and a pinned 512×512 PNG.
- [x] Source/provenance, GPL and dependency notices are retained. The complete
  source archive is attached; release assembly checked all 4,388 manifest files.
- [x] Review policy read; helper behavior and console-testing limits disclosed.
- [x] Public source repository: confirmed public on 2026-10-18.
- [x] `PPSA99991.zip` attached to the published release, with GitHub's digest
  verified against the record. A raw Git branch download is not accepted instead.
- [x] Full online catalog verification of the public repository, exact asset,
  content version and icon.
- [x] [PR #397](https://github.com/darkxex/ps5-homebrew-catalog/pull/397) opened from HandshakeHackin, the owner of `source_repo`.
- [x] Upstream submission validation, release scan and catalog tests passed.
- [x] Maintainer approval and catalog merge.
- [x] Live store deployment verified at `https://prosperostore.net/app/PPSA99991/`.

Release: https://github.com/HandshakeHackin/youtube-maxd/releases/tag/v1.2.0-preview.1

Artifact: `PPSA99991.zip`, 99,483,205 bytes.

SHA-256: `6b40cce43ff9c70caae8ce308a1875ac8f55427d51f1b2579e5728021b48afb9`

This is byte-identical to the existing `YouTube-Maxd-PS5.zip`. No already
listed artifact is replaced. The icon's pinned commit also carries the exact
packaged `sce_sys/param.json`, so the catalog can read `contentVersion`.

## Helpers and scan

The official catalog scanner at
`0c44e95fefd170ff4e5be4f941bae9cfd6f67f81` read the ZIP locally without executing
it: **0 errors, 6 warnings and 1 summary notice**. The full report is
`docs/releases/v1.2.0-preview.1/catalog-scan.txt` in the app repository.

- The main title can request sandbox elevation through `elevate_proc` and
  contains the loopback payload-loader address.
- `lapy.elf`, SHA-256
  `b0623679f92298f08983b2b61bbf322b6bfa032a12befdc1b3661b76fa5bc1a4`, is the
  exact-title startup/elevation helper. Build integration:
  `scripts/build_lapy_helper.py`; client: `native/elevation/`; matching pinned
  helper source: `runtime-sources/Lapy-57fcc19.tar`.
- `self-updater.elf`, SHA-256
  `36a230285f413a51cfb18a0b38eb37501c24caab35abdaf40a7b2f9dcbb6dd4f`, is the
  retained inherited update payload. Its source is `native/update/helper/` and
  `native/update/kit/`. Automatic updates are disabled in this Max’d build.
- Both helper hashes were absent from the approved list at the checked
  preparation revision. This preparation did not edit `helpers/approved.json`
  or assert separate helper-whitelist approval.

## Validation and limits

Native compilation, executable/container/package checks, fixture-based service
and account tests, real-model host recognition, Smart playlist/Auto Max tests,
controller-bridge tests and UI fixtures passed during development. Release
assembly verifies the app/source hashes and bundled model/icon. These results
are recorded separately from the catalog's static scan.

This exact integrated build has not been tested on a PS5. Firmware 11.00 boot,
live YouTube extraction/playback and Google consent, codecs/4K/HDR/AV1 throughput,
mic routing/speed, actual DualSense effects, live Smart playlist feeds and Auto
Max decisions, casting and background audio remain unverified. Upstream's
hardware acceptance is not this fork's acceptance. Account-required playback
is off by default; ad removal is not guaranteed.

Full source, attribution and test details: app repository README,
`UPSTREAM.json`, `THIRD_PARTY_NOTICES.md`, `MAXD_CHANGES.md` and
`docs/MAXD_VALIDATION.md`. Independent homebrew, not an official Google,
YouTube, Sony or ProsperoTube release.
