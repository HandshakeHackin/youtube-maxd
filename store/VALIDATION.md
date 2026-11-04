# 1.6.0 store validation

- GitHub release v1.6.0 is public/latest, not a draft or prerelease. All seven uploaded asset digests were checked by the publisher.
- Exact candidate 75ee03d9f2f1ef3f30b930412d5890284beac16f passed focused CI and the PS5 package job. Local release-identity, About, updater and PS5 build checks passed.
- Independently downloaded CI ZIP: CRC, unique entries, 0777 modes, title/contentVersion, build label and complete pinned voice model verified. Relabeled bytes reproduce the public asset digest.
- Catalog check: 183 valid records, zero errors/warnings/notices. Online verification: published asset digest and contentVersion 01.006.000 match; zero errors/warnings.
- Only apps/PPSA99991.json changes in the catalog branch. The branch is pushed; upstream PR creation was denied with HTTP 403 by both CLI and connector. Store acceptance is not confirmed.
- Physical console acceptance remains pending. The inherited broad Offline suite is not claimed all green.
