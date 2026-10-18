# Catalog update — YouTube Max’d 1.3.0-preview.1

**Status:** GitHub release published and verified. The update branch is pushed and
passes online catalog verification. Automatic pull-request creation is blocked:
GitHub returns **403 — Resource not accessible by integration** for this upstream
repository through both the CLI and connected GitHub app. The store still lists
the earlier version until the update is submitted and merged.

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

## Submit the update

[Open the prefilled store update](https://github.com/darkxex/ps5-homebrew-catalog/compare/main...HandshakeHackin:update-youtube-maxd-1.3.0-preview.1?expand=1&title=Update+YouTube+Max%E2%80%99d+to+1.3.0-preview.1&body=%2A%2ATitle+ID%3A%2A%2A+PPSA99991%0A%2A%2ANew+listing%2C+update+or+reservation%3A%2A%2A+update%0A%2A%2AWhat+the+app+does%3A%2A%2A+Native+standalone+PS5+YouTube+client+based+on+ProsperoTube%2FEVO.+This+preview+adds+six+viewing+tools+and+keeps+PiP+off+by+default.%0A%2A%2ACategory%3A%2A%2A+video%0A%2A%2AFork+or+derivative+of+an+app+in+the+catalog%3F%2A%2A+Yes%3A+ProsperoTube+by+BlackBearReloaded+and+contributors.+Existing+derivative+listing+approved+in+%23397%3B+source+and+licenses+remain+credited.%0A%0A%23%23+Changes%0A%0A-+Transcript+search%2C+timestamp+seeking+and+local+per-profile+bookmarks.%0A-+Optional+Afterglow+in-app+PiP%2C+%2A%2Aoff+by+default%2A%2A+and+explicitly+selected+per+session.%0A-+Local+push-to-talk+playback+commands+and+time-budget+session+builder.%0A-+Four+signed-out+family+profiles+with+PIN%2Fchannel%2Fsearch%2Ftime+controls.%0A-+PS5-hosted+LAN+party+queue+with+QR+joining%2C+suggestions%2Fvotes+and+host+approval.%0A-+Content+version+raised+from+%6001.002.000%60+to+%6001.003.000%60%2C+retaining+title%2Fdata+identity.%0A%0ARelease%2Fchangelog%3A+https%3A%2F%2Fgithub.com%2FHandshakeHackin%2Fyoutube-maxd%2Freleases%2Ftag%2Fv1.3.0-preview.1%0A%0AImmutable+ZIP%3A+%60PPSA99991.zip%60%2C+99%2C519%2C618+bytes.%0ASHA-256%3A+%6022b4bf2c8b7fa67f3e11198645fab9746f7381170b12dfc82ce709814792e593%60.%0AThe+tag%27s+%60store%2Fsce_sys%60+metadata%2Ficon+match+the+packaged+app.+Complete+sources%2C%0Achecksums%2C+testing+instructions%2Fresults+and+the+official+static+scan+accompany%0Athe+release.+The+old+release+assets+remain+untouched.%0A%0AThis+exact+build+has+%2A%2Anot%2A%2A+been+tested+on+PS5+hardware+or+a+live+Google+account.%0ABoot%2C+real+playback%2Fsign-in%2C+PiP+codecs%2FHDR%2C+microphone+latency%2Frouting+and%0Aphysical+controller+behavior+still+need+acceptance.+Linked+Google+child-profile%0Aimport+remains+research.+Local+family+limits+are+not+YouTube+Kids+certification%3B%0Athe+party+page+uses+HTTP+on+a+trusted+LAN.%0A%0AThe+app+retains+its+disclosed+Lapy%2Felevation+and+self-updater+payloads%3B+automatic%0Aupdating+remains+disabled.+Scan%3A+0+errors%2C+6+warnings+and+1+summary+notice%3B+full+findings+and+helper+hashes+are+attached.%0AThis+request+does+not+change+%60helpers%2Fapproved.json%60+or+claim+helper+approval.%0A%0A-+%5Bx%5D+Native+PS5+app+with+its+own+eboot.bin+and+sce_sys%3B+not+a+payload+or+PS4+package.%0A-+%5Bx%5D+Submitted+by+HandshakeHackin%2C+owner+of+source_repo.%0A-+%5Bx%5D+Artifact+is+the+app-folder+ZIP+for+PPSA99991.%0A-+%5Bx%5D+Published+experimental+GitHub+release%2C+matching+tag%2Fversion+and+exact+ZIP+digest.%0A-+%5Bx%5D+Asset+remains+immutable%3B+original+release+remains+available.%0A-+%5Bx%5D+Higher%2C+matching+contentVersion+is+committed+at+the+release+tag.%0A-+%5Bx%5D+Video+category%2C+original+license+and+app+identity+retained.%0A-+%5Bx%5D+Distribution+rights%2C+corresponding+sources+and+dependency+notices+retained.%0A-+%5Bx%5D+Existing+distinct-app+listing%3B+review+policy+read%2C+no+upstream+endorsement+claimed.%0A%0AOnly+%60apps%2FPPSA99991.json%60+changes%2C+limited+to+the+four+release-bump+fields.%0A) and click **Create pull request**
while signed in as **HandshakeHackin**. The one-record change, title and full
submission text are prepared; no file upload is needed.

If GitHub leaves the description blank, copy [PULL_REQUEST.md](PULL_REQUEST.md).
The branch is `HandshakeHackin:update-youtube-maxd-1.3.0-preview.1`.
