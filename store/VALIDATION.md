# Store preparation validation — 2026-10-18

Catalog revision: `0c44e95fefd170ff4e5be4f941bae9cfd6f67f81`.
App source: `014adc2d901518f47fb4d1fbc99eab15b41b79aa`.

| Check | Result |
| --- | --- |
| Official offline catalog check, including proposed entry and existing catalog | Passed: 95 records, zero errors/warnings/notices |
| Title ID | `PPSA99991` absent from checked upstream catalog; not reserved by this preparation |
| Catalog record | Twelve fields; `video` category; version matches `v1.2.0-preview.1` |
| App ZIP | 303 entries; CRCs, 0777 modes, identity and original SHA-256 verified |
| Executable / voice model | Match the original preview's pinned hashes |
| Artwork / metadata | 512×512 icon and param.json match app bytes; metadata copies pushed at downloads commit `df694f5aa53906fe84e26e857542076ceb807577` |
| Complete local source distribution | All 4,388 manifest file hashes and source archive CRCs verified |
| Official static catalog ZIP scan | Passed: zero errors, six warnings and one summary notice; full report retained |
| GitHub release publication | [Publisher succeeded](https://github.com/HandshakeHackin/youtube-maxd/actions/runs/39143346224); all nine assets attached; app URL and GitHub digest match the catalog record |
| Published complete source | CI verified 4,388 manifest files; SHA-256 `5cee83a5d64f16be306ddefd08705220f2240b7ee6c77055b8ed56e955943d59` |
| Current online catalog verification | Blocked: source_repo is private; no public-verification pass claimed |
| Public submission / deployment | Not performed |
| PS5 hardware acceptance | Not performed; this exact integrated build remains experimental |

The scanner's six warnings cover sandbox elevation, the local payload-loader
address, two bundled payloads and the two helper hashes absent from the current
approved list. Approval remains a catalog-maintainer task. See
[the complete report](../docs/releases/v1.2.0-preview.1/catalog-scan.txt).

The ZIP was not rebuilt or altered for store naming. Its SHA-256 is
`6b40cce43ff9c70caae8ce308a1875ac8f55427d51f1b2579e5728021b48afb9`.
The prepared [catalog.patch](catalog.patch) changes only `apps/PPSA99991.json`;
rebase/recheck against current upstream before submission.
