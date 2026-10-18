# Store update validation — 2026-10-18

Release: **v1.3.0-preview.1** · title **PPSA99991** · content **01.003.000**.
Native application source: `78718a740137f1a111aa73926d74d22d8af8b1d4`.
Catalog tooling revision: `c3e61941fda62b4624f088a0f5344ff729bcd051`.

| Check | Result |
| --- | --- |
| Native PS5 compilation and signed-program integrity | Passed; fresh build from the application source above |
| App ZIP | 99,519,618 bytes; 305 entries; CRCs and 0777 modes verified |
| Identity/content version | PPSA99991 / 01.003.000; higher than 01.002.000 |
| Artwork/metadata | Exact packaged copies in store/sce_sys; 512×512 PNG |
| PiP behavior | Defaults off; fresh session reset and explicit activation covered by ASan/UBSan tests |
| New viewing policies and party HTTP/page checks | Passed |
| Updated service HTTP fixtures | 186 passed, zero failed |
| Release/version/generated About changelog checks | Passed |
| Official catalog unit suite | 176 tests passed, two skipped |
| Offline catalog check | 98 valid records, zero errors/warnings/notices |
| Catalog record | Twelve fields; only version, artifact URL, SHA-256 and tagged icon URL change |
| Full matching sources/checksums | Attached to the published release; exact verified manifest count in native-validation.json |
| Official static scan | Passed: 0 errors, 6 warnings, 1 summary notice; [full report](../docs/releases/v1.3.0-preview.1/catalog-scan.txt) |
| Public GitHub release | Published; nine assets with matching GitHub digests/checksums; [publisher succeeded](https://github.com/HandshakeHackin/youtube-maxd/actions/runs/39157826519) |
| Online catalog verification | Passed: zero errors/warnings; ZIP digest, tagged icon and contentVersion 01.003.000 verified |
| Store pull request | Prepared branch pushed; creation blocked by GitHub integration permission (403); [prefilled form](SUBMISSION.md#submit-the-update) |
| PS5/live YouTube/Google acceptance | Not performed; this remains an experimental prerelease |

App SHA-256: `22b4bf2c8b7fa67f3e11198645fab9746f7381170b12dfc82ce709814792e593`.

Retained helpers:

- `lapy.elf`: `b0623679f92298f08983b2b61bbf322b6bfa032a12befdc1b3661b76fa5bc1a4`.
- `self-updater.elf`: `36a230285f413a51cfb18a0b38eb37501c24caab35abdaf40a7b2f9dcbb6dd4f`.

The update does not modify the catalog's approved-helper list. The static report
discloses sandbox elevation, the local loader and payload behavior; a passing
scan/catalog merge is not PS5 acceptance or separate helper-whitelist approval.
Original listing approval remains [PR #397](https://github.com/darkxex/ps5-homebrew-catalog/pull/397).

Published receipts: [release-assets.json](../docs/releases/v1.3.0-preview.1/release-assets.json)
and [native-validation.json](../docs/releases/v1.3.0-preview.1/native-validation.json).
All 4,442 published corresponding-source manifest entries were verified by CI.
