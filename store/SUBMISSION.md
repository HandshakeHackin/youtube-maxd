# Catalog update — YouTube Max’d 1.3.0-preview.1

**Status:** approved and merged as [catalog PR #417](https://github.com/darkxex/ps5-homebrew-catalog/pull/417) on
2026-10-18 at 20:16 UTC. Submission validation, release scan, offline tests and
automatic release-bump checks passed. The [deployment](https://github.com/darkxex/ps5-homebrew-catalog/actions/runs/39162533006) also passed.
The [store page](https://prosperostore.net/app/PPSA99991/) and update API now
serve **1.3.0-preview.1**, content version **01.003.000**, with the expected ZIP
URL and SHA-256; verified on 2026-10-18.

Original listing: [approved PR #397](https://github.com/darkxex/ps5-homebrew-catalog/pull/397).
[Current store page](https://prosperostore.net/app/PPSA99991/).

**Title ID:** PPSA99991 · **Type:** update · **Category:** video.
**Publisher/source owner:** HandshakeHackin.
**Content version:** `01.003.000` (previously `01.002.000`).

Native standalone PS5 YouTube client for firmware 11.00 / ShadowMount+.
Runs its embedded YouTube service and speech recognition on the console.
No PC/NAS companion or developer Google Cloud project is required.

## Changes

- Transcript search, timestamp seeking and profile-owned bookmarks.
- Optional Afterglow in-app PiP: **off by default**, explicitly selected per
  session through R2 → Picture-in-picture; never restored automatically.
- Local push-to-talk playback commands.
- Time-budget viewing session builder.
- Four independent signed-out family profiles with parent PIN, approved
  channels, search restriction and daily viewing-time limits.
- PS5-hosted LAN party queue with QR joining, suggestions, votes and host approval.

Full [release changelog](../docs/releases/v1.3.0-preview.1.md).
Linked Google supervised-profile import remains research. These local family
slots do not impersonate Google child identities or provide YouTube Kids certification.

## Provenance and licensing

Based on **ProsperoTube by BlackBearReloaded and contributors**, pinned at
`3446f364172c299a17cecf29a3efcf23c8463f68`, with **EVO Player**
`9715f6ee9bf9b53461d6fdf9a2e11b4c48357783`. GPL-3.0-or-later; upstream and
component notices remain in the app and complete corresponding-source archive.
The app's player, account flow and community integrations remain credited upstream.
No upstream endorsement or refusal to accept features is claimed.

## Artifact and helpers

Published release: https://github.com/HandshakeHackin/youtube-maxd/releases/tag/v1.3.0-preview.1

The record pins the immutable `PPSA99991.zip` attachment and its exact SHA-256.
The tag's `store/sce_sys` files match the app's icon and metadata. Matching
complete sources, changelog, checksums, test guide/results and static scan are
attached to the same release. No original release asset is overwritten.

The app retains the exact-title Lapy startup/elevation helper and the inherited
self-updater payload. Automatic in-app updating remains disabled. Helper hashes
are recorded in `native-validation.json`; corresponding source is retained in
`runtime-sources/Lapy-57fcc19.tar`, `native/update/helper` and `native/update/kit`.
The static scan reports sandbox-elevation/local-loader/payload behavior. This
update changes only the app's catalog record, not `helpers/approved.json`.
No separate helper approval is asserted.

## Validation and practical limits

A fresh native PS5 compilation, signed-container inspection, package metadata,
0777 permissions, CRCs, model/icon integrity, complete source-manifest hashes
and official catalog checks are required for this revision. Recorded host tests
cover the new viewing tools, family isolation/controls, party queue and background
playback. [Validation details](VALIDATION.md) distinguish repeated checks from
previous development results.

This exact build has not been tested on a PS5. Boot, live YouTube playback and
Google consent, PiP codec/HDR behavior, microphone routing/latency and physical
controller effects remain unverified. The release is an experimental prerelease.
Party rooms use HTTP on a trusted LAN; local parental controls can be bypassed
by editing app data. Ad removal and account-required playback are not guaranteed.
Independent homebrew, unaffiliated with Google, YouTube or Sony.

<a id="submit-the-update"></a>

## Submission status

[Request #417](https://github.com/darkxex/ps5-homebrew-catalog/pull/417) was submitted by **HandshakeHackin**, approved and
merged. It changes only `apps/PPSA99991.json`; the merged record matches
[PPSA99991.json](PPSA99991.json) exactly. The store update is live.

[Store deployment receipt](../docs/releases/v1.3.0-preview.1/store-deployment.json).
The earlier integration permission issue was resolved by owner submission.
