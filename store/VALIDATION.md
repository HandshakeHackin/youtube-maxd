# Store update validation — 2026-10-19

Release **v1.4.0** · title **PPSA99991** · content **01.004.000**.
Native source: `1eb9b8b358087738bc8a331ee2e612d73391a14b`.
Catalog tooling: `54e7f5a7cdc95e5ad589dcec4efead6f3f57bb12`.

- Native compile: 314 objects; signed-program integrity valid.
- ZIP: 99,530,570 bytes, 305 entries; CRC and 0777 permissions verified.
- SHA-256: `16a4b5960f1369fd593cd17852bd04c940d2febfa535094a8a647a40e0ca741e`.
- Metadata/icon: exact packaged copies in store/sce_sys; unchanged 512×512 icon.
- Remote/report HTTP, generated provider, C++ player adapter and closed report
  allowlist/private export: ASan/UBSan tests passed.
- UI: 1,132 regression checks, plus 400 final phone/report fixture checks, zero failed.
- Embedded service: 792 plain host checks passed across selftest/HTTP/accounts/import/casting.
- Generated About, versions, fork identity/lightbar and background lifecycle: passed.
- Official static scan: passed, zero errors, six warnings, one notice; see
  [full report](../docs/releases/v1.4.0/catalog-scan.txt). Warnings disclose the
  retained elevation/loader/helpers and do not grant helper approval.
- Only four release-bump fields change in the catalog record.
- Full source/checksum and static scan evidence are shipped with the release.
- Online verification follows publication; actual state is in SUBMISSION.md.
- No PS5, live Google account or live YouTube acceptance testing performed.

Helper SHA-256 values:

- `lapy.elf`: `b0623679f92298f08983b2b61bbf322b6bfa032a12befdc1b3661b76fa5bc1a4`.
- `self-updater.elf`: `36a230285f413a51cfb18a0b38eb37501c24caab35abdaf40a7b2f9dcbb6dd4f`.

Static scan/helper metadata does not establish device functionality or separate
helper-whitelist approval. Source/credit for ProsperoTube/EVO is retained.
