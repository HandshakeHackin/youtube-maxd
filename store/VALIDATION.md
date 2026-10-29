# 1.5.0 store validation

- Full PS5 package job passed for source 001a848fb7dcfcfc0d9aa64331db64a06502b611; the focused feature workflow also passed for that source.
- Stable GitHub release v1.5.0 is public/latest, not a draft or prerelease. All seven uploaded asset SHA-256 digests were checked before publication.
- Published native-validation.json, SHA256SUMS and RELEASE_NOTES.md were independently downloaded and verified against their GitHub digests.
- Local PS5 build passed: 305 entries, all 0777, CRC/title/version checks passed. The published CI package has its CI build label replaced in place; all other executable and asset bytes are retained.
- Catalog offline check: 157 valid records, zero errors/warnings/notices. Online verify: the published asset digest and contentVersion 01.005.000 match, zero errors/warnings. The catalog check reads GitHub asset metadata; it does not download the entire ZIP.
- Exact app bytes: 101821490; SHA-256: 6687af734b1a102a00938cdaf3c81d18da6874245237eaafc85d764f6cabdf0c.
- Physical PS5 acceptance remains pending. Repository-wide test success is not claimed.
- Store update branch commit: 5bb203e. Upstream PR creation is blocked by integration permissions (HTTP 403); see SUBMISSION.md.
