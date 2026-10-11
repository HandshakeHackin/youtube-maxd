# Library service — integration in progress

The optional companion library worker uses MIT-licensed YouTube.js 18.1.0.
It adds home recommendations, account subscriptions/history/watch-later,
playlist browsing/edit operations, video ratings and comments to the service.
The native Discover screen opens home and account/library screens through the
existing asynchronous job API. Profiles and remote playlist folders preserve
their account context. Local favorites, queue, follows and history remain console
data, stored separately for each selected profile.

This is not full SmartTube parity. Live chat has a request-driven service and
native retained-message reader and playback-clock replay sidebar; PS5 replay acceptance remains pending. Chat posting has
composition/review and confirmation guards, with real-account/PS5 acceptance pending.
Comment posting is also unfinished. Device-code login is implemented with real-account/PS5 acceptance pending. Account
cookies are configured explicitly on the companion; no browser cookies are
read automatically. A configured profile is not proof that its session is valid.
Real-account acceptance remains unverified.

### Playlist playback context

Playlist and Watch Later rows now retain the source playlist and occurrence
index through worker pagination, OAuth catalog conversion and HTTP/job output.
Both classic playlist nodes and modern video cards are supported. Repeated IDs
and sparse positions survive; search deduplication remains unchanged. This is
the service foundation for playback modes. Native rows, focus/click identity,
launch staging and asynchronous resolution retain the selected occurrence.
Recovery preserves it; profile/source changes invalidate it. Canonical local
history and queue entries do not inherit a browsing occurrence. Seven-mode EOF
policy, channel context and console acceptance remain pending.

The authenticated `playback_queue` operation exposes the existing WatchNext
window adapter through both direct and asynchronous library requests. It
retains profile ownership, sparse indices and repeats, and rejects contradictory
endpoint/selected-row positions. The native provider now exposes scoped
begin/status/cancel operations for these windows, validates their contents and
discards retired responses. With queue/playlist autoplay enabled, natural EOF
consumes the next returned occurrence after giving explicit queues priority.
Any controller press cancels the window wait. The full seven-mode policy
remains incomplete. At an exhausted window edge, autoplay makes
one indexed lookup for the immediately following occurrence and stages it only
if confirmed. Forward/reverse edge lookups now cross consecutive explicitly
unavailable target slots within the returned panel, retaining occurrence indices.
When a finite bound is known, an unavailable-only window returns a validated
next scan index or explicit exhaustion. The native client follows those steps
as cancellable asynchronous jobs without changing playback ownership. Each step
must make progress in the requested direction. Unobserved gaps and unknown-size
unavailable windows stop indexed traversal. Mix window edges use scoped next-video
metadata instead of attempting index-only radio navigation.
Index-only `playback_at` requests can retrieve a different occurrence and its
window while keeping native playback ownership unchanged. This requires an
explicit selected position from upstream; radio rebasing is not accepted for
index-only navigation. Window-edge autoplay uses this path; the remaining mode
policy and controls are still incomplete.
For edge traversal, `playback_at` accepts strict integer `direction` -1 or 1.
The worker proves the selected requested position before following consecutive
unavailable rows; it returns `requested_index` and `skipped_unavailable` with the
confirmed playable target. The native client checks direction, distance and
request ownership before staging it. Direction zero retains exact-index lookup
for shuffle; unavailable shuffle targets continue to use its normal fallback.
Validated finite `playlist_size` metadata is retained separately from the loaded
window size. Missing or inconsistent counts remain unknown. A confirmed final
occurrence stops autoplay without probing past the playlist; other window edges
continue through indexed lookup. Infinite/radio queues do not supply finite sizes.

The profile-owned playback-mode chooser offers continuous, repeat-one, shuffle, playlist-only,
reverse, stop-at-end and close-at-end. Reverse prioritizes the preceding playlist/channel occurrence
over the explicit queue and stops at index zero. Shuffle requires a validated
playlist size greater than 30 and chooses another index; failed lookups fall back to normal
continuation. Small/unknown sizes and channel groups continue normally, matching
the active SmartTube controller path. Playlist-only stops after a channel video
unless an explicit queue entry is available. The profile-owned "Queue
respects playback mode" preference defaults off, matching the reference: explicit
queues continue in stop-at-end and close-at-end modes unless enabled. Otherwise
close-at-end returns to the browser after natural drain. The preference also
controls automatic unavailable-video skips, and changing it cancels a staged
queue retry. All seven choices are also available in the in-player settings picker.
Natural drain in repeat-one seeks to zero once, retains the current queued entry,
and rearms completion only after playback leaves EOF and the seek is idle. Failed
seeks pause with a message instead of retrying each frame. Exhausted continuation
pauses explicitly, while phone-owned completion remains under phone control.
Acknowledged SponsorBlock end skips pause the current frame and enter the same
completion path, preserving phone ownership and asynchronous playlist waits.
A successful repeat/manual seek retires that explicit completion only after the
idle playback position reaches the target; a failed seek keeps it paused.
Outside playlist context, continuous playback uses the authenticated
`playback_next` operation after explicit queues are exhausted. Shuffle and reverse
also use this fallback when no playlist is active; playlist-only does not.
The adapter reads the first WEB autoplay endpoint, following the pinned
YouTube.js `VideoInfo.autoplay_video_endpoint` policy, and never substitutes an
arbitrary suggestion card. Missing/self-referencing endpoints stop playback;
the same ID at a different validated playlist occurrence remains playable.
Native jobs retain source/profile/selection ownership, support controller
cancellation and reject retired errors before credential handling. Channel
groups use the dedicated anchored Videos windows documented below. Continuous/shuffle modes consult next-video metadata
after confirmed finite playlist exhaustion. At finite ends and Mix edges,
playlist-only also consults that metadata but accepts only a target in the same
playlist, including an explicit wrap to its first occurrence. Requests
retain the active playlist/index, and validated target context survives native
handoff. Real YouTube/PS5 acceptance of these transitions remains pending.


