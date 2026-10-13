# Library service — integration in progress

The optional companion library worker uses MIT-licensed YouTube.js 18.1.0.
It adds home recommendations, account subscriptions/history/watch-later,
playlist browsing/edit operations, video ratings and comments to the service.
The native Discover screen opens home and account/library screens through the
existing asynchronous job API. Profiles and remote playlist folders preserve
their account context. Local favorites, queue, follows and history remain console
data, stored separately for each selected profile.

The native video actions menu includes **Related videos**, using the selected
profile's WatchNext feed through the `suggestions` library operation. Its video
identity is required at the API boundary and checked against the upstream info.
The implementation includes direct Video, CompactVideo, GridVideo and Lockup video
cards, SmartTube watched-suggestion filtering, profile-owned continuations and
retained browsing-group positions. Duplicate occurrences remain distinct; browser
pagination and playback navigation share each continuation. Shelves, ads and
playlist cards are not flattened into this video group. Other suggestion layouts,
remote-only fallback selection and live-service/PS5 acceptance remain
unfinished. This path requires the real library worker, not the fixture library.

Unselected WatchNext chip filters appear as folders above the direct suggestions.
Each folder is an opaque, profile-owned reference, limited to 256 retained entries
and a two-minute lifetime. Opening it uses that chip's parsed endpoint and creates
an independent feed/group. Concurrent opens share the result and cursor. Parsed
reload targets and filtered continuation nodes are validated; expired in-flight
requests cannot publish a group. The original feed and its continuation remain
independent. HorizontalList, ExpandedShelfContents and VerticalList shelves now open as separate section folders, including shelves on root and filtered continuations. Their own continuation requests share browser/playback paging and preserve repeated video occurrences. Nested shelves are bounded to four levels, with at most 64 section folders and 4,096 entries per page. Unsupported layouts remain unfinished.

Home also exposes supported RichShelf and Shelf rows plus unselected primary and secondary
filters through the same expiring, profile-owned folder references. Main-grid
watched filtering does not hide completed videos in secondary Home rows, matching
the pinned SmartTube group-position rule. Recognized shelf children are removed
from the flattened main list and retain separate occurrence groups. The main grid
uses RichItem source order rather than parser memo type order. Section and filter
continuations stay independent; other layouts remain
unfinished. These additions require the actual library worker, not the fixture.
Secondary controls require one explicitly selected primary filter, skip selected or disabled controls, and use the pinned two-filter API without fetching the selected primary again. Tabless reload responses are read from parsed response lists because the pinned generic page accessor dereferences a missing Tab.

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

## Accounts in the built-in service

Since 2026-10-13 the service inside the app answers everything on this page
itself, with the same requests, answers, codes and bounds: device
authorization, profiles, account lists, account changes with their
one-attempt guards, history reports, token refresh and account removal. What
differs from the companion described below:

- Credentials are in `/data/prosperotube/accounts/oauth-profiles.json` on the
  console (same format; no lock file, one process writes it).
- The guards the Python service kept "across worker loss" sit above the
  library object and hold across its rebuilds; nothing survives an app restart
  except the accounts themselves.
- "OAuth TV resolution" does not apply: every profile resolves videos as Guest
  does, and the account session is used for playback only behind
  `account_playback`, which is off. Health carries `account_playback`.
- `profile_managed`, cookie profiles and `auth_storage_unconfirmed` do not
  exist there.

Reasons and what is unverified: [decisions](DECISIONS.md) (2026-10-13),
`standalone/README.md`.

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


## Retained browsing groups (service integration)

Ordinary worker catalog pages now carry `playback_context: {group, index}`.
The opaque group reference belongs to the selected profile and preserves repeated
video occurrences through local pages and upstream continuations. Playlist and
channel contexts retain their existing contracts. Upcoming entries remain visible
but are ineligible for next/previous selection, following the pinned SmartTube
`SuggestionsController.findNextSectionVideo` and `getPreviousFromGroup` behavior.

`group_window` accepts the group reference, anchored `id`/`index`, optional
`target`/`direction`, and the channel/name verification flags. Responses use
`group:<reference>` in the existing window shape. Forward selection attempts at
most one upstream continuation before reporting no eligible item. Concurrent
browser and playback requests share the upstream continuation and append its
positions once. A failed fetch can be retried. Foreign, expired and mismatched
anchors fail without returning another group's data.

Retention is bounded to 32 groups, 4096 records each, with six hours of inactivity
expiry. Browsing starts a new group after capacity/eviction; navigation beyond a
retained group's capacity reports failure. These bounds and long-session behavior
still require acceptance. Native selection now recognizes these references and sends anchored group-window
requests without treating the opaque reference as a YouTube playlist ID. Manual
Next/Previous preserve group positions; continuous completion traverses the group,
and playlist-only completion excludes ordinary browsing groups. Failed forward
group lookup falls back to next-video metadata. OAuth search/catalog conversion
preserves supplied group occurrences without deduplicating repeated video IDs.
When the library worker is enabled, Guest search/channel/playlist catalogs now
use its bounded continuation adapter too, retaining the same group references
used by native navigation. Guest video resolution stays on the existing public
yt-dlp extractor; Guest cannot request account credentials. Worker failures are
reported rather than replacing a retained group with unrelated extractor rows.
Without the library worker, the original Guest catalog behavior remains available
without retained browsing groups. PS5 and live-service acceptance remain pending.


Primary cookie-account catalog searches now use the worker continuation/group
adapter too. Its requests carry the selected profile ID, while video extraction
continues through that profile's cookie-backed resolver. Revoking the adapter
also invalidates that resolver and its subprocesses. Catalog operations cannot
request OAuth credentials. Existing secondary-account/brand-channel video
restrictions remain; this change does not claim those playback contexts work.


