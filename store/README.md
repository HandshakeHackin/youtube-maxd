# ProsperoStore preparation

Catalog: [darkxex/ps5-homebrew-catalog](https://github.com/darkxex/ps5-homebrew-catalog).
Store: [prosperostore.net](https://prosperostore.net/).

The proposed listing is [PPSA99991.json](PPSA99991.json), category **Video & TV**.
It targets the existing experimental preview `v1.2.0-preview.1` and the exact
original app ZIP, renamed `PPSA99991.zip` without changing its bytes.
[SUBMISSION.md](SUBMISSION.md) is the prepared reviewer description.

The GitHub preview now has all nine verified attachments, including the
required app ZIP and complete source archive. The repository is now **public**.
Official offline and online catalog checks pass with zero errors or warnings.
No catalog pull request has been opened; store availability follows maintainer
review, merge and deployment.

## Prepared files

- `PPSA99991.json`: current twelve-field record, including `category`.
- `sce_sys/icon0.png` and `sce_sys/param.json`: exact copies of the packaged
  artwork/identity; public catalog metadata is pinned to the downloads commit
  in the record's icon URL, with `param.json` beside it.
- `SUBMISSION.md`: provenance, additional features, helper disclosure and limits.
- `VALIDATION.md`: completed local checks and remaining publication gates.
- `catalog.patch`: the one-record catalog change, ready to apply in a catalog clone.
- `PULL_REQUEST.md`: prepared text for the upstream submission form.
- `../docs/releases/v1.2.0-preview.1/`: testing instructions, recorded validation
  and the official static scan report.
- `../.github/workflows/publish-maxd-preview.yml`: manually invoked publisher
  for the app, matching source, icon, notes, checksums and verification evidence.
  It uses the existing binary and refuses to replace conflicting release files.

The compiled app is still source revision
`014adc2d901518f47fb4d1fbc99eab15b41b79aa`; documentation and distribution
preparation do not change its version or executable.

## Validation

Requirements were checked against catalog commit
`0c44e95fefd170ff4e5be4f941bae9cfd6f67f81` on 2026-10-18.
The title ID was absent from that catalog; it is not reserved by this preparation.

The local ZIP scan passed with **0 errors, 6 warnings and 1 summary notice**.
It reports sandbox elevation, the local loader address, Lapy and the retained
self-updater payload. Both helper hashes need maintainer review. The catalog's
approved-helper list is not changed. Read the full report; a static scan does
not establish PS5 functionality or approve a helper.

To check a fresh catalog checkout after copying the record into `apps/`:

```sh
python3 -m catalog check
python3 -m catalog verify PPSA99991
python3 -m catalog scan PPSA99991 --zip /path/to/PPSA99991.zip
```

`check` validates record structure and uniqueness offline. `verify` additionally
verifies the public repository, release ZIP/GitHub digest, content version and
reachable pinned icon. It passed after the repository was made public. The scanner needs the
catalog's `requirements-scan.txt`. Checks must be rerun against current upstream
before opening a submission.

The store accepts ZIP folder bundles. FFPFSC is not required and is currently
not accepted for new listings. Pre-releases are accepted, so console testing
status stays explicit rather than presenting this preview as a stable release.

## Handoff

The [release publisher run](https://github.com/HandshakeHackin/youtube-maxd/actions/runs/39143346224)
succeeded. The app asset’s GitHub digest matches the proposed record; see the
[published asset receipt](../docs/releases/v1.2.0-preview.1/release-assets.json).

Submit only `apps/PPSA99991.json` to the catalog from **HandshakeHackin**,
the source repository owner. Use the
prepared description and disclose the helper warnings. Reviewers decide whether
the fork meets their distinct-app policy; no upstream refusal or consent is
claimed. Store availability follows maintainer merge and deployment.

The one-record change is prepared on [HandshakeHackin:add-youtube-maxd-1.2.0-preview.1](https://github.com/HandshakeHackin/ps5-homebrew-catalog/tree/add-youtube-maxd-1.2.0-preview.1).

[Open the prepared catalog pull request](https://github.com/darkxex/ps5-homebrew-catalog/compare/main...HandshakeHackin:ps5-homebrew-catalog:add-youtube-maxd-1.2.0-preview.1?expand=1&title=Add+YouTube+Max%E2%80%99d+1.2.0+preview&body=Add+YouTube+Max%E2%80%99d+1.2.0-preview.1+as+a+new+native+PS5+app+listing+%28PPSA99991%2C+video%29.%0A%0AThe+publisher+is+HandshakeHackin%2C+which+owns+the+public+source%2Frelease+repository.+This+is+a+ProsperoTube%2FEVO+derivative%2C+with+original+BlackBearReloaded+and+dependency+credits+retained.+User-facing+additions+include+bundled+on-console+voice+recognition%2C+profile-owned+rule-based+Smart+playlists+and+optional+guarded+Auto+Max+quality.%0A%0A-+Published+ZIP%3A+PPSA99991.zip%2C+99%2C483%2C205+bytes.+SHA-256%3A+6b40cce43ff9c70caae8ce308a1875ac8f55427d51f1b2579e5728021b48afb9.%0A-+Matching+complete+source%2C+icon%2C+checksums+and+testing+evidence+are+attached+to+the+published+prerelease.+Packaged+contentVersion+is+01.002.000%3B+the+pinned+icon+has+matching+param.json+beside+it.%0A-+Official+offline+catalog+and+online+release%2Ficon%2FcontentVersion+checks+pass+with+zero+errors+or+warnings.%0A-+Static+archive+scan%3A+zero+errors%2C+six+warnings+and+one+summary+notice.+It+reports+existing+sandbox+elevation%2C+the+loopback+loader+address+and+bundled+Lapy%2Fself-updater+payloads.+Both+helper+hashes+need+maintainer+review%3B+this+PR+does+not+edit+helpers%2Fapproved.json.+Automatic+updates+are+disabled.%0A-+This+exact+integrated+build+has+not+been+tested+on+a+PS5.+Boot%2C+live+playback%2Faccount+consent%2C+4K%2FHDR%2FAV1+throughput%2C+microphone+routing%2Fspeed+and+physical+controller+effects+remain+unverified.+The+release+is+explicitly+experimental.%0A%0ADetailed+provenance%2C+user-facing+differences%2C+helper+hashes%2Fsource+and+validation+limits%3A%0Ahttps%3A%2F%2Fgithub.com%2FHandshakeHackin%2Fyoutube-maxd%2Fblob%2Fmain%2Fstore%2FSUBMISSION.md%0A%0ARelease%3A%0Ahttps%3A%2F%2Fgithub.com%2FHandshakeHackin%2Fyoutube-maxd%2Freleases%2Ftag%2Fv1.2.0-preview.1%0A%0AOnly+apps%2FPPSA99991.json+changes.+Maintainers+decide+whether+the+derivative+meets+the+distinct-app+policy%3B+no+upstream+refusal+or+endorsement+is+claimed.%0A). Review the form and select **Create pull request** to submit. Opening this link does not itself submit anything.
