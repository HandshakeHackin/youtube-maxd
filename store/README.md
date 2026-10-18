# ProsperoStore update

YouTube Max’d is already listed as **PPSA99991** in the **Video & TV** category:
[store page](https://prosperostore.net/app/PPSA99991/).
The original listing was approved in [PR #397](https://github.com/darkxex/ps5-homebrew-catalog/pull/397).

This directory tracks the **1.3.0-preview.1 update**, with the six viewing
features and **PiP off by default**. Content version rises to `01.003.000` so
consoles can detect the update; title ID and app data location stay the same.
The new release remains experimental and has not been tested on PS5 hardware.

- [PPSA99991.json](PPSA99991.json): exact public release ZIP, SHA-256 and tagged icon.
- [sce_sys](sce_sys): metadata and icon matching the packaged app.
- [SUBMISSION.md](SUBMISSION.md): changes, provenance and submission status.
- [VALIDATION.md](VALIDATION.md): checks and remaining hardware limits.
- [PULL_REQUEST.md](PULL_REQUEST.md): upstream update request text.
- [catalog.patch](catalog.patch): changes only `apps/PPSA99991.json`.
- [Release notes](../docs/releases/v1.3.0-preview.1.md) and
  [testing evidence](../docs/releases/v1.3.0-preview.1/TEST_RESULTS.txt).

The update was approved and merged in [PR #417](https://github.com/darkxex/ps5-homebrew-catalog/pull/417) on 2026-10-18.
The [store page](https://prosperostore.net/app/PPSA99991/) and update API now
serve **1.3.0-preview.1 / 01.003.000** with the verified release ZIP and digest.
Submission checks and [website deployment](https://github.com/darkxex/ps5-homebrew-catalog/actions/runs/39162533006) passed.
The original `v1.2.0-preview.1` release remains available on GitHub.
A catalog merge or static scan does not establish PS5 functionality or separate
helper-whitelist approval.

## Verify against current upstream

Copy the record into a fresh catalog checkout, then run:

```sh
python3 -m catalog check
python3 -m catalog verify PPSA99991
python3 -m catalog scan PPSA99991 --zip /path/to/PPSA99991.zip
```

Scanner dependencies are in the catalog's `requirements-scan.txt`. Published
release assets remain immutable. The upstream catalog owns review and deployment.
