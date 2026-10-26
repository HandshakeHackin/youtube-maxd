Update YouTube Max’d (PPSA99991) from 1.4.1 to 1.4.2.

# YouTube Max’d 1.4.2

### Fixed
- L3 voice search: correct a model-verification stack overflow by moving its 64 KiB buffer to the heap and giving recognition a checked 4 MiB thread stack. Thread setup errors are handled safely; full model SHA verification stays enabled.
- Voice model discovery: support the existing title-specific installation locations as well as /app0, including /data/homebrew/PPSA99991.
- Russian text: extend Cyrillic fonts to status messages, logs and chapter titles. The reporter confirmed these text corrections.
- Signed-in Home: later pages now show playable videos from supported shelves instead of category folders. Preserve content filters, watched preferences, duplicate removal and account-specific pagination.

### Install
Download PPSA99991.zip, extract it, and replace the complete PPSA99991 application folder using your normal installer. Include all voice assets. Title ID and app data location are unchanged; picture-in-picture remains off by default.

### Validation and feedback
Targeted host regressions, native PS5 compilation and package checks are documented in TEST_RESULTS.md. The broader inherited offline suite is not fully passing. No PS5 hardware confirmation has been performed for this release. Please test L3 voice search and signed-in Home pages beyond the first page and report back on issues #1 and #2, which remain open for confirmation.

Based on ProsperoTube and EVO Player; upstream credits, licenses and corresponding sources are retained.

Only apps/PPSA99991.json changes. Submitted by HandshakeHackin, owner of the source repository. The record pins the published release ZIP and its GitHub SHA-256 digest. Title ID is unchanged; contentVersion rises from 01.004.001 to 01.004.002. Matching source, licenses and validation receipts accompany the release. No helper approvals are modified.

Published app ZIP: 101,813,655 bytes. SHA-256: `a0bc2b8d13425eae776324ee1d01064ecc4816b47db1f65cede129fbfe8e0d9b`.
