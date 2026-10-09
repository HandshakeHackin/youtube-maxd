# Library service — integration in progress

The optional companion library worker uses MIT-licensed YouTube.js 18.1.0.
It adds home recommendations, account subscriptions/history/watch-later,
playlist browsing/edit operations, video ratings and comments to the service.
The native Discover screen opens home and account/library screens through the
existing asynchronous job API. Profiles and remote playlist folders preserve
their account context. Local favorites, queue and history remain console data.

This is not full SmartTube parity. Device-code login, live chat and comment posting
are not connected yet. Account
cookies are configured explicitly on the companion; no browser cookies are
read automatically. A configured profile is not proof that its session is valid.
Real YouTube/account acceptance remains unverified.

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
or a single Cookie-header value are supported for library operations. Use a
private host directory. Cookies remain on that host and never appear in profile
responses, console files, process command lines, logs or error diagnostics.
The existing playback extractor has its own `--cookies` option; per-profile
private-video playback still requires integration and must not be implied by
successful account browsing.

## API and failure contract

All operations require the ordinary companion bearer token.
`POST /v1/library` accepts a bounded action/profile/item request.
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
Library networking is disabled in explicit offline fixture mode.

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