### Live-chat service reader

The native player can show a live top-chat sidebar beside video. In YouTube
preferences, select **Live chat sidebar** to cycle Off / Left / Right. During
YouTube video playback, press **R2** and select **Hide live chat**, **Live chat:
left** or **Live chat: right** in Playback Options. The selected preference is
marked; these actions preserve playback position and audio without reopening the
video. Selecting a side again retries a failed chat. The sidebar shows
six recent previews; the browser reader retains full message text and history.
Updates are request-driven, scoped to the playing video/profile, and stop when
the player leaves or the chat ends/fails. The sidebar hides during stats,
scrubbing and Music mode. Completed broadcasts with replay use the video clock
and show CHAT REPLAY. Pause holds messages; seeks clear the old view and synchronize
the new one. PS5 replay acceptance remains pending.

`live_chat` opens a public or configured-profile chat for an eleven-character
video `id`, with `chat_mode` set to `top` or `live`. `chat_poll` and `chat_close`
use the returned opaque `cursor` and the same profile. These operations use the
authenticated companion library/job API. They never post messages, execute
message menus, or run background pollers. A live poll performs at most one upstream
request. A replay update performs at most four bounded catch-up requests. Opening a nonselected mode can require the initial header and one mode
continuation request. Honor `poll_after_ms`; early polls do not contact YouTube.

Responses have `kind: live_chat`, the most recent 24 normalized messages by default,
`cursor`, `ended`, `is_replay: false`, and `poll_after_ms`. The service retains at
most 256 messages in each of 16 chats and 256 opaque continuation references.
Contexts expire after 30 minutes without a completed update. Repeating a consumed
live cursor returns its cached update; a replay repeat is cached for the exact
position, and stale cursors cannot seek the current view. Close invalidates all references to that chat;
foreign, expired and ordinary catalog cursors cannot poll it. Worker loss or
profile removal requires reopening the chat.

Text, paid messages/stickers and membership/gift announcements are normalized as
display data. Replacement and moderator item/author deletion actions update the
retained snapshot; unsupported actions are skipped. Text remains literal, not
trusted markup. Signed server continuations, author moderation IDs, endpoints and
account credentials remain private to the worker. Opening with `chat_history: true`
returns up to 256 retained messages, bounded additionally to 768 KiB of serialized
message data to fit the worker transport. The native reader requests this mode;
L1/R1 browse the retained history, opening at the newest message. Each update is
an authoritative snapshot, so moderator removals also remove older retained text.
Selection and text scroll remain stable while the selected message is retained.
History stays in RAM and is cleared on chat/source/profile changes; messages from
before opening the chat cannot be recovered by this path. Deploy the matching
companion version with the native client. For replay, open/poll with numeric
`chat_position_sec` (0–1,000,000). Responses have `is_replay: true`, matching
`position_ms` and message `offset_ms` fields. Future actions are held until the
video reaches them; signed pre-stream offsets remain visible at frame zero. Pause avoids new upstream reads once catch-up reaches a future action or EOF;
rewind or a jump over ten seconds resets the retained view and uses a seek
continuation. EOF waits for buffered future actions. Replay is read-only; sending
returns `chat_replay_readonly`. Replay without a playback clock returns
`chat_playback_required`; the native browser reader currently directs users to
play the video and enable the sidebar. Pending actions are limited to 4,096 and
1 MiB per context; request failures preserve committed state. Full replay-history
browsing, real account contexts and PS5 acceptance remain pending.

R3 on a native live-video row opens top chat. Within chat, R3 switches top/live
mode, L1/R1 page through the retained snapshot, L2/R2 scroll the selected
message, and Touchpad opens the video's ordinary comments. Updates honor the
server interval and allow one request at a time. They preserve the currently
selected message and its scroll position when that message remains in the
snapshot. A removed message selects a nearby row with fresh text scroll.

The native client uses authenticated direct `/v1/library` requests for chat;
regular catalog jobs retain their existing behavior. Closing chat, changing
mode/source/profile, entering setup or search closes the owned context. Late
results are retired using the original companion address, bearer and profile.
Chat capabilities and full message text remain in RAM, outside `youtube.json`.
Malformed updates retain the last readable snapshot and require reopening;
the client does not retry an uncertain failed poll automatically. Fixture mode
provides rolling generated chat on the HLS fixture for offline lifecycle checks.

### Ratings for displayed comments