Retained group windows reuse channel metadata only after the entire verification
succeeds and the original group still belongs to the caller. Partial failures do
not update retained rows. Repeated occurrences share verified identities, reducing
repeat lookups across adjacent windows. Unrequested display names are omitted
from window responses; requesting name verification still requires complete,
valid names for native filtering.


Concurrent requests for the same catalog cursor share one resulting page and
successor cursor. Ownership/expiry is rechecked after fetching, before exposing
new rows. Upstream failure releases the pending operation so a later request can
retry; a completed request retains its exact result for idempotent retries.


Primary Home RichItem videos now follow SmartTube's watched-suggestion threshold:
explicitly watched entries, progress above 95 percent, or its 10-percent minimum
marker are omitted. Partially watched entries, nested shelves, search and playlists
remain available. The same wrapper processes Home continuation pages before they
become retained groups. Unknown Home layouts are preserved; other suggestion layouts are still pending.


Catalog rows carry `watch_percent` (`-1` for unknown, otherwise 0–100), including
through Guest/cookie/OAuth catalog adapters. Native resume ignores percentages up
to 10 and at 100, matching SmartTube. Partial remote progress supplies the displayed
resume position; unknown or fully watched remote state can use existing local
state. Remote metadata does not overwrite saved local progress. Home filtering
happens before this local-state merge, following the pinned source order.


Watch progress also supports modern Lockup thumbnail bottom overlays through the
pinned parser's progress-bar model. These values follow the same numeric bounds,
Home filtering and native resume path as legacy resume overlays.


Topic TV tiles and raw web video rows preserve structured live markers, scheduled
start times and resume overlays. Future scheduled videos stay visible but are
excluded from automatic source-group traversal; progress uses the same catalog
resume path. Live detection uses structured styles, independent of badge labels.


Retained source groups keep valid parsed upcoming timestamps and reevaluate them
when navigating. An occurrence becomes eligible at its scheduled time without
rebuilding the group or changing positions. Unknown timestamps retain the
explicit upcoming flag; stream resolution still determines actual availability.


## Search, sections, description and feedback contracts

These library actions use `POST /v1/library` or a `POST /v1/jobs` request with
`action: "library"` and the nested `library` object. Every request accepts the
usual `profile` (default `guest`). Unknown fields and values are rejected with
HTTP 422 before the worker is reached; the worker repeats the validation. List
responses use the ordinary page shape `{"api_version":1,"kind":"catalog",
"items":[?],"cursor":"?","has_more":bool}` with at most 24 items; pass `cursor`
to the ordinary `continue` action with the same profile. Cursors are opaque,
profile-owned and expire after two minutes; YouTube continuation tokens never
leave the companion. Offline tests cover these contracts with the pinned parser;
none has been run against the live service, an account or a PS5.

### `search`

Request fields:

| Field | Values | Default |
| --- | --- | --- |
| `query` | 1?200 characters, not blank, no control characters | required |
| `upload_date` | `any`, `today`, `week`, `month`, `year` | `any` |
| `duration` | `any`, `short` (under 3 min), `medium` (3?20 min), `long` (over 20 min) | `any` |
| `type` | `all`, `video`, `channel`, `playlist`, `movie`, `shorts` | `all` |
| `features` | list of distinct `hd`, `subtitles`, `creative_commons`, `3d`, `live`, `purchased`, `4k`, `360`, `location`, `hdr`, `vr180` (at most 11) | `[]` |
| `order` | `relevance`, `view_count` | `relevance` |

These are exactly the values the pinned YouTube.js 18.1.0 search-filter encoder
offers. It has no "last hour" upload date and no upload-date or rating order, so
`hour`, `upload_date` and `rating` are rejected like any other unknown value
(HTTP 422, worker code `invalid_search`). The option fields are accepted only
with `action: "search"`. Guest and account profiles can search.

The response is a catalog page in upstream order. Each item has a `kind`:

- `video`: the ordinary video row (`id` is the 11-character video ID, `title`,
  `subtitle`, `overview`, `art_url`, `channel_id`, `duration_sec`, `is_live`,
  `is_upcoming`, `played`, `watch_percent`, `is_folder: false`) with the
  retained-group `playback_context: {group, index}`.
- `channel`: `{"kind":"channel","id":"channel:UC?","channel_id":"UC?",
  "title":?,"subtitle":?,"overview":?,"art_url":?,"is_folder":true}`.
  `title` is at most 192 characters, `subtitle` is the subscriber text (at most
  64, may be empty), `overview` the description snippet (at most 320). Open it
  with the existing `channel:UC?` catalog parent.
- `playlist`: the existing playlist folder row (`id` is
  `library:playlist:<playlist id>`, `title`, `subtitle`, `art_url`,
  `is_folder: true`) plus `kind`.

Advertisements are omitted, one level of result shelves is flattened in place,
and a repeated identity appears once per page. Continuation pages come from the
retained filtered feed, so they keep the query and all options without the
client resending them. The top-level `search` job and `GET /v1/search` are
unchanged: they return videos only and take no options.

### `search_suggestions`

