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
bounded metadata and has_more. Playlist video rows may include
`playback_context: {"playlist": "PL...", "index": 0}`. The index is the original
zero-based occurrence position, not the row's position in a returned page.
Repeated video IDs remain separate playlist rows; unavailable entries can leave
index gaps. Missing or conflicting source positions yield no context. Ordinary
search/channel rows do not inherit a playlist context from incidental endpoints.
Library cursors retain the playlist and profile across continuation pages. Resolve selects height, codec, audio/caption
language and automatic captions; returns choices, captions and chapters.
The optional `paid_content_text` field carries YouTube's localized paid-promotion
disclosure (at most 240 UTF-8 bytes). It is read only from a matching video's
player response, independent of SponsorBlock. Missing/malformed disclosure
metadata yields an empty string; it does not change stream selection.
Each adaptive choice pairs one video/audio track; the client uses the preferred
first choice. Preferences apply to the next opened video.

`POST /v1/library` accepts `action: "playback_queue"`, `profile`, `id`,
`playlist` and a required integer `index` (0–100000). The same request can be
nested under `library` in an asynchronous `action: "library"` job. It uses the
profile's WatchNext client and returns `playlist`, `video_id`, `index`,
`window_start`, `video_ids` (at most 128), optional sparse `video_indices`, and
`radio_rebased`. Without `video_indices`, positions are consecutive from
`window_start`. Repeated video IDs represent distinct occurrences. Contradictory
selected rows or endpoint indices fail instead of changing the requested
occurrence. This is a bounded window, not proof of the complete playlist or its
end; consumers must retain occurrence identity when requesting further windows.

`playback_at` uses the same direct/job transport and window response, with
`profile`, `playlist` and `index`, but no video ID. It looks up the selected
occurrence by index. Exactly one playable selected row and an explicit matching
endpoint or panel index must prove the result; ambiguous, unpositioned and
infinite/radio results fail. This permits navigation outside the currently
loaded window without guessing which video occupies a playlist position.

Windows may include `playlist_size` (a finite count, 1–100001). It is accepted
only from explicit numeric `totalVideos` metadata consistent with every returned
source position; it is never inferred from window length or localized text.
Invalid/missing counts stay unknown. Infinite/radio windows do not advertise a
finite size; `is_infinite: true` is included when supplied by the source panel.
The native consumer revalidates this metadata and represents unknown size as 0.

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
deadline, 16 in-flight jobs, 80 retained job records, 32 sessions, a 30-minute idle
session lease and 4,096 HLS resources. Completed jobs permit repeat polling but
do not consume the in-flight limit. At capacity, the oldest delivered completed
result is evicted first; abandoned completed sign-in/chat results use the usual
cancellation cleanup. An evicted or expired job returns 410.
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
Removal/idle expiry cannot resurrect a session. Live HLS playlist roots also
renew on 401/403/410 under the same profile, with exact unique format identities
and verified audio-group/master pairing. All registered roots update together;
changed formats or pairing fail without a partial replacement. Root requests
fetch complete dynamic playlists without Range/If-Range or VOD byte validators.
An older in-flight playlist response cannot restore an expired root URL.
Segments, keys, initialization objects and captions are not aliased to new
resources. Their expiry, unverifiable VOD identity and removed capabilities end
the relay session; the native controlled recovery path can reopen the selected
format at the VOD resume position or live edge, within its retry budget.
PS5 recovery acceptance and full DVR renewal remain pending. Network failures
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

## Controlled native playback restart

`/v1/resolve` and a resolve job's `playback` record accept an optional strict
boolean `fresh` (default false). True bypasses the selected profile's extraction
cache through the same bounded worker and OAuth preflight. It creates a new
playback capability; it does not splice changed bytes into an old representation.
The profile is checked again after extraction before capability creation.

After terminal input failure, the PS5 player may stop and reopen through the
existing asynchronous launch worker. The native provider retains a RAM-only
recovery snapshot bound to the video, profile, companion address and pairing
token. It allows three restarts per explicit selection. Recovery does not consume
or complete a queue entry. It preserves fractional VOD position and caption
visibility; live sources reopen at position zero. Paused/scrubbing/seeking
playback waits. Decoder fatal errors retain the existing stop path.

Fresh choices must have a unique match for the prior container, codecs,
dimensions, live flag, range and audio channel count. Missing/ambiguous matches
fail without choosing a different format. Cancel, profile/source changes and
leaving the browser invalidate pending recovery. A changed context after open
stops the newly opened player before navigation. Open failure returns control
to the browser; selecting the video again starts a new retry budget. Console
restart, audio/position/format continuity and controller acceptance remain pending.

