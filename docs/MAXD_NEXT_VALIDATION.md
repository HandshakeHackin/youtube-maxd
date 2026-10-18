# Next experience validation

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
| Native PS5 build/package | Pending final build |
| PS5 firmware 11.00 / ShadowMount+ acceptance | Not run; no console attached |

The host sanitizer runs use AddressSanitizer and UndefinedBehaviorSanitizer.
LeakSanitizer is disabled here because the execution sandbox prevents its
process inspection. This is not a claim of PS5 performance, codec acceptance,
working Google linked-profile discovery or controller microphone compatibility.
