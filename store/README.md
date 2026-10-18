# ProsperoStore preparation

Catalog: [darkxex/ps5-homebrew-catalog](https://github.com/darkxex/ps5-homebrew-catalog).
Store: [prosperostore.net](https://prosperostore.net/).

The proposed listing is [PPSA99991.json](PPSA99991.json), category **Video & TV**.
It targets the existing experimental preview `v1.2.0-preview.1` and the exact
original app ZIP, renamed `PPSA99991.zip` without changing its bytes.
[SUBMISSION.md](SUBMISSION.md) is the prepared reviewer description.

The GitHub preview now has all nine verified attachments, including the
required app ZIP and complete source archive. The repository is currently
private; the catalog requires a public repository, then publisher verification,
maintainer review, merge and deployment.
Repository visibility has not been changed. No catalog pull request has been opened.

## Prepared files

- `PPSA99991.json`: current twelve-field record, including `category`.
- `sce_sys/icon0.png` and `sce_sys/param.json`: exact copies of the packaged
  artwork/identity; public catalog metadata is pinned to the downloads commit
  in the record's icon URL, with `param.json` beside it.
- `SUBMISSION.md`: provenance, additional features, helper disclosure and limits.
- `VALIDATION.md`: completed local checks and remaining publication gates.
- `catalog.patch`: the one-record catalog change, ready to apply in a catalog clone.
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
requires the public repository, release ZIP/GitHub digest and reachable pinned
icon; it cannot pass while this repository is private. The scanner needs the
catalog's `requirements-scan.txt`. Checks must be rerun against current upstream
before opening a submission.

The store accepts ZIP folder bundles. FFPFSC is not required and is currently
not accepted for new listings. Pre-releases are accepted, so console testing
status stays explicit rather than presenting this preview as a stable release.

## Handoff

The [release publisher run](https://github.com/HandshakeHackin/youtube-maxd/actions/runs/39143346224)
succeeded. The app asset’s GitHub digest matches the proposed record; see the
[published asset receipt](../docs/releases/v1.2.0-preview.1/release-assets.json).

Once publication prerequisites are satisfied, submit only `apps/PPSA99991.json`
to the catalog from **HandshakeHackin**, the source repository owner. Use the
prepared description and disclose the helper warnings. Reviewers decide whether
the fork meets their distinct-app policy; no upstream refusal or consent is
claimed. Store availability follows maintainer merge and deployment.