Request: `query`, 0?200 characters without control characters (an empty query
returns the service's default completions). Response:
`{"api_version":1,"kind":"search_suggestions","query":"<trimmed query>",
"suggestions":["?"]}` with at most 15 distinct strings of at most 200
characters each, control characters removed. The request uses the selected
profile's client and YouTube's completion origin. An unreadable reply fails with
`upstream_format` (502). There is no cursor.

### `description`

Request: `id` (11-character video ID). Response:
`{"api_version":1,"kind":"description","id":?,"title":?,"channel":?,
"channel_id":"UC?"|"","published":"YYYY-MM-DD"|"","views":"<digits>"|"",
"description":?,"truncated":bool}`. `description` is the complete plain text
with newlines, cut to 20,000 UTF-16 code units without splitting a surrogate
pair; `truncated` is true only when text was cut. `title` and `channel` are at
most 192 characters. `published` and `views` are empty when the player metadata
lacks them. A reply for another video fails with `upstream_format` (502). The
adapter makes one pinned `getBasicInfo` player request; Guest is allowed.

### Trending, Shorts and own uploads

`{"action":"topics","section":"trending"}` and `"section":"shorts"` follow the
existing curated-section contract: shelf folders `section:<section>:<id>` opened
with `topic_open`, or ? when YouTube returns one grid without shelves ? a flat
page of videos, both continued with `continue`. Shorts tiles are ordinary video
rows. `"section":"my_videos"` lists the signed-in profile's own uploads the same
way; Guest, a profile without credentials or an expired session fails with
`account_required` (401) before any request. The three sections use YouTube's
TV browse identities `FEtrending`, `FEshorts` and `FEmy_videos`, taken from the
SmartTube service. Checked live on 2026-10-11 as Guest: YouTube answers
`FEtrending` and `FEshorts` with HTTP 400 on the TV and the web client, so both
sections fail with `catalog_unavailable` (404) and nothing is listed. Their
layouts for a signed-in profile and `FEmy_videos` remain unverified.

### `channels`

Account action, no fields besides `profile`. Returns a catalog page of the
profile's subscribed channels as `kind: "channel"` folder rows (the shape under
`search`), each channel once, continued with `continue`. Guest fails with
`account_required` (401). The pinned parser does not expose a new-content
marker; the opt-in `new_content` field of the third batch reads it from the
raw guide response instead.

### `notifications`

Account action, no fields besides `profile`. Returns a catalog page of video
rows (`kind: "video"`), one per notification that opens a video: `id` is the
video ID, `title` the notification message, `subtitle` the sent-time text,
`art_url` the video thumbnail, and `unread` a boolean. Other notification types
are omitted. Rows carry the usual retained-group `playback_context`; a video
named by two notifications appears twice. Continue with `continue`.
Notification identifiers, menus and click-tracking endpoints stay private. An
unsupported menu fails with `upstream_format` (502). Hiding a notification is
not implemented (the pinned menu exposes no feedback token).

### Recommendation feedback

`home` pages (including Home rows and filters) for a configured account profile
add three fields to a video row when YouTube supplied feedback for that card:
`feedback_cursor` (opaque reference, at most 64 characters),
`feedback_not_interested` and `feedback_channel` (which choices exist). Guest
rows never carry them. The feedback tokens stay in the worker; choices are
recognized by the card menu's structured icon, not by localized labels, and an
ambiguous menu exposes no choice. References belong to the profile and card,
expire after 30 minutes, and at most 1,024 are retained (oldest dropped first).

