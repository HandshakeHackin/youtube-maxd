# Companion protocol v1

A native source is an HTTP(S) LAN origin with optional `#123456` PIN. The PIN
stays in memory until `/v1/pair` exchanges it for the stable bearer token;
the client stores the token and normalized origin. PIN lifetime: ten minutes,
failed attempts limited per IP. Restarting the host prints a new PIN without
rotating the token.

`/v1` routes except health/pair require `Authorization: Bearer TOKEN`.
The native client posts `/v1/jobs` with action search/catalog/details/resolve,
polls `/v1/jobs/{id}`, and cancels with DELETE. Responses are pending, done with
result, or failed with sanitized error. This avoids extraction blocking the
native six-second HTTP timeout. Direct search/catalog/video/resolve endpoints
are available for host tools. Native completions are generation-checked.

Catalog identities are video/channel/playlist IDs, never URLs. Pages include
bounded metadata and has_more. Resolve selects height, codec, audio/caption
language and automatic captions; returns choices, captions and chapters.
Each adaptive choice pairs one video/audio track; the client uses the preferred
first choice. Preferences apply to the next opened video.

`/media/{random-session}/{resource}` is an expiring capability for FFmpeg and
captions, requiring no bearer header. The relay preserves single byte ranges,
scopes sensitive headers to origins, limits redirects and restricts resolver
URLs to known HTTPS YouTube domains. Separate full-file streams become static
DASH. HLS links/keys become bounded relay capabilities. WebVTT becomes SRT
without changing timestamps. HLS keys retain file suffixes for FFmpeg extension
validation. The PS5 checks resolved/caption URLs against its configured origin.

Limits: 8 KiB request bodies, 8 MiB extractor JSON, two workers, 45-second worker
deadline, 16 jobs, 32 sessions, 30-minute session expiry and 4,096 HLS resources.
Expired upstream access returns a sanitized error; reselecting the ID refreshes.

This is a trusted-LAN service. HTTP is plaintext; pairing does not replace TLS.
Do not expose it to the public internet. PS5 state contains a bearer credential
without application-level encryption. Rotate by stopping the companion,
removing its token file, restarting and pairing again. Cookies/proof-token
arguments are explicitly supplied host configuration. Extractor stderr and
access logs are disabled. SponsorBlock uses a hash prefix but still sees the
request network origin. Local lists never mutate Google state.

Device sign-in backend (requires `--library-node` and the LAN bearer token):

- POST `/v1/auth/start` with `{"name":"YouTube account"}` starts one session.
- POST `/v1/auth/status` with `{"session":"32 lowercase hex characters"}`
  reads state and, when due, performs one bounded upstream token poll.
- POST `/v1/auth/cancel` with the same session cancels pending authorization.

Pending responses contain `session`, `state:"pending"`, `user_code`,
`verification_url`, `expires_in` and `poll_after` (seconds). Display the code and
URL, and wait at least `poll_after` before requesting status again. Authorized
responses contain `session`, `state:"authorized"` and `profile:{id,name}`.
Other terminal states are `canceled`, `denied`, `expired` and `failed`; failed
responses include a fixed public error. Start a fresh session after a failure.
Cancellation after authorization does not remove or revoke the account.
Device codes, client secrets, access/refresh tokens never appear in HTTP responses.
These direct endpoints currently serve host tools; native jobs/sign-in remain
pending and must account for upstream work exceeding the six-second HTTP limit.
