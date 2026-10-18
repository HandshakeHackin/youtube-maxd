# ProsperoStore submission

Catalog: [darkxex/ps5-homebrew-catalog](https://github.com/darkxex/ps5-homebrew-catalog).
Store: [prosperostore.net](https://prosperostore.net/).

The proposed listing is [PPSA99991.json](PPSA99991.json), category **Video & TV**.
It targets the existing experimental preview `v1.2.0-preview.1` and the exact
original app ZIP, renamed `PPSA99991.zip` without changing its bytes.
[SUBMISSION.md](SUBMISSION.md) contains provenance, helper disclosures and review details.

The GitHub preview now has all nine verified attachments, including the
required app ZIP and complete source archive. The repository is now **public**.
Official offline and online catalog checks pass with zero errors or warnings.
The owner submitted [catalog PR #397](https://github.com/darkxex/ps5-homebrew-catalog/pull/397). Submission validation,
the release scan and catalog tests passed. It remains open and awaits maintainer
review, merge and deployment; no live store listing is claimed.

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
Tests and offline check also passed. There are no submitted reviews yet.

Maintainers decide whether the fork meets their distinct-app policy and review
the disclosed helper warnings; no upstream refusal, endorsement or helper
approval is claimed. Store availability follows maintainer merge and deployment.

The one-record change is prepared on [HandshakeHackin:add-youtube-maxd-1.2.0-preview.1](https://github.com/HandshakeHackin/ps5-homebrew-catalog/tree/add-youtube-maxd-1.2.0-preview.1).

[Track the submitted catalog pull request](https://github.com/darkxex/ps5-homebrew-catalog/pull/397). The request is open; no duplicate submission is needed.