`{"action":"feedback","profile":?,"id":"<video id>","cursor":"<feedback_cursor>",
"feedback":"not_interested"|"dont_recommend_channel"}` submits that card's
retained token once. Success is
`{"api_version":1,"success":true,"video_id":?,"feedback":?}` and requires
YouTube's processed confirmation; the native client should then remove the
card. Errors: `account_required` (401) for Guest or a signed-out session,
`cursor_expired` (409) for an expired, foreign or mismatched reference,
`invalid_feedback` (400) for a choice the card does not offer,
`feedback_unconfirmed` (409 or 502) when the result is unknown, and
`feedback_busy` (429) when 256 intents are already retained. Each
reference/choice pair permits one attempt: a confirmed repeat returns the same
result, an unconfirmed or in-flight one is never resubmitted. The Python
supervisor keeps that gate for 30 minutes across worker restarts, timeouts and
cancellation, and clears it when the profile is removed. Shorts cards do not
carry feedback references; Subscriptions cards carry the `hide` choice described
under [Hide a Subscriptions entry](#hide-a-subscriptions-entry).

### Fixture mode

The offline fixture library answers `topics` for `trending`, `shorts` and
(account profiles only) `my_videos`, `search_suggestions` and `description` for
its generated videos. `search`, `channels`, `notifications` and `feedback`
report `unknown_operation` there.


## Locale, content filters, channel pages and account changes

Second batch of library contracts. Transport, page shape, cursors and limits are
those of the previous section. Every new request field is optional and its
default is the behaviour of earlier clients; every new response field is
additive. A companion that supports them lists `locale`, `thumbnail_source` and
`storyboards` (always) and `content_filters` and `channel_pages` (with the
library worker) in `GET /v1/health` `features`. An older companion rejects the
unknown fields with HTTP 422, so check `features` before sending them. Offline
tests cover these contracts with the pinned parser; none has been run against
the live service, an account or a PS5.

### Content language and country: `hl`, `gl`

Accepted on every library action, on the top-level `search`, `catalog` and
`details` jobs, on `GET /v1/search`, `GET /v1/catalog` (query parameters `hl`,
`gl`) and `GET /v1/video/{id}` (`hl`), and `hl` alone on a resolve request
(`/v1/resolve` and the `playback` object of a resolve job). Empty string (the
default) keeps the service default.

- `hl` — one of the 49 tags SmartTube ships (its `languages.xml` plus the `fa`
  and `th` resource folders), case-sensitive:
  `ar az bg bn ca cs da de el en en-NZ es et fa fi fr gl he hi hr hu hy id it ja
  ko lt lv mo nb nl pl pt-BR pt-PT ro ru sk sl sq sr sv te th tjl tr uk vi zh-CN
  zh-TW`. The companion sends YouTube's own code: identical except
  `he`→`iw`, `nb`→`no`, `mo`→`ro`, `pt-BR`→`pt`, `en-NZ`→`en-GB`. `tjl` is
  accepted but changes nothing, because YouTube has no such interface language.
- `gl` — one of 89 upper-case ISO country codes from SmartTube's `countries.xml`
  (its `GB-SCT`, `nl_BE` and `fr_BE` entries are covered by `GB` and `BE`, and
  its Armenia entry is sent as `AM`): `US GB HR CA UA RU BY KZ AZ AM RS EE BG DE
  RO ES PL FI LV FR HU NL BE NO CZ IT GR DK LT MD SK SI SE SG NZ NG AU CH IL SA
  MA BD BR PT AR MX HK MY JP IN KE TR EG VN CN TW ID KR PK BO CL CO CR EC SV GT
  HN NI PA PR PY PE DO UY VE IE PH IQ AT TZ NP ZA ZW DZ TH MK TN LK JO`.

Any other value fails: HTTP 422 on JSON bodies, HTTP 400 `invalid_locale` on
query parameters, worker code `invalid_locale`.

The worker keeps one InnerTube client per profile and locale (`hl`/`gl` become
the client context language and location; at most 16 localized clients are
retained, oldest dropped first). A page, cursor, section folder or retained
group stays bound to the client that opened it, so `continue`, `topic_open`,
`suggestions_filter` and the window actions ignore `hl`/`gl` and can never mix
locales. An OAuth profile signs each localized client in separately. Worker
catalog caches in the Python service are keyed by `hl`, `gl` and `hide`. On the
yt-dlp path only `hl` has an equivalent (its `lang` extractor argument, applied
when yt-dlp lists the code; every tag above except `tjl` qualifies); `gl` is
ignored there, and the extraction cache is partitioned by `hl`.

### Hide-content switches: `hide`

`hide` is a list of distinct names (at most 16) on library list actions and on
the top-level `search`/`catalog` jobs. The names are SmartTube's sixteen
"Hide unwanted content" switches:

| Name | Applies to |
| --- | --- |
| `shorts_all` | Shorts in every list |
| `shorts_home`, `shorts_search`, `shorts_subscriptions`, `shorts_history`, `shorts_channel`, `shorts_trending` | Shorts in that section |
| `mixes` | mix playlist cards (playlist ID starting with `RD`) in every list |
| `watched_home` | watched cards in the primary Home grid |
| `watched_subscriptions`, `watched_watch_later`, `watched_notifications` | watched videos in that list |
| `streams_subscriptions` | live streams in Subscriptions |
| `upcoming_subscriptions`, `upcoming_home`, `upcoming_channel` | upcoming videos in that section |

Sections follow the action: `home`; `search`; `subscriptions`; `history`;
`watch_later`; `notifications`; `channel` for `channel`, `channel_search` and
the `channel:UC…` catalog parent; `trending` for `topics`/`topic_open` with
`section: "trending"`. A Home or channel shelf opened with `suggestions_filter`
keeps the section of the page that listed it. Other lists honour only
`shorts_all` and `mixes`.

**Default.** When `hide` is absent (or `null`) nothing new is hidden and Home
keeps its earlier hard-wired rule (watched cards are dropped from the primary
grid). When `hide` is present — even `[]` — it is the complete selection: Home
drops watched cards only if `watched_home` is listed. "Watched" is the existing
rule: marked played, more than 95 % watched, or exactly 10 %. Notification rows
carry no watch state, so `watched_notifications` currently hides nothing.

The selection is stored with the cursor, so continuation pages apply it without
the client resending it; `continue` ignores `hide`. Hidden rows keep their
source and retained-group positions (`playback_context` indices are sparse), and
a page can be empty while `has_more` is true — keep following the cursor.
Unknown or repeated names fail with HTTP 422 / worker code `invalid_filter`.
On a companion without the library worker (pure yt-dlp catalogs) `hide` is
accepted and ignored.

Video rows now carry `is_short: true` when the card is a Short (omitted
otherwise; `false` on rows from the top-level jobs). The evidence is structural:
a Shorts tile type, a `SHORT` lockup, a reel/`/shorts/` watch endpoint or the
`SHORTS` time-status overlay; never the title or the duration.

### Channel pages: `channel` and `channel_search`

`{"action":"channel","channel":"UC…","tab":…,"channel_sort":…}`

| Field | Values | Default |
| --- | --- | --- |
| `channel` | channel ID (`UC` + 22 characters) | required |
| `tab` | `home`, `videos`, `shorts`, `live`, `playlists` | `videos` |
| `channel_sort` | `newest`, `popular`, `oldest` | `newest` |

`{"action":"channel_search","channel":"UC…","query":"1–200 characters"}`
searches inside the channel; `tab` and `channel_sort` are not accepted there.
Both work for Guest and account profiles and accept `hl`, `gl`, `hide` and
`thumbnails`. `id` and `playlist` must be empty.

The response is an ordinary catalog page plus, on the first page only, a
`channel` object:

```json
{"id":"UC…","title":"…","tabs":["home","videos","shorts","playlists"],
 "search":true,"tab":"videos","sort":"newest","sorts":["newest","popular","oldest"]}
```

`tabs` lists the sections this channel offers (in the order above), `search`
whether `channel_search` is available, `tab` the opened section (`"search"` for
`channel_search`), `sorts` the orders offered for it and `sort` the applied one
(empty when no order is offered). Rows by section:

- `videos` in the default order: video rows with the channel position
  `playback_context: {"channel":"UC…","index":n}`, exactly like the
  `channel:UC…` catalog parent, so `channel_window` navigation applies.
- `videos` in another order, `live`, `shorts` and `channel_search`: video rows
  with the retained-group context `{group, index}`. Shorts rows have
  `is_short: true`, the channel as `subtitle`/`channel_id` and no duration.
- `playlists` (and playlists found by `channel_search`): playlist folder rows
  `library:playlist:<id>`.
- `home`: one folder row per titled shelf (`id` `library:filter:<uuid>`,
  `subtitle` `"Channel section"`, opened with `suggestions_filter`, two-minute
  lifetime, video and playlist rows inside), followed by video cards that are
  not inside a shelf. No continuation. Shorts shelves and featured-channel
  shelves are not listed.

Continue with `continue`. Errors: `channel_tab_unavailable` (404) for a section
the channel lacks, `channel_sort_unavailable` (404) for an order that is not
offered, `channel_search_unavailable` (404), `invalid_search` for the query,
`invalid_identity`/`invalid_request` for a malformed request.

Orders are recognized by position, because their labels are localized: a chip
bar of two or three chips whose first chip is selected is read as newest,
popular, oldest. Any other layout (drop-down, four chips, another selection)
reports `sorts: []`. Checked live on 2026-10-11 as Guest on the Videos, Shorts
and Live sections of a large channel: three chips, and the second and third
return the most viewed and the oldest uploads. The Playlists section has no
order option here.

Cards of a channel's own Videos and Live sections name no channel upstream
(their first metadata part is the view count), so the companion fills
`subtitle` and `channel_id` from the page owner; a card that links another
channel keeps it. The same holds for the top-level `catalog` of a channel.
Scheduled streams of the Live section carry only a localized "Upcoming" badge,
so `is_upcoming` stays false for them.

### Save or remove another channel's playlist

`{"action":"playlist_save","profile":…,"playlist":"<playlist id>","session":"<confirmation>"}`
adds the playlist to the account library; `playlist_unsave` removes it.
`session` is a caller-chosen confirmation identifier (1–64 of
`A-Za-z0-9_-`), unique per intended change, as for `mark_watched`. `WL` and
`LL` are rejected. Success:
`{"api_version":1,"success":true,"playlist_id":"…","saved":true|false}`.

One confirmation permits one attempt: a confirmed repeat returns the same
result; a different target under the same `session`, an in-flight or an
unconfirmed attempt returns `playlist_unconfirmed` (409, or 502 for the attempt
that was not confirmed) and is never sent again — reload the Playlists list to
see the actual state. Other errors: `account_required` (401), `unknown_profile`
(404), `invalid_request` (400), `account_busy` (429, 256 retained
confirmations). The Python supervisor keeps the gate for 30 minutes across
worker restarts, timeouts and cancellation, and clears it with the profile.
The worker uses the pinned `playlist.addToLibrary` / `removeFromLibrary`.

### Pause, resume and clear the account watch history

`{"action":"history_pause"|"history_resume"|"history_clear","profile":…,"session":"<confirmation>"}`
with the same one-attempt `session` gate. Success:
`{"api_version":1,"success":true,"history":"paused"|"active"|"cleared"}`.
`history_clear` removes the whole account watch history; ask the user first.

The `history` list page gains `history_paused: true|false` on its first page
when YouTube offers exactly one of the pause and resume controls (the field is
absent when the state is unknown).

Errors: `history_control_unavailable` (404) when the account's History page does
not offer that control — nothing was sent, and the same `session` may be used
again; `history_unconfirmed` (409/502) when the result is unknown or the
`session` was already used; `account_required`, `unknown_profile`,
`invalid_request`, `account_busy` as above.

The pinned YouTube.js 18.1.0 has no method for these controls. The worker reads
them from the parsed History page: a feed-action button whose confirmation
dialog's confirm button carries one feedback token, recognized by the button
icon (`DELETE` = clear, `PAUSE` = pause, `PLAY` = resume), never by its label;
duplicates with different tokens, disabled buttons and direct (unconfirmed)
buttons are ignored. The token is submitted once to `/feedback` and YouTube's
processed confirmation is required. The icon names are inferred from the web
client's layout and have not been verified against a live account; an
unrecognized layout reports `history_control_unavailable` and changes nothing.

### Hide a Subscriptions entry

`subscriptions` rows for an account profile now use the recommendation-feedback
reference of the previous section: a card whose menu carries a hide token gets
`feedback_cursor` and `feedback_hide: true` (plus `feedback_not_interested` /
`feedback_channel`, normally false there). Submit it with
`{"action":"feedback","profile":…,"id":"<video id>","cursor":"<feedback_cursor>","feedback":"hide"}`;
success is `{"api_version":1,"success":true,"video_id":…,"feedback":"hide"}`
and the client should then remove the card. Lifetime, one-attempt rule and
errors are those of recommendation feedback. `feedback_hide` appears only when
true (Home rows may carry it too). The token is recognized by the menu icon
`HIDE` (inferred, unverified live). Hiding a notification is not implemented:
the pinned notification menu exposes no feedback token.

### Thumbnail source: `thumbnails`

`thumbnails` is `default` (unchanged), `start`, `middle` or `end`, accepted on
every library action, the top-level `search`/`catalog`/`details` jobs and
`GET /v1/search` / `GET /v1/catalog`. For the last three values each video
row's `art_url` becomes `https://i.ytimg.com/vi/<id>/hq1.jpg`, `hq2.jpg` or
`hq3.jpg` (SmartTube's start/middle/end frames). Live and upcoming rows,
folders, and rows whose image is not a YouTube thumbnail keep theirs. Because
the DeArrow decoration runs afterwards, `dearrow_art_url` is unchanged when a
submission exists, a video without a submission shows the chosen frame, and a
failed generated DeArrow thumbnail falls back to the chosen frame.

### `description` without the worker

When the companion runs without the library worker, `/v1/library` and library
jobs still answer `description` (and only that action) from one yt-dlp video
extraction, with the response shape of the previous section. `published` comes
from the upload date and `views` from the view count; `hl` selects translated
metadata. List `overview` fields stay bounded as before.

### Fixture mode

The offline fixture library ignores `hl`, `gl` and `hide`, applies `thumbnails`
only to YouTube image addresses (its generated thumbnails are local), and
reports `unknown_operation` for `channel`, `channel_search`, `playlist_save`,
`playlist_unsave` and the three history controls.


## Kids, reminders, playlist order, details, comment posting and host options

Third batch of contracts. Transport, page shape, cursors, limits and the
one-attempt `session` rule are those of the two previous sections. Every new
request field is optional with the earlier behaviour as its default; every new
response field is additive. `GET /v1/health` `features` lists
`translated_captions`, `video_details` and `subscriptions_import` (always),
`kids`, `stream_reminder`, `playlist_order`, `subscribe_notifications`,
`comment_post`, `search_first` and `channel_new_content` (with the library
worker, not in fixture mode), `dislikes` (only with `--return-youtube-dislike`)
and `proxy` (only with `--proxy`). An older companion rejects the unknown
fields and actions with HTTP 422. Offline tests cover these contracts with the
pinned parser and synthetic responses; none has been run against an account
or a PS5. The Guest reads (Kids, `video_details`, `first_playable`,
`translated_captions`) were checked live on 2026-10-11, see
[Guest live check](#guest-live-check-of-2026-10-11). Statements marked *inferred* describe upstream
layouts that the pinned YouTube.js 18.1.0 does not parse and that could not be
checked offline; an unrecognised layout sends nothing and returns the stated
error.

### Kids section

`{"action":"topics","section":"kids"}` returns one folder row per YouTube Kids
category: `{"id":"section:kids:<uuid>","title":"<category>","subtitle":"YouTube
Kids","art_url":"","is_folder":true}`. Open one with
`{"action":"topic_open","section":"kids","id":"<uuid>"}`; it returns ordinary
video rows with the retained-group `playback_context`, paged with `continue`
(the companion pages the category locally; YouTube's own Kids continuation is
not requested). References belong to the profile and expire after ten minutes
(`cursor_expired`, 409). Guest is allowed. The worker uses the pinned Kids
client (`kids.getHomeFeed()`, browse identity `FEkids_home`, client `YTKIDS`)
and its `selectCategoryTab()`. Errors: `upstream_format` (502) when the page has
no category tabs, `catalog_unavailable` (404) when the Kids client is missing.

### Stream reminder: `stream_reminder`

`{"action":"stream_reminder","profile":…,"id":"<video id>","reminder":"status"|"set"|"unset","session":"<confirmation>"}`

- `status` (any profile, no `session`):
  `{"api_version":1,"kind":"stream_reminder","id":…,"upcoming":bool,"available":bool,"reminder_set":bool}`.
  `available` is true only when YouTube offers the "Notify me" control to this
  profile; `reminder_set` is its current state (false when unavailable).
- `set` / `unset` (account profile, `session` required):
  `{"api_version":1,"success":true,"id":…,"reminder_set":true|false}`. When the
  reminder already has the requested state nothing is sent.

Errors: `reminder_unavailable` (404) when no control is offered — nothing was
sent and the same `session` may be used again; `reminder_unconfirmed` (409, or
502 for the attempt that was not confirmed) — never sent again;
`account_required` (401), `unknown_profile` (404), `invalid_request` /
`invalid_identity` (400), `account_busy` (429), `upstream_format` (502) for a
reply about another video.

*Inferred:* the pinned parser keeps neither the watch-page toggle nor its
state, so the worker reads the raw `/player` response: exactly one
`toggleButtonRenderer` under `playabilityStatus` whose default endpoint is
`addUpcomingEventReminderEndpoint` and whose toggled endpoint is
`removeUpcomingEventReminderEndpoint`, each with `params`, plus a boolean
`isToggled`; the change posts those `params` to
`notification/add_upcoming_event_reminder` or
`notification/remove_upcoming_event_reminder`. SmartTube's own reminder is
local (it polls the video and starts playback when it goes live); a native
client can build that from `status` and the existing `is_upcoming` /
`upcoming_start_ms` row fields without this account change.

List rows: the existing `is_upcoming` flag marks upcoming videos (no separate
`upcoming` field was added). A row gains `reminder_set: true|false` only when
the parsed card states it, which in the pinned parser is the legacy grid card
(`GridVideo`); search, Subscriptions and channel rows leave it out (unknown).

### Playlist order: `playlist_order`

`{"action":"playlist_order","profile":…,"playlist":"<playlist id, WL or LL>","playlist_order":…,"session":"<confirmation>"}`
for an account profile. `id` must be empty.

- Without `playlist_order` (or `""`): the offered orders,
  `{"api_version":1,"kind":"playlist_order","playlist_id":…,"editable":bool,"orders":["added_newest",…],"order":"<selected>"|""}`.
  `editable` is false and `orders` empty for a playlist the account cannot
  sort (another channel's playlist, or an unrecognised layout).
- With `playlist_order` one of `added_newest`, `added_oldest`, `popular`,
  `published_newest`, `published_oldest` and a `session`:
  `{"api_version":1,"success":true,"playlist_id":…,"order":…}`. YouTube stores
  the order with the playlist; reload the `playlist` list afterwards.

These are SmartTube's five choices (date added newest/oldest, popularity, date
published newest/oldest); it offers no manual order. Errors:
`playlist_order_unavailable` (404) when the page does not offer that order —
nothing was sent, the `session` stays usable; `playlist_unconfirmed` (409/502);
`account_required`, `unknown_profile`, `invalid_request`, `account_busy`.

*Inferred:* the pinned playlist parser drops the sort menu, so the worker reads
the raw browse response (`VL<playlist>`): `sortFilterSubMenuRenderer` items
whose `playlistEditEndpoint` targets this playlist with exactly one
`ACTION_SET_PLAYLIST_VIDEO_ORDER` action. Its numeric `playlistVideoOrder` is
mapped 1–5 to the five names in the order above (SmartTube's
`PLAYLIST_ORDER_*` constants, recalled rather than read: its service module is
absent from the reference tree). Only a value the page itself offered is sent,
through the pinned `browse/edit_playlist` path, and
`status: "STATUS_SUCCEEDED"` is required. Viewing another channel's playlist
has no order option: the browse endpoint offers none there.

### Notification level on subscriptions

`subscribe` accepts `notifications`: `all`, `personalized` or `none` (default
`""` = unchanged behaviour). The response then carries
`"notifications":"<level>"` when the level was applied, or `""` when the
subscription succeeded but the level was not confirmed (do not subscribe
again; send `subscribe_notifications`).

`{"action":"subscribe_notifications","profile":…,"id":"UC…","notifications":"all"|"personalized"|"none"}`
changes the level of an existing subscription:
`{"api_version":1,"success":true,"playlist_id":"","notifications":…}`, or
`operation_failed` (502). Both use the pinned
`interact.setNotificationPreferences`. No `session` is needed: the change is
idempotent. `notifications` on any other action is rejected.

### Likes and dislikes

`description` gains `like_count`: `"<digits>"` or `""`. The worker's
`description` reads player metadata only, which has no like count, so it is
filled on the yt-dlp path (companion without the worker) and by
`video_details` below.

`GET /v1/dislikes/{video id}` (bearer token) relays Return YouTube Dislike,
which is where SmartTube takes dislikes from (recalled; its service module is
absent from the reference tree). It is off unless the host starts the
companion with `--return-youtube-dislike`:

- disabled: `{"api_version":1,"enabled":false}` and no outbound request;
- enabled: `{"api_version":1,"enabled":true,"id":…,"available":true,"likes":n,"dislikes":n}`
  or `{…,"available":false}` when the service fails or its answer is invalid.

The companion sends only `GET https://returnyoutubedislikeapi.com/votes?videoId=<id>`
(no account data) and accepts only a JSON object of at most 64 KiB whose `id`
equals the requested video and whose `likes` and `dislikes` are plain integers
from 0 to 10^12. Known counts are cached for one hour, unavailable answers for
five minutes, 128 videos at most. Enabling it discloses watched video IDs to
that third party; say so in the native setting.

### Video details: `video_details`

`{"action":"video_details","profile":…,"id":"<video id>"}`, Guest allowed:

```json
{"api_version":1,"kind":"video_details","id":"…","title":"…",
 "channel":{"id":"UC…","name":"…","subscribers":"1.2M subscribers","art_url":"https://yt3.ggpht.com/…"},
 "published":"YYYY-MM-DD","views":"1234567","like_count":"4321",
 "description":"<at most 600 UTF-16 units>","description_truncated":true,
 "duration_sec":125,"badges":["live","upcoming","4k"],"has_related":true}
```

`title` and `channel.name` are at most 192 characters; `channel.id`,
`published`, `views`, `like_count` and `channel.subscribers` (display text, at
most 64 characters) are `""` when unknown. `channel.art_url` is an HTTPS
`ytimg.com` / `ggpht.com` / `googleusercontent.com` address like other artwork,
or `""`. `description` is the excerpt for the screen (the full text stays with
`description`). `badges` holds any of `live`, `upcoming`, `4k`; `4k` appears
when the watch response lists a format of at least 2160 lines. `has_related`
says whether `suggestions` for this video would list anything. The worker makes
one pinned `getInfo` request (player + watch-next); a reply for another video
fails with `upstream_format` (502).

Without the library worker the action is answered from one yt-dlp extraction
with the same shape: `channel.subscribers` is then a plain number string,
`channel.art_url` is `""` and `has_related` is `false`.

### Comments: state, count and posting

Comment rows (`comments`, `replies`) gain `liked`, `disliked` (the viewer's own
rating), `hearted` (creator heart), `member` (channel member) and
`respond_cursor` (opaque reference for replying, at most 64 characters, `""`
when the profile cannot reply to that comment). `pinned`, `owner` and the
author avatar `art_url` already existed. The first `comments` page also carries
`comment_count` (YouTube's display text, at most 32 characters, `""` when
absent) and `can_post` (the signed-in profile may post a top-level comment).

`{"action":"comment_post","profile":…,"id":"<video id>","text":…,"session":"<confirmation>"}`
posts a top-level comment;
`{"action":"comment_reply","profile":…,"id":"<video id>","cursor":"<respond_cursor>","text":…,"session":"<confirmation>"}`
replies to the comment the reference came from. `text` is 1–1000 characters
after trimming, newlines allowed, no other control characters; the whole
request must still fit the 8 KiB request limit. Success:
`{"api_version":1,"success":true,"id":…,"session":…,"reply":false|true}`.
Reload the list to see the new comment; its ID is not returned.

One `session` permits one attempt for one exact video, reference and text: a
confirmed repeat returns the same result, anything else returns
`comment_unconfirmed` (409, or 502 for the attempt that was not confirmed) and
is never sent again. Other errors: `invalid_comment` (400), `cursor_expired`
(409; reply references belong to the profile, expire after 30 minutes and 256
are retained), `account_required`, `unknown_profile`, `account_busy`. The
worker uses the pinned `interact.comment` and the pinned comment `reply()`;
confirmation requires `actionResult.status: "STATUS_SUCCEEDED"` (*inferred*
reply shape). Like/dislike of a comment remains `comment_rate`.

### First playable search result

`search` accepts `first_playable: true` (all search options still apply). The
response is then not a page:
`{"api_version":1,"kind":"search_first","query":"<trimmed query>","found":bool,"item":{video row}|null}`.
`item` is the first row of the first result page that is a video and not
upcoming (live is playable), with the usual retained-group `playback_context`;
no cursor is retained. `found: false` means the first page had no such row.

### New-content mark of subscribed channels

`channels` accepts `new_content: true`. Channel rows whose state YouTube's
guide reports then carry `has_new: true|false`; rows without the field are
unknown. The mark costs one extra guide request and its failure never fails
the list. *Inferred:* the pinned guide parser drops the marker, so the worker
reads the raw `/guide` response (`guideEntryRenderer.presentationStyle` =
`GUIDE_ENTRY_PRESENTATION_STYLE_NEW_CONTENT` / `…_NONE`, keyed by the entry's
channel ID).

There is no "mark as watched" action: the pinned library has no call for it.
SmartTube clears the mark by loading the channel's uploads, so open the
existing `channel` action (`tab: "videos"`) under the account profile for
"mark channel as watched", and keep a local last-seen marker per channel if
the native list must be reliable.

### Subscriptions import

`POST /v1/subscriptions/import` (bearer token) takes the export file itself as
the request body (any content type, UTF-8 with or without a byte-order mark,
at most 1 MiB; larger bodies get HTTP 413) and returns

```json
{"api_version":1,"kind":"subscriptions_import","format":"newpipe",
 "channels":[{"channel_id":"UC…","title":"…"}],"skipped":0,"truncated":false}
```

`format` is `newpipe`, `pockettube` or `grayjay`. `channels` holds each channel
once, in file order, at most 2,048 (`truncated` says more were present);
`title` is at most 192 characters and `""` when the file has none; a `group`
field appears only for PocketTube (its group name, at most 64 characters).
`skipped` counts entries without a usable `UC…` identity (handles, user names,
other services). Nothing is stored and no account is touched: the native
client adds the list to its local follows. It works without the library worker
and in fixture mode. An unrecognised file fails with `invalid_import` (400).

Formats (*inferred* from the applications' export files; SmartTube's importer
is in its absent service module):

- NewPipe: `{"subscriptions":[{"service_id":0,"url":"https://www.youtube.com/channel/UC…","name":"…"}]}`;
  entries of other services are skipped.
- PocketTube: `{"<group>":["UC…",…],…}`; keys starting with `ysc_` are ignored.
- GrayJay: a JSON list whose entries are channel addresses, or subscription
  records (objects, or JSON text of one) with `channel.url` / `channel.name`
  or `url` / `name`.

### Host options: `--proxy` and `--return-youtube-dislike`

`--proxy URL` sends every YouTube connection of the companion through one web
proxy: the yt-dlp resolvers (including the OAuth TV adapter), the library
worker's requests, the media/caption/storyboard relay, DeArrow, SponsorBlock,
the dislike relay and the phone-connection client. `URL` is
`http://[user:password@]host[:port]` or `https://…` (default ports 80 and 443;
no path, query or fragment; percent-encode special characters in the
credentials). SOCKS is refused at start-up: the relay's HTTP client has no
SOCKS support here, and media addresses only work from the address that
resolved them, so all components must share the proxy. The address is never
printed or returned: a start-up error does not repeat it, the worker receives
it through its environment (not its arguments), and `GET /v1/health` reports
only `"proxy": true|false` (plus `proxy` in `features` when enabled). The
worker opens one CONNECT tunnel per request and never falls back to a direct
connection; a malformed setting stops the worker (`library_transport`), a
failing proxy returns `proxy_failed` (502).

`--return-youtube-dislike` enables the dislike relay described above.

### Fixture mode

The offline fixture library answers `description` with `like_count`, and the
companion answers `POST /v1/subscriptions/import`, `GET /v1/dislikes/{id}` and
`translated_captions` there. It reports `invalid_catalog` for the `kids`
section and `unknown_operation` for `video_details`, `stream_reminder`,
`playlist_order`, `subscribe_notifications`, `comment_post` and
`comment_reply`, and ignores `notifications`, `first_playable` and
`new_content`.

## Guest live check of 2026-10-11

The Guest read paths were run once against YouTube from a companion bound to
loopback, with yt-dlp 2026.08.19, the pinned YouTube.js 18.1.0 and no
proof-of-origin token provider. No account, console or change on YouTube was
involved. `scripts/probe_live_guest.py` repeats the check against a running
companion (`--base`, `--code` or `--token-file`); it prints one pass/fail line
per area and no tokens or media addresses.

Working as specified: pairing, both searches with every option group and two
pages, `search_suggestions`, `first_playable`, the Music, Gaming, News, Sports,
Live, Movies and Kids sections, channel sections with orders and
`channel_search`, playlists with their positions, `playback_queue`,
`playback_at`, `playback_next`, `channel_window`, `group_window`,
`suggestions`, `video_details`, `description`, comments in both orders, live
chat in both modes, chat replay with seeks, `hl`/`gl`, `hide`, `thumbnails`,
SponsorBlock and DeArrow.

What the live service does differently from the offline assumptions:

- **Home is empty for Guest.** YouTube returns only a "start searching" notice
  to a signed-out session without history, on the web and the TV client. The
  action answers an empty page.
- **Trending and Shorts sections do not exist** for Guest (HTTP 400 upstream),
  reported as `catalog_unavailable`.
- **Shorts in search** arrive as grid shelves of Shorts tiles. They are video
  rows with `is_short`; a `type: "shorts"` search holds nothing else. The
  tile image comes from the tile's watch endpoint.
- **Replies** are opened through the continuation in the reply renderer's
  contents; the pinned thread parser looks elsewhere and failed on every
  thread. The first upstream reply page held ten comments.
- **Section shelves hold few rows.** A News, Sports or Live shelf lists five
  tiles and a continuation that returns nothing more, so `has_more` can be
  true before an empty last page.
- **Movies** are posters whose title sits in a header overlay; they open a
  purchase page upstream and do not resolve for Guest.
- **Related-video filter chips** are absent for Guest; `suggestions` returns
  the plain list.
- **Country.** `gl` changes search results; the TV section shelves kept
  following the network location of the companion.
- **Live chat custom emoji** arrive as their identifier text, not an image or
  a shortcut.

Media resolution without a token provider: the web clients offer no direct
addresses (YouTube forces its streaming protocol there), yet yt-dlp still
returned complete direct lists from its other clients. Through the relay, with
byte ranges anywhere in the file: H.264 up to 1080p60, VP9 and AV1 up to
2160p60, AAC audio, live HLS (H.264 up to 1080p60 with separate audio and a
DVR window), human-made and automatic captions, and storyboard sheets. HEVC
was offered for none of the tested videos, so `codec: "hevc"` fails with
`unsupported_media`. **Machine-translated captions are listed but their
download is refused** (HTTP 429 upstream, relayed as `upstream_media`): the
translation address needs a proof-of-origin token.
