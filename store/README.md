# ProsperoStore submission

Catalog: [darkxex/ps5-homebrew-catalog](https://github.com/darkxex/ps5-homebrew-catalog).
Store: [prosperostore.net](https://prosperostore.net/).

The catalog listing is [PPSA99991.json](PPSA99991.json), category **Video & TV**.
It targets the existing experimental preview `v1.2.0-preview.1` and the exact
original app ZIP, renamed `PPSA99991.zip` without changing its bytes.
[SUBMISSION.md](SUBMISSION.md) contains provenance, helper disclosures and review details.

The GitHub preview now has all nine verified attachments, including the
required app ZIP and complete source archive. The repository is now **public**.
Official offline and online catalog checks pass with zero errors or warnings.
The owner submitted [catalog PR #397](https://github.com/darkxex/ps5-homebrew-catalog/pull/397). Submission validation,
the release scan and catalog tests passed. The PR was approved and merged on
2026-10-18. The [store listing](https://prosperostore.net/app/PPSA99991/) is live
with `1.2.0-preview.1`, verified on 2026-10-18.

The six newer experience features remain in the development candidate. This
store listing continues to use the original preview; no newer binary or catalog
update has been published by the feature work.

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
The title ID was absent at preparation; it is now listed by the merged record.

The local ZIP scan passed with **0 errors, 6 warnings and 1 summary notice**.
It reports sandbox elevation, the local loader address, Lapy and the retained
self-updater payload. The helper hashes were not in the approved list at that preparation revision.
This preparation did not change the catalog's approved-helper list. Read the full report; a static scan does
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
catalog's `requirements-scan.txt`. Recheck against current upstream
if the submission changes.

The store accepts ZIP folder bundles. FFPFSC is not required and is currently
not accepted for new listings. Pre-releases are accepted, so console testing
status stays explicit rather than presenting this preview as a stable release.

## Submission status

The [release publisher run](https://github.com/HandshakeHackin/youtube-maxd/actions/runs/39143346224)
succeeded. The app asset’s GitHub digest matches the proposed record; see the
[published asset receipt](../docs/releases/v1.2.0-preview.1/release-assets.json).

[Catalog PR #397](https://github.com/darkxex/ps5-homebrew-catalog/pull/397) was opened by **HandshakeHackin**, the source
repository owner, on 2026-10-18. It adds only `apps/PPSA99991.json`.
[Validate submission and Scan the release](https://github.com/darkxex/ps5-homebrew-catalog/actions/runs/39144729795) passed; the catalog
Tests and offline check also passed. The request was approved and merged on
2026-10-18; the merged catalog record matches the prepared artifact URL and SHA-256.

Maintainers approved the catalog submission; the disclosed scan warnings remain
part of the review history. A live listing does not establish PS5 hardware
acceptance, upstream endorsement or separate helper-whitelist approval.

The one-record change was prepared on [HandshakeHackin:add-youtube-maxd-1.2.0-preview.1](https://github.com/HandshakeHackin/ps5-homebrew-catalog/tree/add-youtube-maxd-1.2.0-preview.1).

[Track the submitted catalog pull request](https://github.com/darkxex/ps5-homebrew-catalog/pull/397). The request is merged and the listing is live; no duplicate submission is needed.