Optional catalog branding: search/catalog GETs and jobs accept independent
`dearrow_titles` / `dearrow_thumbnails` booleans (default false). Canonical item
`title` / `art_url` remain unchanged; optional `dearrow_title` / `dearrow_art_url`
carry replacements. Library video rows use the same job path. Metadata queries
use four-character SHA-256 prefixes; a matching video ID and trusted first non-original submission
are required. Service timeouts/invalid data retain originals. Generated image
capabilities use `/art/dearrow/{token}`, with bounded HTTPS image reads and original
YouTube-image fallback. Credentials never accompany these third-party requests.
Native preferences and original/title metadata persist; expiring image capabilities
do not survive a restart. DeArrow cache keys contain public video hashes only.

Library requests also accept `search` (validated upload-date, duration, type,
feature and order options; channel and playlist cards with a `kind` field),
`search_suggestions`, `description`, `channels`, `notifications`, `feedback`,
and the `trending`, `shorts` and `my_videos` sections of `topics`. Their request
and response fields, bounds and errors are specified in
[LIBRARY_SERVICE.md](LIBRARY_SERVICE.md#search-sections-description-and-feedback-contracts).
The top-level `search` job and `GET /v1/search` are unchanged.

## Second-batch request and response fields

All fields below are optional; without them a request behaves as before, and a
client may ignore the added response fields. `GET /v1/health` lists `locale`,
`thumbnail_source` and `storyboards` in `features` (plus `content_filters` and
`channel_pages` when the library worker runs) and adds a boolean
`sponsorblock_alt`. Field values, defaults and errors for `hl`, `gl`, `hide`,
`thumbnails` and the new library actions (`channel`, `channel_search`,
`playlist_save`, `playlist_unsave`, `history_pause`, `history_resume`,
`history_clear`, `feedback` with `hide`) are specified in
[LIBRARY_SERVICE.md](LIBRARY_SERVICE.md#locale-content-filters-channel-pages-and-account-changes).

Top-level jobs (`POST /v1/jobs`) accept `hl`, `gl`, `hide` and `thumbnails`
beside `action: "search" | "catalog" | "details"`; `GET /v1/search` and
`GET /v1/catalog` accept `hl`, `gl` and `thumbnails` as query parameters and
`GET /v1/video/{id}` accepts `hl`. A resolve request accepts `hl` (translated
title and chapter titles where YouTube has them). Rows from these routes carry
a boolean `is_short`. A library job with `action: "description"` is answered
without the library worker.

Resolve results (`/v1/resolve` and resolve jobs) add:

- `loudness_db`: number or `null`. YouTube's loudness offset in decibels for
  the video (`playerConfig.audioConfig.loudnessDb` of the matching player
  response), finite and within ±60, otherwise `null`. A positive value means
  the video is louder than YouTube's reference level. The usual rule is a
  linear gain of `min(1, 10^(-loudness_db / 20))`; SmartTube's own conversion
  lives in its service module, which is absent from the reference tree, so the
  rule is not taken from it.
- `is_short`: boolean, true when the extractor marks the video as a Short.
- `chapters`: unchanged shape, restated because the native side kept start
  times only: at most 128 entries `{"title": "<at most 96 characters>",
  "start": seconds, "end": seconds}`; `title` may be empty.
- `storyboards`: at most four seek-preview levels, smallest first, empty for
  live streams or when the extractor returns none:

  ```json
  {"width":160,"height":90,"columns":5,"rows":5,"count":120,"sheets":5,
   "interval_ms":1000,"url":"http://HOST/media/<session>/sb1-$M.jpg"}
  ```

  `width`/`height` are one thumbnail's pixels (1–1024), a sheet is a JPEG grid
  of `columns` × `rows` thumbnails (1–32 each) filled row by row, `count` is the
  number of thumbnails, `sheets` the number of sheet images (1–512; the last
  one may be partly filled) and `interval_ms` the time between thumbnails
  (video duration divided by `count`). Replace the literal `$M` in `url` with
  a sheet number from 0 to `sheets - 1`. Thumbnail *n* (0-based, at
  `n * interval_ms`) is in sheet `n / (columns*rows)`, cell
  `n % (columns*rows)`, row `cell / columns`, column `cell % columns`. Each
  sheet is an expiring relay capability of the playback session like captions
  and media: no bearer header, the signed upstream address stays on the
  companion, no account cookie is sent upstream, and fetching a sheet renews
  the session lease. Levels with more than 512 sheets, inconsistent counts or a
  non-YouTube image origin are omitted.

`GET /v1/sponsor/{id}` accepts `alt=true`. When the host started the companion
with `--sponsorblock-server https://HOST` (a bare HTTPS origin: default port, no
path, query or credentials; anything else is refused at start-up), such a
request queries that server instead of `sponsor.ajay.app`, with the same
hashed-prefix request, validation and one-hour cache (cached per server).
Without the option `alt` is ignored and the primary server is used. The
alternative host SmartTube's own service uses could not be established from the
reference tree, so it is host configuration rather than a built-in address.
