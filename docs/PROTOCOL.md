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
DASH. Split live HLS is paired using explicit CODECS and AUDIO-group metadata
from the extractor's original master response. Exact rendition URLs establish
the relationship; unknown audio codecs and unrelated manifests are not paired.
A local master references separate audio/video relay playlists, without exposing
signed upstream URLs. HLS links/keys become bounded relay capabilities. WebVTT becomes SRT
without changing timestamps. HLS keys retain file suffixes for FFmpeg extension
validation. The PS5 checks resolved/caption URLs against its configured origin.

Limits: 8 KiB request bodies, 8 MiB extractor JSON, two workers, 45-second worker
deadline, 16 jobs, 32 sessions, a 30-minute idle session lease and 4,096 HLS resources.
Successful manifest/caption delivery and streamed media bytes renew that lease.
Missing resources, failed upstream requests and expired/invalidated tokens do
not renew it; activity never restores a removed capability. Active HLS playlists
retain their current variant/segment/key/init graph. At capacity, the least
recently listed resources outside that graph are retired. Recent older windows
remain available while capacity permits. An oversized current graph fails
atomically and preserves the previous usable registry. This does not guarantee
unbounded DVR history; long-window playback/seek requires separate acceptance.
Expired VOD HTTP/DASH access (401/403/410) triggers one fresh resolution under the
session's profile. It bypasses the extractor cache, keeps the relay capability
and manifests, and atomically renews matched format roots. Identity includes
format ID, container, codecs, dimensions, frame rate, language and audio layout;
changed or ambiguous matches never select another quality automatically.

After an upstream representation has been accepted, resumed ranges require its
unchanged strong ETag, or Last-Modified when no ETag exists and the response Date
is at least 60 seconds newer. The latter is our clock margin for the
[HTTP validator rules](https://www.rfc-editor.org/rfc/rfc9110.html#section-8.8.2.2).
Renewed range requests use If-Range and verify response bounds and total size;
weak tags, changed/missing validators, ignored ranges and changed content coding
end the capability with a sanitized `expired_media` error. First-open failures
can renew before any representation is accepted. Observation state is shared
with earlier in-flight responses, preventing stale streams from losing its
identity checks.

Renewal is single-flight per session, with a 30-second minimum between attempts
and a 60-second resolution bound in addition to the resolver's own deadline.
Removal/idle expiry cannot resurrect a session. Live HLS roots/segments, captions,
unverifiable byte identity and native re-open recovery remain separate work;
their expired access still requires reselecting the video ID. Network failures
and other upstream statuses do not enter this expired-link retry path.

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
The same operations are available through POST `/v1/jobs`, using
`{"action":"auth","auth":{"action":"start","name":"YouTube account"}}` or
`{"action":"auth","auth":{"action":"status","session":"32 lowercase hex characters"}}`.
Use action `cancel` with the session to cancel explicitly. Poll jobs through the
ordinary GET endpoint; completed results contain the same public response.
Deleting an in-flight sign-in job invalidates its operation before canceling the
worker. Deleting or expiring an undelivered completed start also cancels its
orphaned session. Once GET delivers a start result, the caller owns the session:
deleting that completed job leaves authorization pending until explicit session
cancellation or expiry. The native account browser now uses this job flow;
PS5 and real-account acceptance remain pending.

Selected OAuth profiles now route search/catalog through TV library continuations
and video resolution through a TV-only extraction adapter. Token preparation is
an internal worker operation and cannot be requested through HTTP. All public
resolve/job responses retain the existing capability-only media protocol. Real
account and TV signature compatibility acceptance remain pending.

Account removal uses the existing authenticated library endpoint or library job:
`{"action":"profile_remove","profile":"oauth_0123456789abcdef"}`.
Confirmed results contain `api_version: 1`, `success: true` and the same `profile`.
The companion removes that app-signed-in profile from private storage and runtime,
invalidates its resolver/capabilities/jobs and retained authorization results,
then retires the cached Node worker. The removal job retains its own result.
Guest is rejected with `account_required`, missing profiles with `unknown_profile`,
and externally configured cookie accounts with `profile_managed`.
`auth_storage_unconfirmed` means replacement occurred but durable synchronization
was not confirmed; runtime credentials still retire. No upstream account mutation
or token-revocation request is sent by this operation.

Comment ratings use the authenticated library endpoint/job with
`{"action":"comment_rate","profile":"PROFILE","cursor":"RATING_REFERENCE","rating":"like"}`.
`rating` is `like`, `dislike`, or `none`. Comment/reply rows expose
`rating_cursor`, `rating_like`, `rating_dislike` and `rating_remove`; Guest or
unsupported parser controls expose an empty cursor and false options.
References are opaque, profile-bound and valid for two minutes. They are not
reply or paging cursors. Accepted results contain `api_version:1`, `success:true`
and the requested `rating`, with no raw upstream commands/results.
One reference permits one attempt: confirmed same-rating retries reuse its
result, while changed/uncertain attempts return `comment_unconfirmed` (409).
Missing/expired/wrong-owner references return `cursor_expired` (409), Guest
returns `account_required`, unavailable choices return `invalid_rating`, and
exhausted companion intent retention returns `comment_busy` (429).
Transport acceptance still requires live account readback during validation.
