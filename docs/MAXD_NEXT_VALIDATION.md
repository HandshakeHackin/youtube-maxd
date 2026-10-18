# Viewing features validation

Candidate branch: `feature/maxd-next-experience`.

Recorded on 2026-10-18. All account/media responses in these host checks are
fixtures. No Google device authorization, live account access or PS5 connection
was used.

| Check | Result |
| --- | --- |
| Session selection vs exhaustive subset enumeration; invalid budgets/durations | Passed |
| WebVTT/SubRip parsing, malformed clocks, markup, bookmark JSON round trip | Passed |
| Strict command recognition and unknown speech fallback policy | Passed |
| PiP ownership/profile matching, destination rectangle and clear/reset | Passed |
| Provider transcript download, bookmark scopes, session creation | Passed |
| Provider parent PIN, restricted profile exit, actual resolved-channel denial, viewing limit and reload | Passed |
| Real loopback party HTTP pairing/authentication, origin checks, suggestion/vote behavior, approval/rejection, expiry and revocation | Passed |
| Provider party approval, vote ordering, removal preservation and profile revocation | Passed |
| Existing Smart playlist, quality and protected-profile regression checks | Passed |
| Existing JavaScript library suite | 393 passed |
| Local family client/token isolation and party-page URL validation | Passed |
| Embedded service self-test, HTTP, account, import and phone protocol checks | 792 passed |
| Generated background playback ownership, EOF and protected input regression | Passed |
| New Afterglow settings/party/QR boards | 6 boards × 4 looks; 356 checks passed |
| Native PS5 build/package | 312 objects compiled; signed-program integrity valid; 305 ZIP entries verified as 0777 |
| PS5 firmware 11.00 / ShadowMount+ acceptance | Not run; no console attached |

The host sanitizer runs use AddressSanitizer and UndefinedBehaviorSanitizer.
LeakSanitizer is disabled here because the execution sandbox prevents its
process inspection. This is not a claim of PS5 performance, codec acceptance,
working Google linked-profile discovery or controller microphone compatibility.

Candidate implementation: `c9a90784dcd178d5f99b967f4fec7e22048830ba`.

- App ZIP: 99527492 bytes; SHA-256 `e4b65696edf35c38c487b6ad19feedf60d73e14d002fb19d9a3255fbc5405b64`.
- Executable: 55195628 bytes; SHA-256 `19d456d4ff3d4385adba01cbeb6747878b1732ccc299f5703ba1a3f07fd9cc46`.

These hashes describe the earlier local candidate, not the 1.3.0 release.
The published revision raises contentVersion to `01.003.000`, includes the PiP
default-off reset, and has its own [release evidence](releases/v1.3.0-preview.1/TEST_RESULTS.txt).
Exact release hashes are attached in `native-validation.json` and `SHA256SUMS`.