Touchpad on a displayed comment or reply opens available like, dislike, or
remove-rating controls. Guest has no rating controls. The full comment and
selected profile appear in a review; Cross submits once and Circle returns
without submitting it. The pinned SmartTube service provides
[comment like/dislike operations](https://github.com/yuliskov/MediaServiceCore/blob/4d9767733aa92269b9d24f24844a7fa1bb5196c1/youtubeapi/src/main/java/com/liskovsoft/youtubeapi/service/YouTubeCommentsService.kt).

The companion retains the actual pinned parser node and its server-provided
commands. Only a random, profile-owned `rating_cursor` and option booleans cross
the API; clients cannot supply an endpoint or encoded comment command.
References expire after two minutes and are capped at 256. Each permits one
rating attempt; a confirmed retry returns the prior result. A different rating
or an uncertain attempt requires reloading comments. The Python guard retains
attempted intents for 30 minutes, including across Node worker loss, so a queued
or canceled request cannot recreate an uncertain mutation.

HTTP success establishes request acceptance, not a verified readback of the
live account's vote. Unknown responses and mismatching native confirmations
fail. Offline fixtures support isolated ratings/removal in comment lists and
reply threads; restart resets them. Capabilities remain in RAM. Persisted PS5
state stays under `/data/prosperotube` through Lapy. Real-account outcomes and
PS5 controls remain unverified.

Touchpad click on a native video opens account actions for the selected profile:
like, dislike, remove rating, add/remove YouTube Watch later, and subscribe or
unsubscribe when the video's channel identity is available. Guest browsing
directs the user to select an account profile. Selecting an operation opens a
confirmation; only Cross on that confirmation submits its one-time intent.
Pending, confirmed and unconfirmed results are distinguished. Reopening the
result does not resubmit; switching profiles invalidates the confirmation.
An interrupted operation may have changed the account, so the user must check
it before deliberately repeating the action. No live account mutations were
performed to validate this implementation.

Candidate `15d0787` passed the PS5 build/package and native provider tests with
address/undefined-behavior sanitizers. Offline cases cover all seven operations,
one-time submissions, profile changes, re-pairing and unconfirmed results. The
shipping menu and confirmation RML were rendered and inspected on the host.
PS5 controller acceptance and real-account outcomes remain unverified.

Native playlist controls support create, rename, delete and adding/removing a
selected video. Accounts & library creates an empty playlist; video account
actions create one with the video or open the paginated account-playlist picker.
Touchpad click on an account playlist opens rename/delete. The keyboard accepts
names of 1–150 Unicode code points; invalid UTF-8, control characters and longer
names are rejected. The full name and selected identities appear in the scrollable
confirmation reader. New playlists explicitly request `PRIVATE` visibility.
All changes use the same one-time, profile-bound confirmation as account actions;
source/profile changes invalidate text-entry tickets. Built-in Watch later and
Liked videos cannot be renamed or deleted from these controls. Created-playlist
responses offer a link to open the new playlist. PS5/live acceptance remains pending.

Candidate `17ba784c` passed the PS5 build/package checks and was frozen with a
123-file checksum manifest (eboot SHA-256 prefix `2578b354ccfbaf2d`). Native
sanitizer cases cover picker paging, profile isolation, Unicode name limits,
safe JSON serialization, create/rename/add/remove/delete and one-time submission.
The shipping confirmation reader was rendered and inspected on the host.
The companion's 17 Node tests include the pinned create-playlist endpoint and
its explicit private-visibility payload. This establishes offline behavior;
server visibility and controller behavior still require live/PS5 acceptance.

## Installation

Use Node.js 20 or newer on the companion host. Install and build the single-file
worker from the pinned lock file:

```sh
cd companion/innertube
npm ci --ignore-scripts
npm run build
npm test
```

Bundling avoids slow imports of thousands of files when the workspace is on a
Windows filesystem. The generated bundle and dependency directory are ignored;
source and the exact dependency lock are committed. They run on the companion,
not on the console. Retain dependency licenses with any bundled distribution.

Enable anonymous recommendations/comments with:

```sh
PYTHONPATH=companion python -m prosperotube --library-node /path/to/node \
  --host 0.0.0.0 --public-base http://YOUR-LAN-IP:8765
```

Add `--library-profiles /private/path/profiles.json` for accounts. Its format is
an array of at most eight profiles:

```json
[
  {"id":"personal","name":"Personal","cookie_file":"youtube.cookies","account_index":0}
]
```

Cookie paths are resolved relative to that configuration. Netscape cookie files
or a single Cookie-header value are supported for library and primary-account extraction. Use a
private host directory. Cookies remain on that host and never appear in profile
responses, console files, process command lines, logs or error diagnostics.
The selected native profile now accompanies search, catalog and playback jobs.
For a profile with `account_index: 0` and no brand `channel`, the resolver installs
that profile's cookie snapshot in its own YouTube-scoped cookie jar. Each profile
has a separate extraction cache; all profiles share the subprocess concurrency
limit. Unknown profiles are rejected. Secondary account indexes and brand-channel
playback currently fail explicitly with `playback_account_context`, pending a
verified mapping; they do not fall back to the primary account.

Guest uses the ordinary resolver. The legacy `--cookies` option, when explicitly
supplied, still applies to that Guest resolver; omit it for anonymous Guest
browsing. Configured profile credentials do not fall back to that file. Profile
configuration and cookie snapshots are loaded at startup; restart to refresh them.
URL-scoped cookies needed by selected media or captions stay on the companion
relay and do not appear in the public playback response. Primary-account private
video access and live cookie/session validity remain unverified.

The service accepts an optional bounded `profile` on search/catalog/video queries
and general extraction jobs, and inside `ResolveRequest`/a resolve job's `playback`.
The nested playback profile controls resolution even if the outer job has a
different profile. Omitting it preserves the Guest default.

Candidate `1546374e` passed the PS5 build/package checks and was frozen with
123 files (eboot SHA-256 prefix `f1f869079b9fd8ff`). The native sanitizer suite
verifies Guest/selected-profile request payloads and cancellation. The full
Python suite passed 47 tests; added cases cover actual extractor IPC/cache
partitioning, API and asynchronous-job routing, rejected account contexts and
the pinned yt-dlp cookie jar's HTTPS/domain scope. The cookie handoff test also
checks that a webpage's cookies cannot become inherited headers on all media
formats. No live account or PS5 deployment was performed for this candidate.

## Playback history reports

**Sync YouTube history** is a saved native preference, enabled by default for
selected account profiles. Guest never submits reports. A playback context is
bound to the profile and video returned by resolution; selecting a stream alone
does not write history. After the first positive playback position, the companion
gets tracking data for that video, adds it to history and updates its watch time.
The adapter uses the pinned YouTube.js [tracking methods](https://github.com/LuanRT/YouTube.js/blob/main/src/parser/youtube/VideoInfo.ts).
Position updates are coalesced and limited to one per 30 seconds; stopping sends
the latest position, including a queued terminal report during a rapid video
change. Native browsing cancellation leaves these reports running. Profile/source
changes invalidate the active context and ignore late results; they cannot undo
a report the server has already processed. Turning sync off prevents future
reports; an already submitted request may still finish.

`watch_progress` uses the authenticated library API with `profile`, video `id`,
an opaque playback `session`, finite `position_sec` from 0 to 1,000,000 and
`state` (`playing`/`stopped`). The worker validates the signed-in session and
tracking video's identity. It retains at most 64 tracking contexts, scoped to
profile/session, with a 30-minute inactivity expiry. Duplicate reports use their
confirmed result while retained, and stopped sessions reject later changes.
Failure or uncertainty stops further attempts for that context, including when
adding history succeeded but updating position did not. The native preferences
show pending, confirmed or unconfirmed report status. A new playback selection
creates a new context. Server restart/expiry discards retained contexts; this is
not a durable exactly-once transaction protocol.

The Python companion also retains bounded uncertainty guards outside the Node
worker. Worker loss blocks all tracking contexts whose objects were lost, so a
queued final report cannot recreate an attempted history addition after a timeout
or cancellation restarts Node. These guards survive worker restarts while the
companion process remains running; a new explicit playback context can proceed.

Fixture mode records synthetic history and resume positions separately per
account through this same API. These offline cases and confirmed HTTP responses
do not prove real YouTube watch-history display, cross-device resume behavior or
PS5 playback-lifecycle timing. Those remain acceptance gates. The console's local
history remains separate; clearing it does not clear YouTube account history.

Native candidate `8a8d84ed` passed the PS5 build/package and was frozen with
123 files (eboot SHA-256 prefix `00fd2953f8cf3456`). Companion revision `385f6d9`
adds the worker-loss guard; native sources and build scripts are unchanged from
the frozen candidate. The native sanitizer suite verifies the actual 30-second
throttle, coalescing, stop reports, rapid changes, settings persistence, browsing
cancellation, profile changes, stale 401s and suppression after uncertainty.
All 19 Node tests passed, including actual pinned tracking calls, response failure,
identity checks, closed sessions and storage limits. The full Python suite passed
50 tests; its subprocess cases prove that timeout and unrelated worker loss block
queued/repeated reports while a new explicit context can recover. No PS5 or live
account history acceptance was performed for this candidate.

## Local profile state

Local favorites, queue ordering, channel follows, resume positions and watched
markers use the selected profile ID plus the stable video ID. Cached title/artwork
metadata may be reused across profiles; account/local state is never copied with
it. Clear-history and clear-queue apply only to the current profile. Playback
progress/completion stays bound to the profile that started the video, even if
the browser switches profiles. Autoplay stops across that switch, and a pending
next-queue item is invalidated. Switching back restores the saved lists.

The native `youtube.json` format is version 2, with a profile on each record.
Version-1 global records migrate to **Guest**, preserving favorites, follows,
queue order, positions and watched markers. They are not assigned to whichever
account happened to be selected. Duplicate keys retain their first record;
invalid/missing version-2 profile IDs are skipped. An unrecognized version,
malformed record-list schema or unreadable file blocks saving, preserving the
existing file rather than overwriting it on shutdown. Profile IDs must remain
stable in the companion configuration; they are user-configured identifiers.

The current native store retains at most 384 records across profiles. Favorites,
queued videos and followed-channel records are protected from ordinary cache
eviction. Player preferences remain console-wide. PS5 navigation/lifecycle and
larger-library retention remain acceptance work toward full parity.

Candidate `09c453c6` passed the PS5 build/package and was frozen with 123 files
(eboot SHA-256 prefix `f41250a4e796f575`). The native provider suite passed with
address/undefined-behavior sanitizers. Cases cover independent lists and clear
operations, persistence of the same video under two profiles, completion after
a profile switch, queue/autoplay isolation, legacy Guest migration, duplicate
and invalid records, and refusal to overwrite unsupported/malformed state files
on shutdown. The shipping layout is unchanged; PS5 behavior remains unverified.

## API and failure contract

All operations require the ordinary companion bearer token.
`POST /v1/library` accepts a bounded action/profile/item request.

Curated TV sections use `{"action":"topics","section":"music"}` (also
`gaming`, `news`, `sports`, `live` and `movies`). Shelf items are folders named `section:<section>:<id>`;
open one with `{"action":"topic_open","section":"music","id":"<id>"}`.
Continue either shelf lists or their contents with the usual `continue` action
and returned cursor. IDs/cursors are opaque, profile-scoped and expire; reload
the section when expired. Playlist/channel/storefront folders preserve their
originating section. Channel and Movies storefront pages retain their shelves.
An empty storefront's upstream explanation is returned as `catalog_unavailable`.
These requests use YouTube's TV browse feed. A configured profile with an active
signed-in session prepends a personalized Liked music folder. It uses the pinned
SmartTube TV continuation, then the WEB `VLLM` fallback. Guest or expired sessions
make no personalized request. Missing/empty personalized data leaves the public
shelves available. The retained folder and its continuations remain owned by the
selected profile; WEB continuations retain the WEB client context.
`POST /v1/jobs` with `action: "library"` and a nested `library` request is the
native route, allowing polling/cancellation without blocking input.

Continuation tokens are opaque, profile-scoped, bounded, expire after two
minutes, and permit repeat reads for retries/back navigation. They are not
YouTube credentials. Return pages contain at most 24 normalized display items.
The worker serializes operations and bounds request, response, upstream body,
network and operation sizes/times. Cancellation/timeout reaps the process;
later requests recreate it. Restarting loses temporary cursors, so reload a
screen when its continuation expires.

An uncertain timed-out change is not reported as success; inspect the account
before repeating it. The worker rejects an unconfirmed upstream mutation.
External library networking is disabled in explicit offline fixture mode.

## Offline account fixtures

Starting the companion with `--fixtures fixtures/generated` also enables an
in-memory library with Guest and two clearly labelled offline account profiles.
It uses the same authenticated library and asynchronous-job APIs as normal mode.
The accounts have 25 playlists to exercise paging, generated comments with
long text and literal markup, top/newest sorting and a three-reply thread.
Ratings, subscriptions, Watch later and playlist edits affect synthetic state
only. Profiles have separate state; continuation/reply references are scoped,
bounded and expire after two minutes. All changes reset when the server restarts.
No Node worker, credentials or external networking are used. Real library/profile
flags remain incompatible with fixture mode. These fixtures support repeatable
UI acceptance; their success cannot prove YouTube account compatibility.

The HLS fixture supports selected-account live-chat sending through the same
library and asynchronous-job endpoints. Messages appear in that chat's rolling
snapshot only after fixture confirmation. Identical confirmed intents return
the same message ID; changed or unconfirmed intents cannot send again. Guest,
foreign-profile, expired and closed chat contexts are rejected. Message limits
match the production contract, with 256 intents retained for 30 minutes in RAM.
Profile removal and shutdown clear them. This enables native compose/review/send
acceptance without a Google account or external message posting.

The full Python suite passed 44 tests after this fixture integration. The fixture
cases exercise the real authenticated API and jobs, guest rejection, all account
mutations, Unicode playlist names, profile separation, comments/replies, repeat
reads, reference expiry and bounded storage. Test transport checks reject any
attempt to construct an external HTTP client in this mode.

## Offline evidence

Node tests cover all-item pagination, retry semantics, expiry, profile isolation,
guest authorization, credential exclusion, metadata origins, comment display
records and upstream mutation failures. Python tests exercise the actual bundled
dependency import without network/account use, IPC concurrency/Unicode,
timeout/cancellation/reaping/recovery, authenticated APIs and asynchronous jobs.
The native provider sanitizer test exercises library/profile/playlist navigation,
opaque pagination and rejection of cursors belonging to another screen.

Modern `LockupView` video and collection cards retain their attributed titles,
channel links, thumbnails and timestamp badges. Offline tests instantiate the
pinned YouTube.js parser classes, including invalid duration badges and explicit
promoted-record rejection. This checks parser compatibility without claiming
live account responses or complete advertisement filtering have been validated.

Comment records with reply threads include an opaque `reply_cursor`. Use action
`replies` with that cursor and the same profile to open the thread, then use the
ordinary `continue` action for additional pages. References expire after two
minutes and are limited to 256 retained threads. Repeated reads return the same
page; expired or evicted references require reopening comments. These are browse
operations. In the native browser, R3 on a video opens top comments. R3 toggles
top/newest sorting on the comment list, Cross opens an available reply thread,
L2/R2 scroll the selected comment, and L1/R1 page through the list. Circle returns
to the previous list or video browser. Text and newlines appear in a separate
reader; comment markup is displayed literally. The selected profile accompanies
each request, and changing profiles invalidates native pagination state. Comment
posting and PS5/live-service acceptance of this view remain unfinished.

Candidate `31fb0bd` passed the native provider suite with address/undefined-behavior
sanitizers and the PS5 build/package checks. Host rendering of the shipping RML
verified comment-list focus/activation, full-text scrolling, literal markup and
the empty-discussion state; the rendered reader was inspected. These offline
checks do not establish PS5 or live YouTube discussion acceptance.

The test runner bundles the actual parser with its fixtures into a temporary
directory and removes that directory afterward. This avoids slow imports of
thousands of individual dependency files on Windows-mounted workspaces.

## Device-code account backend

Enabling `--library-node` also enables the authenticated device authorization
endpoints documented in PROTOCOL.md. The companion obtains the public TV client
identity and device code through pinned YouTube.js. Each status request performs
at most one token poll, only after the upstream interval (minimum five seconds).
There is one active session, at most 16 retained sessions, and no automatic
polling timer. Cancellation cancels a queued/in-flight worker request; credentials
returned after cancellation or expiry are discarded. Uncertain redemption is
not retried. Sessions expire and are lost when the companion restarts.

Authorized profiles and refreshed credentials persist in `oauth-profiles.json`
under the companion's configured state directory. On POSIX, directory/file modes
are 0700/0600. Atomic replacement, an exclusive owned lock and snapshot comparison
prevent concurrent writers from silently overwriting accounts. Corrupt or
unsupported files are preserved and reject startup. OAuth and configured cookie
profiles share the eight-profile limit. New profiles become available without
restarting the companion; their next library request recreates the Node worker.
Private IPC carries tokens and refresh updates; HTTP responses expose only
profile ID/name and the user-facing device code. Credentials are never sent to
the PS5. PS5 state/cache/diagnostics continue to use Lapy and `/data/prosperotube`.

OAuth sessions use the TVHTML5 client, consistent with the upstream
[YouTube.js authentication documentation](https://ytjs.dev/guide/authentication).
Cookie and OAuth contexts cannot be mixed. Offline tests exercise actual pinned
TV request headers/context and token refresh. A live public device-code request
and local cancellation passed without token polling or account authorization.
Real authorization and private-account feeds remain unverified. Asynchronous
sign-in jobs use the existing job transport; abandoned starts and canceled polls
discard late responses before persistence. Sign-out remains unfinished.

The native Accounts & library screen now includes **Sign in to YouTube**. It
obtains a code through the job API, shows the verification URL and code, and polls
automatically at the supplied interval. Pending polls preserve controller focus.
Expiry, denial, cancellation and failure stop polling; restarting requires an
explicit new sign-in. An authorized result offers a link to select its profile.
Circle leaving sign-in, closing the browser, searching or changing the companion
source cancels pending authorization. A late response is cleaned up against its
captured original companion address/token. Changing the companion source also
resets the selected account to Guest. Codes/session references remain in memory
and never enter `youtube.json`. This implementation has native sanitizer and
host layout coverage; actual PS5 sign-in and live account acceptance are pending.

## OAuth TV resolution

Selected OAuth profiles now use a separate resolver/cache context. Before video
extraction, a private Node operation prepares the account's TV session and refreshes
tokens with less than 60 seconds remaining. Refresh updates are persisted through
the existing private IPC path. Only the access token/expiry cross into the Python
resolver; the refresh token/client secret remain in the account worker/store.
No HTTP endpoint exposes this operation. The token reaches the extraction child
through stdin, never command arguments or PS5 state.

The original local yt-dlp adapter replaces only that child's YouTube video
extractor. It requests `tv` and `tv_downgraded` and rejects non-TV player fallbacks.
Bearer headers are added only to client-7 TVHTML5 requests. A separate bounded
transport validates the actual JSON context and precise HTTPS YouTube API endpoint,
rejects redirects, limits responses to 8 MiB and uses a 12-second network timeout.
Video extraction and preflight share the resolver's overall deadline. Cookies and
OAuth cannot be mixed. Bearer headers are stripped from normalized formats and
captions; the relay never uses an OAuth header. TV script URLs supplied by TV
configuration are retained; a missing script reference uses the pinned TV variant.
The signature timestamp is extracted from that exact TV script, with the pinned
variant-scoped cache. WEB/config timestamps and caller-supplied `STS@HASH`
overrides cannot replace the TV identity. This prevents a TV request from using
a timestamp belonging to another script; it does not prove current cipher support.

OAuth search/channel/playlist pages use the TV library session and its opaque,
profile-scoped continuations. They retain explicit `has_more` even when a page
contains fewer than 24 videos. Each profile retains at most 32 catalog contexts,
16 pages per context, for 110 seconds. Errors discard the affected context so a
reload can recreate it after worker loss. Back navigation beyond retained pages
refreshes the query. Guest and cookie-profile extraction remain separate.

Offline tests exercise actual pinned TV requests, response parsing, normalized
AVC/AAC adaptive formats and DASH capability creation, refresh/preflight privacy,
redirect/origin/context rejection and profile pagination. The public TV probe on
`jNQXAC9IVRw` returned `LOGIN_REQUIRED` and no formats; no account was authorized.
This is a connected adapter with offline evidence, not proof of private account
playback. Real authorization, TV catalog compatibility, account-only streams,
caption access and signature solving still need live acceptance. The upstream
[TV player issue](https://github.com/yt-dlp/yt-dlp/issues/17389) documents TV script
challenge compatibility problems; the current adapter does not establish that
the pinned evaluator handles every current TV script.

The explicit public-script probe subsequently passed for the fetched
`tv-player-ias.js`: the actual pinned timestamp parser and Node/EJS evaluator
handled one synthetic signature and one synthetic `n` input. See
[the probe command](BUILD.md) and [validation evidence](VALIDATION.md).
This establishes evaluator compatibility with that public script, not account
authorization, real signed media URLs, all TV variants or PS5 playback.

## Remove an account from ProsperoTube

Accounts & library > Remove selected account opens a profile-bound review.
Cross submits one `profile_remove` library job; Circle returns without submitting.
Confirmation must echo the removed profile. Success resets the selected profile,
history/queue handoff and browser stack to Guest/the refreshed account list.
Invalid confirmations preserve selection; replay and profile/source changes do
not resubmit. An authoritative profile list also reconciles an account removed
elsewhere, while malformed lists cannot erase selection.

The companion serializes removal with library operations, commits the private
OAuth credential file, then removes the live profile and cached Node worker.
Profile resolver caches and running evaluator processes are invalidated, queued
or completed jobs for that profile are discarded, retained authorization results
forget it, and its relay/caption/HLS capabilities expire. Active relay streams
stop delivering subsequent chunks and close upstream responses. Guest and other
profiles' capabilities remain available. Node's temporary continuation/tracking
contexts are reset; existing uncertainty guards remain in effect.

An owned-lock, snapshot or pre-commit write failure keeps the live account.
If the file replacement committed but directory sync/cleanup failed, runtime
credentials are still invalidated and `auth_storage_unconfirmed` reports the
durability uncertainty. No success is claimed for that result. Restart the
companion before adding accounts after this error.

This matches the pinned SmartTube service's local
[account removal](https://github.com/yuliskov/MediaServiceCore/blob/4d9767733aa92269b9d24f24844a7fa1bb5196c1/youtubeapi/src/main/java/com/liskovsoft/youtubeapi/service/internal/YouTubeAccountManager.java#L168).
It does not mutate YouTube playlists or revoke the Google account globally.
Accounts added through the app are removable; externally configured cookie
profiles report `profile_managed` and are removed from companion configuration.
Console-local history/favorites/queue records remain scoped to their prior
profile ID. Fixture accounts support the same removal API in memory.

Offline tests include a real bundled Node worker with fetch disabled, private
store reload, failure/foreign-write preservation, removal jobs keeping their
own confirmation, peer isolation, stale authorization/jobs, real resolver-child
retirement, active streaming shutdown and native one-time confirmation. PS5 and
real-account acceptance remain pending; see [validation](VALIDATION.md).

## Live-chat message service

`chat_send` accepts the selected `profile`, live video `id`, owned chat `cursor`,
unique `session` intent and `text` (1 to 200 Unicode characters, no control
characters or blank message). It requires an authenticated library session and
an active, non-replay chat. The pinned YouTube.js sender is used without starting
its background reader; existing request-driven chat polling remains in control.
Only a single returned text-message action echoing the submitted text confirms
success. Attestation-only, missing, different or ambiguous echoes do not confirm
the message. No optimistic local insertion precedes that confirmation.

Successful results contain only API version, success, video ID, intent and
message ID. Repeated identical intents return the retained confirmation;
changed text/context and unconfirmed attempts cannot resubmit. Companion RAM
guards retain up to 256 intents for 30 minutes and survive Node worker loss,
timeout and cancellation. They store a text hash, not message content, and are
removed when the profile is removed. They do not survive a companion restart.
The active Node chat context also bounds attempted intents and confirms the
returned message before adding it to its rolling reader snapshot.

On PS5, L3 in an active chat opens the virtual keyboard. The message is held in
RAM and reviewed with its profile and video before Cross submits one library
job. Circle returns without sending; returning to chat invalidates the draft.
R3 still changes chat mode and Touchpad still opens comments. Composition waits
for an in-flight reader update instead of canceling that context. Guest and
ended chats cannot compose. Profile/source changes, browser closure and stale
keyboard callbacks invalidate the draft. A confirmed result clears its text;
an uncertain result cannot reuse the send action. Native compose/review controls
are implemented with offline evidence; physical keyboard/controller behavior
and real-account posting acceptance remain pending. No real chat messages were
sent during development tests.

The PS5 browser can open a replay from top/newest comments with Triangle.
Replay snapshots use an explicit selected time, support L1/R1 minute navigation,
remain stationary between seeks and reject composition. Native/provider and
actual generated reader tests pass. This is retained-window timeline browsing;
unlimited historical-message pagination and hardware acceptance remain open.

## Local channel blocks

Videos with a stable YouTube channel ID expose Block channel/Unblock channel in
their local video menu. Blocks are owned by the selected profile and persisted
under `/data/prosperotube` with the other local preferences. The Blocked channels
screen removes them even when their videos are hidden. Search/library catalogs
and local history/favorites/queue lists omit matching channel videos without
deleting favorites, queued membership or history. Source continuation flags and
playlist occurrence indices are preserved. The bounded store holds 128 blocks
across profiles. Repeated Block actions are idempotent and stale profile/source
menu IDs fail.

Queue completion, unavailable-item retries, and manual queue Previous/Next skip
blocked channels without consuming their queued entries. A final handoff check
also rejects a queue item blocked after selection.

Playlist windows now carry optional position-aligned `video_channels`; channel
windows carry their verified source channel for every entry. Native navigation
skips known blocked channels in either direction, preserves occurrence indices,
and requests the next window beyond a blocked boundary. A prepared playlist
handoff is checked again before launch. Malformed or mismatched channel arrays
are rejected rather than assigned to another occurrence.

Catalog and retained queue records now also match names using Unicode simple
uppercase-then-lowercase comparison, following the
[Java case-insensitive comparison rule](https://docs.oracle.com/en/java/javase/24/docs/api/java.base/java/lang/String.html#compareToIgnoreCase(java.lang.String)).
The pinned Unicode 15.0.0 table handles non-ASCII and supplementary characters;
there is no locale-specific folding, multi-character expansion or normalization.
Videos with only a channel name can be blocked and persisted. Management actions
carry a list revision as well as the profile/source epoch so removing one row
cannot retarget an older action to another row. Stable-ID matching remains.
Playback navigation can request `require_name: true` together with
`require_channel: true`. Playlist and channel windows return aligned
`video_channel_names`; recommendations return `next_channel_name`. The worker
verifies video/channel identity and author using the same profile client, with
four concurrent lookups and duplicate reuse. Conflicting channel identities or
missing/invalid names fail the request. Native navigation requests names when
that profile has name blocks and checks them again before handoff, including
uncached occurrences. Alternative recommendation selection remains unfinished.
No YouTube account feedback or subscription is modified.

Successful channel block/unblock actions close the local action screens and
reload their originating catalog or search. Search text and the parent folder
are retained; the blocked-channel management screen also refreshes after removal.
Failed/stale actions do not navigate away from their error screen.

With active profile blocks, `playback_next` requests `require_channel: true`.
The worker verifies the selected autoplay video's identity and channel using
`getBasicInfo` on the same profile client. The native provider rejects blocked,
missing or malformed channel metadata and rechecks the channel before handoff.
A blocked/unverifiable recommendation currently stops playback; finding an
alternative unblocked suggestion is still pending. Profiles without blocks avoid
the extra metadata request. The requirement flag is a strict boolean permitted
only for `playback_next`, `playback_queue` and `playback_at`.

Playlist navigation with active blocks also requests `require_channel: true`.
Missing channel IDs in the bounded window are verified through `getBasicInfo` on
the same profile client, once per unique missing video ID with at most four
lookups in flight. Existing occurrence order and repeated IDs are preserved.
On failure, no new lookups start and in-flight calls settle before the request
fails. Native acceptance requires complete channel metadata when blocks are
active, including if blocks changed after the request began. A late block also
prevents an unverified prepared window entry from launching. The normal worker
timeout still applies; a large metadata-poor window may time out rather than
bypass filtering. Without blocks, no extra metadata lookups are requested.

## Account history removal

The account video menu also offers `mark_watched`, separate from the local-only
action. It requires a confirmation session and signed-in profile, obtains the
selected video's current duration, and calls the pinned playback/history tracking
methods at that duration. Live/upcoming videos and missing, mismatched or invalid
durations fail before any history submission. Both tracking responses must
confirm success; only then does the PS5 mark its local record watched. The worker
retains up to 256 confirmation sessions for 30 minutes, returns cached confirmed
results, and rejects uncertain or concurrent retries for the same intent.
The companion supervisor retains the same ownership/result gate across worker
restarts, so a worker timeout cannot replay an uncertain confirmation.
Real-account acceptance remains pending.

Account-history pages now include `history_remove_cursor` where the retained
YouTube.js history page supports removal. `history_remove` accepts that `cursor`,
the same video `id`, and owning `profile`. References expire after two minutes,
remain private to their account/page/video, and never expose feedback tokens.
The worker retains at most 256 references. Local pagination retains the original
feed; upstream continuation pages receive their own feed-owned references.

Removal calls the pinned history page's `removeVideo(id, 1)` and requires its
processed-feedback confirmation. Confirmed retries return the cached result;
failed or in-flight attempts cannot be repeated through the same reference.
Expired, foreign and mismatched references fail before submission. The PS5 video
menu offers removal for a freshly loaded account-history item and confirms the
selected profile before submission. References remain in memory, expire, and are
retired by profile/source changes or submission. A matching confirmed video ID
is required before clearing local history; favorites and queue membership remain.
The separate local-history action does not submit an account change. Real-account
and localized history-layout acceptance remain pending.

## Channel playback windows

`channel_window` accepts `profile`, source `channel` (`UC` ID), current video
`id` and source `index`, plus optional destination `target` and `direction`
(-1, 0, 1). It follows the channel Videos feed in order and verifies the original
video at its source position before returning a target. Changed anchors fail
with `channel_changed`; they are never silently moved to another occurrence.
Forward/backward requests can skip unavailable or upcoming slots within 4095
positions; exact requests require the requested position to be playable.
When a directed scan proves that all remaining slots are unavailable, it returns
an anchored `unavailable_only` exhaustion result instead of a missing-video
error. Native continuous playback can then use next-video metadata at the end.

The response uses the existing bounded window format (at most 128 video IDs
and explicit source indices), an internal `channel:UC…` group key, and mandatory
`anchor_video_id`/`anchor_index`. A finite size is reported only after reaching
the feed end. Raw positions and repeated IDs survive pagination. The operation
uses the selected profile and the existing worker timeout; deep channel scans
may time out and report failure. It never synthesizes an uploads playlist ID.

The native provider uses these windows for forward/reverse completion and
indexed group navigation, retaining selection/profile cancellation ownership.
Continuous mode can request next-video metadata after a confirmed channel end.
Real channel navigation and timing acceptance on PS5 remain pending.
