# Offline validation record — 0.1.0

## Retained-group filtering and metadata reuse — 2026-10-11

Ordinary group windows no longer transmit unrequested display names, avoiding
native rejection of long multibyte subtitles when name filtering is disabled.
When identity verification is required, a complete successful lookup updates
retained occurrences for reuse by later windows. Failed partial verification
commits nothing; metadata finishing after group retirement is rejected.

All 109 worker tests and the worker bundle pass. A new native ASan/UBSan suite
passes forward/reverse name and ID blocking, cross-window traversal, exact source
positions and missing/malformed metadata in retained browsing groups. The source
anchor remains separate from the destination target across window boundaries.
This batch changes service behavior and tests; native implementation bytes are
unchanged. Console and live-service acceptance remain pending.

## Cookie-profile browsing groups — 2026-10-11

Primary cookie-account catalogs now retain worker group contexts just like Guest
and OAuth catalogs. The adapter keeps catalog requests profile-owned and delegates
video resolution to the existing cookie-backed extractor. Profile invalidation
revokes both the adapter and the underlying resolver, preserving cancellation
and stale-reference rejection.

The focused catalog/profile regression run covers two independent cookie
accounts, opaque group propagation, credential-free catalog requests, correct
per-account video credentials and revocation of retained resolver references.
All 55 focused Python tests pass. No native bytes changed or
console connection occurred. Secondary-account/brand-channel video context,
real-account catalogs and PS5 acceptance remain open.

## Guest browsing-group routing — 2026-10-11

With the library worker enabled, Guest catalogs now use the same bounded opaque
continuation adapter as account catalogs. Worker search results retain source
groups through catalog conversion and native selection; public video resolution
continues using the existing Guest extractor. Guest credential access remains
rejected. A failed worker catalog does not silently switch to another source.

All 106 worker tests and the rebuilt bundle pass. Fifty-four Python tests covering
Guest catalogs, occurrence propagation, OAuth playback, profile partitions and
account removal pass. New cases cover repeated IDs across pages, group continuity,
cached-page reuse, independent public video resolution, missing-worker behavior,
and catalog failure. No native bytes changed and no console was contacted.
Real Guest search/continuation and long-session acceptance remain pending.

## Native browsing-group handoff — 2026-10-11

The provider now validates retained group keys, accepts source contexts, and
requests anchored `group_window` data. Manual directions preserve occurrence,
profile and selection ownership. Continuous EOF can advance through the group;
playlist-only completion excludes it. Failed forward windows can fall back to
next metadata. Group requests never use the remote playlist request format.
Catalog models/conversion retain supplied group contexts and repeated IDs.

Four native ASan/UBSan suites pass (group navigation, manual playlist navigation,
playlist windows and recommendations), with additional group cases for natural
EOF, playlist-only pause, malformed anchors and failed-window autoplay fallback. Sixty-three Python catalog,
OAuth playback and action/API checks pass. The PS5 provider object compiles with
the existing 262 SDK math-header qualifier warnings. Guest extractor group creation and real console behavior
remain unverified; no console connection was made.

## Browsing-group service groundwork — 2026-10-11

The pinned SmartTube source selects explicit queue entries first, then applicable
Mix continuation, then a following eligible item in the current browsing group,
then next-video metadata. Previous selects an eligible preceding group item before
the retained queue. This establishes source-group navigation as the next gap;
picking an arbitrary WatchNext card would not implement that behavior.

Worker catalog groups and `group_window` now retain occurrence positions and
share continuation work. The worker suite and rebuilt bundle pass 105 tests;
35 focused API/library tests pass. Cases exercise repeated IDs, upcoming/folder
skips, concurrent continuation reuse, failed-fetch retry, owner/anchor rejection,
expiry/eviction, and continued browsing after the retention bound. Native group
handoff, Guest extractor propagation and hardware acceptance remain pending.
No console connection or PS5 package build was performed in this service batch.

## Verified playback channel names — 2026-10-11

Playlist/channel windows and autoplay targets now provide verified author names
when profile-owned name blocks require them. Duplicate IDs reuse lookups without
changing source positions. Existing channel IDs must agree with fetched metadata;
invalid names are replaced only by verified names, and incomplete responses fail.
Native forward/reverse and cross-window navigation skip matching names, including
uncached entries, and recheck prepared selections at final handoff.

All 99 worker tests and the worker bundle pass. Thirty-four focused API/library
tests and four ASan/UBSan executables pass (blocked recommendations, ordinary
recommendations, playlist windows and blocked window navigation). Cases cover
name-only blocks with different channel IDs, case variants, missing names,
duplicate occurrences and conflicting identities. The PS5 provider object compiles
with the existing 262 SDK math-header qualifier warnings. No console connection was made. Alternative
recommendation selection and hardware acceptance remain open.

## Unicode channel-name blocks — 2026-10-11

Catalog and retained queue records now match blocked channel names with simple
Unicode uppercase-then-lowercase comparison. The pinned Unicode 15.0.0 table
supports non-ASCII and supplementary characters without locale-specific folding,
normalization or multi-character expansion. Name-only block entries persist and
can be removed through revision-owned management actions, preventing stale row
indices from targeting another entry after a removal.

Four ASan/UBSan suites pass (name matching/persistence, channel blocks, blocked
queue navigation and core provider), including Latin accents, Greek sigma,
Turkish I, Cyrillic, supplementary characters, malformed UTF-8, preserved local
queue membership and stale removal rejection. The real RmlUi host regression
also passes; the PS5 provider object compiles with existing SDK warnings.
Regeneration from the hash-pinned UCD input exactly matches the committed table;
native preparation includes an exact copy of the Unicode license in its assets.
Name propagation was completed in the later batch recorded above. Alternative
recommendations and hardware acceptance remain pending.

## Missing playlist-channel metadata — 2026-10-11

Playlist navigation requests channel verification when the current profile has
blocks. The worker fills missing identities on the same profile client, reuses
lookups for repeated IDs, preserves occurrence order and limits concurrency to
four. A failure stops new lookups and waits for already-issued calls to settle.
The provider rejects incomplete metadata while blocks are active and checks
prepared entries again before selection/handoff. Metadata-poor windows remain
subject to the normal worker timeout; failure does not bypass channel filtering.

All 95 worker tests and 30 Python tests pass; the worker bundle rebuilt. Four
native ASan/UBSan suites pass for blocked windows, window parsing/ownership,
manual navigation and next-video policy. Coverage includes duplicate lookup
reuse, profile ownership, bounded concurrency, identity mismatch, transport of
the verification flag and rejection of missing/malformed channel arrays.
The PS5 provider object compiles successfully with the existing SDK math-header
qualifier warnings.
Name matching, alternative recommendations and hardware acceptance remain open.

## Channel-block browser refresh — 2026-10-11

Successful local block/unblock actions close the action levels and refresh the
originating catalog or search. Search text, the parent folder and saved focus
position are retained through the existing browser restoration path. Unblocking
from the management list refreshes that list without leaving a replayable action
in the navigation stack. Failed/stale actions remain on their error screen.

The real generated RmlUi host regression passes search filtering/return, local
history return, management-list removal, focus availability and stale-action
handling, alongside its existing chat/reader/settings/playlist tests. Native
preparation and the PS5 RmlUi provider object compile successfully. Physical UI
acceptance remains pending; this is functional navigation work, not final polish.

## Recommendation channel verification — 2026-10-11

Profiles with active channel blocks request verification of the autoplay target's
channel through the same profile client. Native recommendation parsing and final
handoff reject blocked, missing or malformed identities. Profiles without blocks
avoid the additional lookup; another profile's blocks do not affect them.

All 93 worker tests and 28 Python tests pass, including the selected endpoint's
metadata identity, strict flag schema and direct/job transport. Four native
ASan/UBSan suites pass for recommendation blocking, next-video policy, playlist
windows and blocked windows. An additional regression confirms a block added
after a recommendation arrives prevents handoff. The worker bundle rebuilt and
the PS5 provider object compiles successfully (existing SDK math-header warnings).
Blocked/unverifiable recommendations stop rather than selecting an alternative;
alternative selection, unknown playlist-channel metadata, name matching, UI
refresh and hardware acceptance remain unfinished.

## Channel-aware playback windows — 2026-10-11

WatchNext playlist rows retain channel identities from author browse endpoints
alongside their exact occurrences. Channel windows identify their verified source
channel. Native parsing validates optional aligned channel arrays, and navigation
skips known blocked entries in either direction, including across a window edge.
Final playlist handoff rechecks the selected entry's channel.

All 92 worker tests pass and the worker bundle rebuilt. Four native ASan/UBSan
suites pass (blocked windows, playlist windows, manual navigation and next-video
policy), with an additional reverse-block traversal run. Tests cover duplicate
video/channel alignment, forward/reverse skips, boundary progress, malformed
channel arrays and existing ownership/cancellation behavior. The PS5 provider
object compiles with 262 SDK math-header qualifier warnings. Unknown-channel
resolution, recommendation filtering, name matching, visible-row refresh and
hardware acceptance remain pending.

## Blocked-channel queue navigation — 2026-10-11

Queue EOF selection, unavailable-item retry eligibility and manual queue
Previous/Next now exclude stable-ID blocked channels. The final queue handoff
also rejects an item blocked after it was selected. Entries remain queued, with
their resume position and watched state intact, so unblocking restores them.

Four ASan/UBSan suites pass: blocked queue navigation, channel-block persistence,
queue failure recovery and manual navigation across all seven playback modes.
The PS5 provider object compiles successfully with the existing SDK math-header
qualifier warnings.
This does not cover playlist/recommendation autoplay filtering or hardware
acceptance; those remain pending.

## Channel block-list foundation — 2026-10-11

Stable channel IDs can be blocked/unblocked from video actions and managed in a
profile-owned local list. Matching search/library and local rows are filtered
without destroying their saved history/favorite/queue state. Block actions are
idempotent, and profile/source epochs reject stale menu actions. The store is
bounded to 128 rows and rejects malformed/duplicate rows when loading.

The channel-block ASan/UBSan regression passes persistence/reload, profile
isolation, stale actions, repeated Block, continuation preservation, filtering
and restored favorite/queue visibility after Unblock. Core provider, local-action
and profile-setting ASan/UBSan regressions also pass. The PS5 provider object
compiles with the existing 262 SDK math-header qualifier warnings. SmartTube channel-name
matching, autoplay-window filtering and immediate visible-row refresh remain
unfinished; this is not full channel-block parity or hardware acceptance.

## Account mark-as-watched — 2026-10-11

The PS5 account video menu now confirms a Mark as watched action. The worker
obtains the selected video's finite duration and uses the pinned YouTube.js
history/tracking methods at that position. Both responses must succeed before a
matching result changes local watched state. Live/upcoming videos and invalid or
mismatched metadata are rejected before submission. Favorites and queue remain.

All 91 worker tests pass, including the actual pinned VideoInfo parser/tracking
methods, cached confirmation, account/video ownership, invalid metadata and
concurrent/uncertain submission. Twenty-four Python tests pass, including actual
worker termination/restart with confirmed-result retention and uncertain-intent
rejection by the supervisor. Four native ASan/UBSan suites pass for history actions,
core provider, local actions and profile settings. The PS5 provider object compiles
with 262 SDK math-header qualifier warnings. Real account state was not
modified; real-account acceptance is pending.

## Account-history removal — 2026-10-11

History catalogs now issue bounded profile/video/page-owned removal references.
The worker keeps feedback tokens private, submits through the pinned History
method with a one-page bound, caches confirmed results and rejects repeated
uncertain attempts. Continuation pages retain the proper removal feed.

All 89 worker tests pass, including the actual pinned Video/History parser-method
path and processed-feedback confirmation. Seventy Python transport/context tests
pass, including direct/job removal schema validation and preservation of the
account/video/reference tuple. The worker bundle rebuilt successfully. These
tests use fixtures; no account was modified.

The PS5 video menu now consumes transient history references through its account
confirmation flow. Four native ASan/UBSan suites pass: history removal, core
provider, local video actions and profile settings. Coverage includes cancellation,
incorrect confirmed video IDs, duplicate submissions, profile retirement,
non-persistence of references, and preservation of favorites/queue state.
The PS5 provider object compiles successfully against the PS5 SDK (253 existing
SDK math-header qualifier warnings). Full candidate packaging remains pending.
Real/localized history acceptance remains unfinished.

## Local video actions — 2026-10-11

Guest and account video menus expose Play next, Mark as watched locally, and
Remove from local history. Queue insertion preserves other pending entries'
relative order, avoids duplicate IDs and compacts numeric ranks. The current
playing item is not inserted again. Music origin survives menu navigation.
History edits preserve favorites/queue state and are explicitly local; account
tracking and removal use the separate confirmed account actions described above.

Menu action IDs carry the current profile/source epoch, and stale IDs after
profile changes or reload cannot mutate the newly selected profile. Six native
suites pass (video actions, base, manual navigation, queue failure, music and
profile settings), with an additional music-origin action/persistence check.
Active-playback insertion also passes the manual-navigation regression. Provider
and RmlUi provider objects compile with the PS5 SDK (existing math-header warnings).
The feature inventory now also records channel/manual navigation evidence and
the remaining remote-history gap. Physical menu acceptance remains pending.

## Channel playback-mode policy — 2026-10-11

Source review of pinned SmartTube `VideoLoaderController.initRandomNext` and
`applyPlaybackMode`, plus `SuggestionsController.findNextSectionVideoIfNeeded`,
confirms that active shuffle targets user playlists larger than 30 entries;
the random section helper is disabled. Channel windows therefore retain normal
forward order even when their finite size exceeds 30. Playlist-only now stops
after a channel video unless an explicit queued video is available. Manual
Previous/Next remains independent of this completion policy.

The channel provider regression includes a 120-position channel in shuffle mode,
playlist-only pause, and the explicit-queue exception. Existing playlist tests
retain the size-30 boundary, large indexed shuffle and failed-target fallback.
Channel context, playlist window, manual navigation and recommendation native
suites pass with sanitizers. The PS5 provider object compiles with the existing
SDK math-header warnings. No console connection or full candidate rebuild occurred.

## Manual next/previous video actions — 2026-10-11

The in-player picker now offers Previous video and Next video. Explicit actions
use group/queue navigation independently of all seven EOF modes; Next prioritizes
the pending queue, while Previous prioritizes the group and falls back to retained
queue order. Skipping a queued item consumes its pending entry without marking
it watched or clearing its resume point. Previous can return to that entry.
Explicit queue removal and Clear queue erase the retained navigation order, so
Previous cannot resurrect entries the user removed.
Group failure can consult next-video metadata once. Requests retain active
selection/profile ownership, and player input cancels pending work.

The actual generated player tick waits through seeks before stopping and returning
to the existing provider launch path. Phone-owned playback hides these controls
and cancels pending local navigation. Failed lookups leave the current video
playing and show one notice. This follows the pinned SmartTube controller's
separate explicit-next/previous path rather than applying EOF mode policy.

Native tests cover both directions across all seven modes, unchanged resume and
watched state, queue consumption/previous history, cancellation and stale replies.
Extracted shipping-player tests cover ready, busy, phone, failure and input paths.
Six native regression suites pass across this batch. Provider, PlayerScreen and
AudioTrackPickerScreen objects compile with the PS5 SDK; the final queue removal
change also passes the native base/queue/manual suites and provider recompilation.
Physical controller and real-service acceptance remain pending.

## Channel unavailable-tail completion — 2026-10-11

Directed channel scans now distinguish proven exhaustion from an unavailable
target. If every remaining slot is unavailable/upcoming, the worker returns
the finite size and skipped count with the original video/index anchor. Native
validation checks that anchor before accepting the exhaustion result, then
continuous mode can request next metadata. A stale anchor cannot trigger that
fallback. Reverse exhaustion stops at the beginning of the channel group.

The rebuilt worker passes 85 tests; native context, playlist-window and next
metadata suites pass, including actual EOF dispatch and stale-anchor rejection.
The PS5 provider object compiles with the existing SDK math-header warnings.
These are offline fixtures; console/live-feed acceptance remains outstanding.

## Anchored channel navigation — 2026-10-11

Dedicated channel-window requests now fetch the Videos feed using the selected
worker profile, verify the original video/index, and return bounded neighboring
or indexed occurrences. They preserve source holes and duplicate IDs, skip
upcoming/unavailable slots for directed requests and report finite size only
after feed exhaustion. Native results must echo the original channel anchor;
changed anchors are rejected before navigation. Forward and reverse completion
use the window, and continuous mode can request next metadata at a known end
without treating the channel key as a YouTube playlist ID.

Offline evidence: 84 worker tests and 63 Python context/transport tests pass;
native context, window and recommendation suites cover forward/reverse channel
completion, changed anchors, request serialization and finite-end fallback.
The final provider object compiles with the PS5 SDK, with the existing 253
qualified math-header warnings. The worker bundle was rebuilt successfully.
Deep scans remain subject to worker timeout. Physical channel browsing and
playback, large live feeds and manual navigation acceptance remain pending.

## Native channel occurrence handoff — 2026-10-11

Channel/index metadata now travels through the native occurrence handoff using
an explicit `channel:UC…` group key, retaining duplicate selections and recovery
positions without writing this transient context into history. The parser rejects
mixed playlist/channel context, mismatched source channels and channel keys falsely
presented as playlist IDs. Ordinary catalog requests now retain their parent;
previously that missing snapshot prevented source-context matching.

Playlist-window and recommendation requests reject channel groups until dedicated
channel navigation is implemented, so these keys cannot become YouTube playlist
IDs. Channel next/previous fetching remains incomplete. Native occurrence,
playlist-window and recommendation regression suites pass with sanitizers.
The provider object also compiles with the PS5 SDK (existing SDK math-header
qualifier warnings). No full candidate rebuild or console test was performed.

## Channel catalog context foundation — 2026-10-11

Signed-in channel browsing now selects the Videos tab, matching guest browsing.
Both service paths preserve channel/index context and repeated occurrences;
guest positions require explicit extractor indices, while the worker preserves
raw video positions across filtered rows, local pages and upstream continuations.
Malformed supplied context never falls back to an inferred position. Native
channel context handoff and channel next/previous navigation remain unfinished.

Validation: 78 worker tests and 28 catalog context tests pass, covering profile
isolation, cached continuation replay, source holes, duplicate IDs, guest/OAuth
adapters and direct/job HTTP responses. No console access was performed.

## Playlist-end and Mix next context — 2026-10-11

Next-video requests now optionally include the active playlist and occurrence.
The worker preserves validated target playlist/index metadata, allowing a repeated
video ID at a distinct occurrence while rejecting a self-reference to the same
occurrence. Native parsing validates this context before handoff. Continuous and
shuffle modes consult metadata after confirmed finite playlist exhaustion; Mix
window edges use metadata instead of unsupported index-only radio navigation.
Playlist-only accepts metadata targets only within the same playlist, including
an explicit wrap at a finite end. This follows
the pinned SmartTube SuggestionsController's use of next-media metadata after
queue/group selection; channel-group context remains incomplete.

All 77 worker tests and 60 targeted service/phone-playlist transport tests passed;
the production worker rebuilt. Logs: `.deps/playlist-next-worker.log` and
`.deps/playlist-next-python.log`. Real YouTube/PS5 acceptance remains pending.
The recommendation, playlist-window/context and queue-failure sanitizer batch
passed. After extending same-playlist constraints to finite-end wrap, the final
recommendation/window suites passed again, covering repeated IDs, Mix and finite
boundaries, accepted context handoff and cross-playlist rejection. Logs:
`.deps/playlist-next-native.log` and `.deps/playlist-next-final-native.log`.
Both PS5 provider-object builds passed with existing SDK warnings; final log:
`.deps/playlist-next-final-sdk.log`.
No console connections occurred.

## Continuous playback recommendations — 2026-10-11

The new authenticated direct/job `playback_next` operation reads the WEB autoplay
endpoint for the playing video and selected profile. It follows the pinned
YouTube.js VideoInfo policy; suggestion cards and next-button alternatives are
not substituted. Missing/self-referencing targets terminate normally. Outside
playlist context, continuous/shuffle/reverse modes request it after explicit
queues, while playlist-only, pause, close and repeat retain their own policy.
Native requests snapshot playback selection and scope; cancellation/profile
changes retire replies before HTTP errors can affect credentials. Handoff clears
playlist occurrence and resume state.

All 76 worker tests and 59 targeted service/phone-playlist tests passed, and the
production worker rebuilt (`.deps/playback-next-{worker,python}.log`). The new
native recommendation suite passed mode gating, terminal/malformed results,
cancellation, profile retirement and a late 401. The initial native fixture
omitted the required records array; it was corrected before playback testing.
The broader native regression also exposed a re-pairing edge: retired credentials
must not trigger background recommendation pairing. A token prerequisite now
prevents that request, and the provider regression passed with the guard.
The final recommendation, provider, SponsorBlock, playlist-window/context and
queue-failure sanitizer batch passed. The PS5 provider object compiled with
existing SDK warnings (`.deps/playback-next-{native,sdk}.log`).
Channel context, exhausted-playlist recommendation policy and real YouTube/PS5
acceptance remain pending. No console connections occurred.

## Unavailable-only finite playlist windows — 2026-10-11

Directional traversal now continues beyond an unavailable-only panel when its
source provides a validated finite playlist size. A response carries the proven
unavailable count and either the next index or explicit exhaustion. Native
continuation retains the active occurrence owner, creates one cancellable job
per step and requires monotonic progress within the finite index range. Invalid
step metadata, unobserved gaps, unknown finite bounds and radio/infinite metadata
stop the traversal. Shuffle remains exact-index lookup with its existing fallback.

All 73 worker tests and 58 targeted service/phone-playlist transport tests passed;
the production worker rebuilt. Logs: `.deps/playlist-scan-worker.log` and
`.deps/playlist-scan-python.log`. No console connections occurred.
Native window/context and queue-failure sanitizer suites passed, including two
successive unavailable windows, reverse scanning, finite exhaustion, cancellation,
non-progressing indices and invalid infinite/radio flags. The PS5 provider object
compiled successfully with existing SDK warnings. Logs:
`.deps/playlist-scan-native.log` and `.deps/playlist-scan-sdk.log`.

## Directional unavailable playlist slots — 2026-10-11

Forward/reverse window-edge lookup can cross consecutive explicitly unavailable
rows in a returned WatchNext panel. It requires upstream proof of the requested
occurrence and refuses gaps in source positions. Exact-index shuffle behavior
is unchanged. The service validates strict direction values and action scope;
native handoff validates requested index, bounded skip count, direction and the
confirmed target. Entirely unavailable windows and radio continuation remain open.

All 72 worker tests passed and the production worker rebuilt. All 58 targeted
service/phone-playlist transport tests passed, covering direct/job direction
forwarding and malformed input rejection. Logs:
`.deps/playlist-direction-worker.log` and `.deps/playlist-direction-python.log`.
Native window/context and queue-failure sanitizer suites passed, including forward
and reverse unavailable-slot handoff and inconsistent skip metadata rejection.
The PS5 provider object compiled with existing SDK warnings. Logs:
`.deps/playlist-direction-native.log` and `.deps/playlist-direction-sdk.log`.
No console connections occurred.

## SponsorBlock completion modes — 2026-10-11

An acknowledged final-segment skip now pauses the current frame and presents
explicit completion to the same player path as natural EOF. That path retains
phone queue ownership, waits for local playlist windows and dispatches the chosen
pause/repeat/close/advance policy. It avoids requesting a frame beyond the end.
Manual/repeat seek acceptance alone does not retire explicit completion: an idle
position within one second of the requested target is required. Failed seeks
retain the completion and pause; successful seeks cancel a pending playlist advance.

Final native SponsorBlock, playlist-window and profile suites passed, including
repeat after end skip and successful/failed seek observation. Extracted shipping
player paths passed paused end acknowledgement, subsequent advance/repeat, phone
ownership and pending playlist cases (`.deps/sponsor-modes-final-{native,player}.log`).
The seek handoff, generated-media demux, failed-seek clock, audio-only and video
discard regressions passed. PS5 provider, PlayerScreen and PlaybackController
objects compiled successfully; SDK qualified-math warnings remain. Logs:
`.deps/sponsor-modes-final-seek.log` and `.deps/sponsor-modes-final-sdk.log`.
Hardware acceptance remains pending; no console connections occurred.

## Natural EOF repeat, pause and player mode controls — 2026-10-11

All seven playback-mode choices are now present in browser settings and the
in-player settings picker. Repeat-one emits one seek-to-zero action after natural
drain, preserves the current queued item, and rearms only after a non-EOF idle
playback tick. A rejected seek pauses and displays a message without a retry loop.
Exhausted local continuation explicitly pauses; phone-owned completion is excluded.
The controller accepts Pause from an active Finished state. Completion actions
are scoped to the current profile/source generation and cleared on new playback.

The final native window, profile, queue-failure and SponsorBlock sanitizer batch
passed. Extracted shipping player tests passed repeat, pause, failed-seek,
single-dispatch, phone ownership and scrub/seek/discard/prebuffer gates. Logs:
`.deps/repeat-final-native.log` and `.deps/repeat-final-player.log`.
The PS5 provider, PlayerScreen, AudioTrackPickerScreen and PlaybackController
objects compiled successfully (`.deps/repeat-final-sdk.log`); existing SDK
qualified-math warnings remain. The first picker overlay attempt failed its exact
anchor check before compilation; the branch-chain anchor was corrected and the
final preparation and build passed.
These checks do not prove full mode parity: SponsorBlock-triggered completion,
channel/recommendation context, unavailable playlist slots, radio continuation and
console acceptance remain open. No console connections occurred.

## Explicit queue mode preference and close-at-end — 2026-10-11

The profile-owned "Queue respects playback mode" setting defaults off, as in
the pinned SmartTube PlayerTweaksData. Stop-at-end then continues an explicit
queue, including bounded unavailable-video skips; enabling the preference stops
that continuation. Changing the preference cancels a staged queue retry. Playlist
continuation remains disabled in stop-at-end mode. Close-at-end uses the same
queue exception; otherwise the existing drained-completion player path stops
playback and returns to the browser without staging a next video. Its choice is
persisted per profile and the close request is emitted once per completion.

Native provider, playlist-window, profile-settings, queue-failure and SponsorBlock
sanitizer suites passed. Cases cover preference reload/isolation, explicit queue
handoff without playlist scope, both stop policies and cancellation during an
unavailable-video retry. Actual RmlUi navigation also passed. Logs:
`.deps/queue-mode-native.log` and `.deps/queue-mode-ui.log`.
The initial preference batch also compiled the PS5 provider object successfully
with existing SDK warnings (`.deps/queue-mode-sdk.log`).
After adding close-at-end, the window, profile, queue-failure and SponsorBlock
sanitizer suites and actual RmlUi tests passed again. Close cases cover queue
override, respecting the mode with queued items retained, no queued items,
one-shot close and persisted selection (`.deps/queue-close-{native,ui}.log`).
The updated PS5 provider object also compiled successfully, with the same SDK
warnings (`.deps/queue-close-sdk.log`).
Repeat-one, explicit engine pause and in-player mode controls remain unfinished.
No console connections occurred.

## Playlist modes and profile preferences — 2026-10-11

The browser settings chooser now persists continuous, shuffle, playlist-only,
reverse and stop-at-end modes per profile. Legacy autoplay settings migrate to
continuous or stop. Reverse follows the preceding playlist occurrence before
the explicit queue, looks up a previous window edge, and stops at index zero.
Shuffle uses validated finite sizes greater than 30, excludes the current
occurrence, and falls back to normal continuation after a failed indexed lookup.
Unknown/small sizes retain normal continuation. Mode changes retire pending work.

Native window, profile-settings, queue-failure and music-context sanitizer tests
passed, including migration, profile isolation, reverse boundaries and shuffle
fallback. Actual RmlUi chooser navigation and native phone-provider tests passed.
PS5 provider and RmlUi provider objects compiled with existing SDK warnings.
Logs: `.deps/playback-modes-{native,ui,phone,sdk}.log`.

This remains partial mode parity: repeat, close, queue exceptions/preferences,
in-player selection and channel/recommendation context are still open. Continuous
and playlist-only currently share the implemented queue/playlist behavior.
SponsorBlock completion, unavailable slots and radio continuation also need
integration. No console connections occurred; no new full package was frozen.

## Finite playlist size — 2026-10-11

WatchNext windows now optionally expose validated `playlist_size` separately
from the bounded video list. Numeric counts and plain decimal source strings
are accepted only when positive, within the index limit, and greater than all
observed positions. Missing, localized, inconsistent, infinite/radio and
malformed metadata remains unknown. The native parser revalidates counts and
boolean flags. Natural EOF stops without an extra lookup at a confirmed final
occurrence; unknown sizes and partial windows retain indexed continuation.

All 70 worker tests and 48 service/phone-playlist transport tests passed; the
production worker rebuilt. The 500-entry/128-row worker case checks that size
does not collapse to window length. Logs: `.deps/playlist-size-worker.log` and
`.deps/playlist-size-python.log`.
Native window/context and queue-failure regressions passed; the final window
suite also covers strict flag validation, known-end suppression and continuation
when a known playlist extends beyond the returned window. PS5 provider-object
compilation passed with existing SDK qualified-math warnings. Native logs:
`.deps/playlist-size-native-base.log`, `.deps/playlist-size-native.log`, and
`.deps/playlist-size-sdk.log`.

Field reference: the author's [VLC playlist parser discussion](https://gist.github.com/Daniel-Mendes/21cff1d94b584769fa05a4905bd272b5#comments)
reads raw WEB `playlist.totalVideos`; this is historical format evidence,
not live acceptance of every current client response. Missing current metadata
continues to work as unknown. Shuffle policy, full playback modes and hardware
acceptance remain incomplete. No console connections occurred.

## Playlist window-edge continuation — 2026-10-11

Natural EOF now makes one index-only lookup for the immediately following
occurrence when the anchored WatchNext window ends at the current video. It
stages that confirmed target, rather than the item after it, and retains the
same playback owner across both requests. Repeated EOF ticks do not duplicate
the lookup. Failed/mismatched results stop the attempt; cancellation or a new
selection (including the same video ID) retires it immediately.

The native window fixture passed normal edge continuation, cancellation,
replacement and wrong-index cases; the queue-failure suite also passed. Full
playback modes, traversal across unavailable target slots, radio continuations, channel/recommendation
context and console acceptance remain open. No console connections occurred.
Retired lookup responses are rejected before HTTP error handling, so an old
401 cannot clear current credentials or replace the current error message.
The final window/context, recovery and queue-failure sanitizer batch passed,
including that stale-401 regression. The phone-provider suite and PS5 provider
object compilation also passed; existing SDK qualified-math warnings remain.
Logs: `.deps/playlist-edge-{native,phone,sdk}.log`. This is not yet a complete
frozen application candidate.

## Index-only playlist lookup — 2026-10-11

The `playback_at` direct/job operation requests WatchNext with playlist and
index, without inventing a video ID. Selected playable identity and explicit
upstream index evidence are required; ambiguous selections, missing position
proof, conflicting indices and infinite/radio panels fail. Native indexed
requests retain the currently playing occurrence as owner while validating the
different target. Natural-EOF autoplay rejects unrelated retargeted windows;
the window-edge follow-up permits its specifically requested next occurrence.

The service library/context/window selection passed 50 Python tests and all 68
worker tests passed; the production worker rebuilt. Logs:
`.deps/playback-at-python.log` and `.deps/playback-at-worker.log`.
This provides navigation needed by playback modes; it does not implement their
selectors or shuffle policy. The pinned SmartTube `VideoLoaderController`
shuffles when playlist size is greater than 30, requests a random index and
normally excludes the current index (`Utils.getRandomIndex`). Reliable playlist
size and full mode integration remain necessary. No console connections occurred.
The native window/context and queue-failure sanitizer suites passed, including
different-video target lookup, zero index, request bounds, mismatched target
rejection and unchanged active occurrence. The PS5 provider object compiled
successfully with existing SDK qualified-math warnings. Logs:
`.deps/playback-at-native.log` and `.deps/playback-at-sdk.log`.

## Natural EOF playlist advancement — 2026-10-11

The existing autoplay preference now advances to the next returned playlist
occurrence after natural presentation drain when no explicit queue entry is
available. The player waits for its scoped WatchNext job, then stages one
selection with the original sparse index and zero resume position. Browser
return skips its usual reload while this selection is pending, preserving the
handoff. Controller input cancels the wait; profile changes and phone selection
invalidate its ownership. The existing phone-owned EOF gate remains intact.

Native playlist-window/context, queue-failure and SponsorBlock suites passed.
The expanded native fixture covers queue priority, one-shot playlist handoff,
controller-side cancellation API and profile retirement while waiting. Actual
generated player input/tick, recovery handoff and phone failure host tests pass.
This is the first continuous-playlist path, not the seven-mode policy: reverse,
shuffle, repeat, pause/close, channel/recommendation context and SponsorBlock-induced playlist completion still
need integration. Final packaging and console acceptance remain pending.
The native phone-provider regression and PS5 compilation of the provider,
PlayerScreen and ProviderHostScreen passed. Logs are
`.deps/playlist-eof-{player,recovery,phone-host,phone,sdk}.log`; existing SDK
qualified-math warnings remain. No console connections were made.

## Native playlist window transport — 2026-10-11

The native provider can request and retain a bounded WatchNext window for the
active occurrence. Requests use the existing asynchronous jobs/polling path;
responses require matching playlist, video, occurrence, selection and source/
profile scope. Validation rejects oversized/empty windows, invalid IDs, missing
anchors, malformed sparse positions, index coercion and inconsistent starts.
Dense and sparse windows retain repeats. Cancellation retires retained data;
late retired completions cannot replace a newer ready window.

The playlist-window, playlist-context, recovery and queue-failure ASan/UBSan
suites passed, along with the native phone-provider suite. The expanded window
fixture additionally passed the late-old-response/new-ready-window ordering and
128-entry boundary. Logs: `.deps/native-playlist-window.log` and
`.deps/native-playlist-window-phone.log`. The natural-EOF follow-up above connects
the first consumer; complete playback-mode policy remains pending. No console
connections occurred.
The PS5 SDK provider object compiled successfully
(`.deps/native-playlist-window-sdk.log`), with existing SDK qualified-math
warnings. This change is not yet a complete frozen application candidate.

## Playback window service — 2026-10-11

Added authenticated direct and asynchronous `playback_queue` requests using the
existing profile-owned WatchNext adapter. HTTP validation requires an exact
video/playlist/occurrence, rejecting missing, boolean, fractional, string and
out-of-range indices before invoking the worker. The shared adapter now rejects
conflicting endpoint indices and conflicting selected occurrences for both
native and phone consumers. Bounded windows retain duplicate IDs and sparse
positions; they do not imply a full playlist or terminal boundary.

The 39-case Python library/context/window selection and all 64 worker tests
passed; the production worker rebuilt. Logs: `.deps/playback-window-python.log`
and `.deps/playback-window-worker.log`. Native EOF consumption, seven playback
modes and final-candidate/hardware acceptance remain incomplete. No console
connections were made.
The additional companion, OAuth playback and Lounge regression selection passed
99 tests (`.deps/playback-window-regression.log`).

## Playlist occurrence context — 2026-10-11

Playlist browsing preserves repeated video IDs and original sparse positions.
Guest extraction converts yt-dlp's one-based indices once. The pinned YouTube.js
classic/modern parsers retain validated playlist endpoints through local pages
and upstream continuations; Watch Later and OAuth catalog routes do likewise.
Mismatched identities, invalid positions and cross-profile cursors cannot supply
an occurrence. HTTP catalog and asynchronous job responses retain the context.

The initial Python selection passed 78 cases; the added HTTP/job case then
passed with the complete 17-case context suite (79 distinct Python cases across
the batch). All 62 worker tests passed and the production worker rebuilt.
Commands: pytest for `test_playlist_context`, `test_companion`,
`test_oauth_playback`, `test_library`; Node `companion/innertube/test.mjs` and
`build.mjs`.

Native rows now carry occurrence identity through Rml focus/click, the selection
mailbox, pending playback and an immutable resolution snapshot. Repeated IDs
use distinct UI keys. Recovery retains the occurrence; cancellation consumes
staged selection and a new source/profile epoch invalidates old selections.
Canonical history/queue records remain free of incidental browsing context.
All 18 native ASan/UBSan suites passed, including the new occurrence regression;
the actual shipping RML/production callback test selected the second repeated
video and returned its original index. Full seven-mode EOF behavior, channel
context and console acceptance remain pending. No console access occurred.
Generated recovery and phone-failure host checks and the native phone-provider
suite also passed. PS5 SDK compilation passed for `tube_provider.o`,
`evo_rmlui_provider.o` and `ProviderHostScreen.o`; SDK qualified-math warnings
remain. Logs are `.deps/playlist-{native,ui,recovery,phone-host,phone-provider,sdk}.log`.
This batch has not been built into a complete frozen application package.


## Compiled display transforms and packaged codec probes — 2026-10-11

The native AMDVLK/LLPC build completed and its shipping NV12 gfx1013 probe passed.
The complete 75-pipeline rotation/mirror set compiled with EVO's packager and
passed target, relocation, resource-layout and metadata checks. Immutable artifact
key: `307cf282cfc0dd60168bbf883cb18fdd7408b17f7ce44cb999aa57761b2962d2`.

Native preparation activated every header and the matching runtime; post-install
hashes and activation metadata matched. The actual activated source passed all
2,640 orientation/aspect/pixel/resize combinations, every zoom and frame-snapshot
consistency under ASan/UBSan. The isolated runtime compile and shipping
`media/src/evo_agc_runtime.o` compile both passed with the PS5 SDK. Evidence:
`.deps/shader-compiler-build.log`, `.deps/video-transform-compile.log`, and
`.deps/transform-active*.log`; all corresponding result files are 0. This is
compiled and connected offline evidence, not GPU presentation acceptance or a
final native package. No console connection occurred.

The reusable `scripts/probe_companion_package.py` also passed using committed
companion source `30e106ae02c18335f809f2de9d55ab4a812d8582` and real archived
media in a fresh extraction directory. AV1/libdav1d and HEVC each decoded 240
video frames with audio, sought to four seconds for 59 frames, and matched the
original's first sought-frame hash. No regeneration occurs during those probes.
Evidence: `.deps/companion-package-smoke.log`, result 0. The parity inventory now
records the existing offline sizing/time/pixel evidence and explicitly retains
the missing seven-mode EOF/playlist/queue behavior.


## Packaging completeness — 2026-10-11

The companion fixture allowlist now includes AV1 and HEVC media. Three release
selection tests pass, including every demo catalog item's media/captions and
exclusion of dirty source, private files and redirected fixture symlinks.

Application packaging now requires the verified transform artifact before native
preparation. Its compiler command emits only the artifact path on stdout; progress
uses stderr. Explicit artifact selection retains the existing preflight. The
runtime compile check also accepts already-activated sources without reapplying
the overlay. Fourteen artifact cases and five subtests pass; shell syntax and the
staged runtime's actual PS5 SDK compilation pass. Full compilation, activation
and application packaging remain pending the live compiler bootstrap. This does
not constitute GPU or final-candidate acceptance. A real `build_ps5.sh` rejection
probe also verifies that a missing compiler or invalid explicit artifact stops
before creating the native output tree. No console access occurred.


## SDR HEVC selection and relay — 2026-10-11

HEVC is selectable per profile and participates in automatic selection after
AVC/VP9 and before software AV1 at equal resolution. DASH, progressive and split
live-HLS mapping retain codec identity; DRM, HDR, unsupported transports and
resolution/frame-rate restrictions still apply. Existing persisted codec values
keep their meanings. The package guard requires FFmpeg's HEVC decoder and
MP4-to-Annex-B converter, both present in the installed PS5 archive.

Validation: 124 resolver/quality/fixture checks; all seventeen native sanitizer
cases, including codec requests/reload/wraparound and HEVC profile isolation;
PS5 provider compilation and archive symbol checks. The generated HEVC/AAC
fixture passed real loopback companion DASH/relay decoding (240 frames), seek
to four seconds (59 frames), and exact first-sought-frame hash agreement with
the original. Logs: `.deps/hevc-{service,native,relay,sdk}.log`.

Commands: selected pytest files `test_hevc`, `test_av1`, `test_quality_limits`,
`test_companion`, `test_fixture_library`; `bash scripts/test_native.sh`;
`PYTHONPATH=companion .venv/bin/python scripts/probe_hevc.py`.
The SDK retained its existing 226 qualified-math warnings. No console connection
was made. HEVC console presentation/performance, HDR and real-service HEVC
availability remain unverified; this batch is not a frozen final candidate.


## Activated-path geometry coverage — 2026-10-11

The geometry regression now extracts both the current runtime and the staged
transform runtime, including its actual per-frame snapshot statements. Both
pass under ASan/UBSan. The transform path checks all 2,640 combinations of
eight orientations, eleven aspects, six pixel corrections and five resize
modes, plus every supported zoom for every orientation.

A frame-consistency case changes provider state after capture and proves that
sizing keeps the captured orientation without another provider read. The next
capture retires the transform and returns to the physical display dimensions.
The existing controller, picker and paused HUD-label cases also pass.
Command: `bash scripts/test_video_geometry.sh`, exit 0.

These are extracted host-code checks, not PS5 shader execution. The toolchain
continues compiling; complete shader packaging and actual application activation
remain pending. No console connection was made.


## Verified transform activation and ABI reset — 2026-10-11

Native preparation can now activate the staged renderer only with an explicitly
selected shader artifact. Preflight matches freshly generated sources and the
packager, then checks all headers and metadata before changing native outputs.
The installer preserves unrelated UI shaders and refuses pinned EVO or a
different generated source pin. An activation stamp is written only after
successful complete preparation.

Fourteen synthetic artifact/install cases pass. Real preparation in a dedicated
cache also passes rejection-before-mutation and disabled-reset checks: a bad
artifact preserves the existing shader/stamp, while a successful disabled run
restores the pinned NV12 header, original runtime ABI and removes the stamp.
Evidence: `.deps/transform-prepare.log`, result 0. These tests do not constitute
real transformed-shader activation or GPU acceptance.

Source inspection found that spvgen's shared library has a separate build target
and the name spvgen.so. The bootstrap recipe is corrected, with shell syntax
checked. The live older bootstrap continues compiling LLVM; its cached follow-up
and the full transformed shader build/probe remain pending. No console traffic.


## Rotation and mirror preferences — 2026-10-11

SmartTube's four rotation angles (0/90/180/270) and horizontal mirror now have
profile-owned native state, bounded persistence, settings-browser controls,
in-player options and HUD labels. They compose with existing resize, zoom,
aspect and device-wide pixel correction. Editing another profile preserves
the current video's packed orientation; device correction changes only its
pixel-ratio bits. Mirror actions execute only on page zero.

All sixteen native sanitizer regressions pass, including invalid/corrupt values,
full preference tables, profile reload/isolation, all four choices and action
pagination. Extracted picker and paused-label checks pass, including the
post-toggle mirror label. Actual RmlUi navigation reaches the rotation submenu.
Evidence is in `.deps/rotation-{native,render,ui}.log`.
The final actual-RmlUi check also toggles mirroring on/off, preserves the menu
stack and confirms paging cannot toggle it again (`.deps/rotation-ui-toggle.log`,
result 0). Provider, controller, both HUDs, browser and picker PS5 objects
compile, as does the staged transform runtime
(`.deps/rotation-{sdk,runtime}.log`). The complete orchestration exited 0;
existing SDK math/RmlUi fixture warnings remain.

End-to-end rotation remains unfinished: the PS5 shader compiler is still
building, the full transformed shader set is queued, and the staged runtime
has not been activated in the application. No package or console test is
claimed for these controls.


## Display-transform runtime and packaging preparation — 2026-10-11

The staged AGC overlay now captures one atomic geometry snapshot per frame and
uses its logical canvas consistently for video sizing and both postprocessing
planners. Intermediate passes receive orientation zero. Final passes receive
the preserved UV/interpolation constants plus orientation at byte 32 and the
matching physical viewport. Direct video uses byte 28 of its existing constant
block. This overlay compiles successfully in isolation with the PS5 SDK.

The labeled-image sanitizer/GLSL/SPIR-V checks also pass with checks for scratch
versus scanout uniforms and disabled geometry. Nine artifact-integrity tests
pass, including stale sources, missing/changed headers, wrong target/type,
relocations and source-hash mismatches. Evidence:
`.deps/video-transform-{stage,runtime,artifact}.log`; the orchestration exited 0.
Artifact fixtures verify packaging gates, not GPU code generation.

The complete transformed shader build is queued behind the running compiler
bootstrap. It publishes a hash-keyed artifact only after every real PS5 pipeline
passes EVO's metadata checks. Application activation, persisted controls,
full package and physical acceptance are still pending. No console was contacted.


## Rotation/mirroring preparation — 2026-10-11

Pinned EVO's video, upscale and interpolation generators now have an isolated
staging helper for a shared clockwise quarter-turn/mirror vertex transform.
Generated fragment and pipeline metadata sections remain identical to pinned
EVO. Direct-video uniform layout uses the existing padding at byte 28; the
postprocessing layout adds orientation at byte 32, preserving its existing
interpolation parameters.

The staged GLSL function and native final-viewport helper pass ASan/UBSan against
labeled-image rotation/mirror results for full frames and offset subrectangles.
All three vertex families compile with glslang, pass spirv-val and have their
uniform offsets checked in disassembly. Evidence:
`.deps/video-transform-stage.log` and `.deps/video-transform-stage.result` (0).
The initial GLSL extraction included the .pipe VsInfo metadata; correcting the
test to stop at VsInfo allowed actual vertex compilation.

This is preparation, not a completed playback feature: PS5 LLPC compilation is
still running, runtime and preference/UI wiring remain pending, and no new
candidate or console acceptance is claimed. The isolated compiler probe must
pass before these shaders enter the application.


## Aspect overrides and display pixel correction — 2026-10-11

The pinned SmartTube baseline exposes eleven aspect choices in AppDialogUtil,
with float values from PlayerConstants, and six enabled pixel-ratio choices in
PlayerSettingsPresenter. ProsperoTube uses those values, including upstream's
rounded aspect constants. The shared PS5 AGC transform multiplies the selected
(or source) aspect by the display correction before applying resize/zoom;
Stretch continues to fill both axes.

Aspect selection persists per profile. Pixel correction persists once for the
device and applies across profiles. Changing it while a different profile owns
playback changes only the active pixel bits, preserving that video's aspect,
resize and zoom. Render/start workers continue to use atomic packed snapshots.
Both settings surfaces expose the options; both HUDs show the combined sizing.

All sixteen native sanitizer regressions pass, including exact menu labels,
profile/device persistence, full preference-table handling, corrupt defaults
and active-picture isolation. Extracted shipping AGC tests cover all 330
aspect/pixel/resize combinations, combined percentage zoom, invalid indices,
paused labels and both picker categories. Existing time-display checks and
actual RmlUi selection/paging also pass. The expanded root menu moved history
to page two; its test now follows pagination instead of assuming page zero.
Evidence: `.deps/aspect-{native,render,time,ui}.log`.

PS5 provider, shared AGC runtime, controller, both HUDs, browser and picker
objects compile successfully (`.deps/aspect-sdk.log`; existing SDK math warnings).
The complete check script exited 0. No console connection or new package freeze
has been made. Physical display geometry and upscaler presentation await the
final candidate. Rotation remains separate work.


## Video sizing and percentage zoom — 2026-10-11

The pinned SmartTube `createVideoZoomCategory` defines five resize modes and 55
percentages (50–95 by 5, 96–100 by 1, 105–300 by 5). All sixty choices now exist
in profile-owned settings and the in-player picker. Fit width/height, Fit,
Fill and Stretch feed the existing PS5 AGC video-quad transform; percentage
zoom scales Fit around the center. This shared blitter serves native and
FFmpeg frames. Upscaling/interpolation receive the resulting scale through
their existing path; no CPU frame rescale or PS4 dependency was introduced.

An atomic packed geometry snapshot separates render-thread reads from mutable
profile preferences. Start/stop activate/retire the transform; another profile's
settings do not change the active picture. Saved sizing returns on subsequent
playback. Both HUDs label the active transform, including paused RmlUi updates.
Triangle uses the saved basic mode and returns percentage/fixed-axis sizing to
Fit before resuming its Fit/Fill/Stretch cycle.

Validation:
- All sixteen native ASan/UBSan regressions pass, including all sixty menu
  choices, three-page traversal, invalid values, profile reload, full preference
  table handling and active-picture isolation across profile changes.
- A final native run compares every paged percentage ID to an independent
  literal list from the pinned SmartTube ranges; it passes
  (`.deps/video-geometry-native-options-final.log`).
- The extracted shipping AGC function passes all percentage scales with
  portrait/wide sources, concrete fixed-axis/crop/stretch cases and invalid
  inputs. Extracted paused-label, picker and actual controller methods pass;
  the previous time-display regression also passes.
- Actual RmlUi selection scrolls to the chosen row and reaches 300% through
  all three pages. The original test attempted to activate a non-visible row;
  its corrected interaction follows the real viewport behavior.
- PS5 compilation passes for provider, AGC runtime, playback controller, both
  HUDs, provider UI and audio/options picker. Existing SDK math and fixture RmlUi
  width warnings remain. Ignored evidence is in `.deps/video-geometry-native.log`
  and `.deps/video-geometry-{render,time,ui,sdk}-final.log`; the final orchestration
  result is `.deps/video-geometry-final.result` (`0`).

No full package freeze or console connection was made. GPU presentation,
upscaling/interpolation interactions and long-session acceptance await the final
candidate. Aspect-ratio overrides and rotation remain separate parity work.

## Playback time display and bounded settings pages — 2026-10-11

The pinned SmartTube `appendEndingTimeCategory` offers disabled extra timing,
remaining time and ending time; `onSetEndingTimeLabel` adjusts estimates for
playback speed. ProsperoTube now offers total duration, remaining time (default)
and estimated finish time, with per-profile persistence and settings/in-player
selectors. Both shipping HUDs share the same bounded formatter and use actual
audio playback rate plus the displayed seek-preview position. Finish time uses
the platform local clock, refreshes ahead of RmlUi's unchanged-state early return
while paused, and only writes changed labels. Live and unknown-duration inputs
show LIVE and --:-- respectively. Invalid clocks fall back to remaining time.

The new root setting exposed the old 24-element menu buffer limit under ASan.
Local menu construction now uses a checked 64-element buffer, while callbacks
retain 24-item paging. Fetching later settings pages cannot repeat their action.
The last root item remains reachable through real RmlUi pagination.

Validation:
- All fifteen native provider ASan/UBSan regressions pass. New cases cover the
  selector, rejected values, profile isolation, reload, legacy/corrupt defaults,
  two-page settings traversal and no repeated autoplay toggle on page two.
- The shared formatter and extracted shipping HUI/RML/picker blocks pass
  ASan/UBSan: speed correction, fractional countdown rounding, midnight wrap,
  paused wall-clock refresh, no redundant text writes, scrub targets, live,
  unknown/non-finite/bounded inputs and small output buffers.
- Actual RmlUi tests pass for the new submenu and the final settings-page item.
  PS5 compilation passes for provider, RmlUi app/provider, HUI OSD and audio
  options picker. Existing SDK math warnings and fixture RmlUi width warnings
  remain.
- Ignored evidence: `.deps/time-display-{native,labels,ui,sdk}.log` and
  `.deps/time-display.result` (`0`). The initial sanitizer failures were fixed,
  not waived. No full package freeze or console connection was made.

PS5 presentation, local timezone and clock acceptance remain pending with the
final candidate. This is one completed offline settings section, not complete
SmartTube settings or localization parity.

## Recovery retains quality preferences and frame rate — 2026-10-11

The native provider snapshots resolution, frame-rate and codec preferences when
starting resolution and carries that snapshot through the returned choice into
recovery. Changing preferences after resolution returns no longer alters a
later automatic reopen; subsequent ordinary resolution uses the new settings.
The generated EVO choice structure now carries FPS, parsed from the companion,
and native recovery requires exact known/unknown and fractional-rate matching
alongside the existing dimensions/codecs/audio checks. In particular, 29.97 fps
cannot silently become 30/60 fps or unknown during recovery.

Validation:
- All fifteen provider ASan/UBSan regressions pass, including changed preferences
  between resolve completion and selection, three bounded recovery attempts,
  exact fractional-rate matching, rejected mismatched/unknown/NaN replacements,
  and new preferences on the next ordinary resolution.
- 35 existing VOD/live renewal tests pass; service renewal already includes FPS
  in its immutable representation identity and rejects changed formats.
- Extracted shipping callback/pthread/completion tests pass after updating their
  harness dependencies for previously introduced unavailable-queue branches.
  The first regression fixture intentionally changed settings during an active
  resolution and triggered the provider's cancellation guard; the final fixture
  exercises the post-resolution/pre-selection boundary without bypassing it.
- PS5 provider, provider manager, launch host, playback controller and provider
  UI compile. Existing SDK/compiler warnings remain. Ignored evidence:
  `.deps/recovery-quality-native.log`, `.deps/recovery-quality-service.log`,
  `.deps/recovery-quality-host-final.log`, `.deps/recovery-quality-sdk-final.log`
  and `.deps/recovery-quality-followup.result` (`0`).

The added field is an overlay of the generated provider ABI; pinned EVO sources
are unchanged. The full candidate must rebuild its provider consumers together.
No new full package freeze, console connection or hardware acceptance is claimed.

## Resolution and frame-rate preferences — 2026-10-11

The maximum-quality cycle is replaced by an explicit eight-choice submenu:
144, 240, 360, 480, 720, 1080, 1440 and 2160 lines. A separate per-profile
frame-rate menu provides Automatic, up to 30 fps and up to 60 fps. Both apply
to subsequent resolution requests. Existing profiles default to Automatic.
The companion request accepts strict integer `max_fps` from 0 through 60;
0 disables this constraint. Explicit limits reject formats without a known
positive rate and formats above the limit before transport selection. Choices
now return `fps` and display the known rate in their existing native label.
Resolution/frame-rate constraints remain combined with codec, SDR and transport
eligibility; this is not yet complete SmartTube video-preset parity.

Validation:
- 92 service tests passed across the quality, AV1 and companion suites. After
  adding two transport cases, the expanded 37-case quality suite passed,
  covering eight heights, all three codecs, fractional rates, unknown/excessive
  rates, invalid inputs, progressive MP4 and live HLS (94 distinct service
  cases across these runs).
- Native provider and profile-settings ASan/UBSan suites pass, including actual
  resolve JSON, all eight selectors, malformed actions, profile isolation and
  persistence/reload. Actual RmlUi navigation confirms both submenus push and
  retain their folder level through selection.
- PS5 SDK compilation of provider and provider UI passes; existing SDK math
  warnings remain. Ignored evidence: `.deps/quality-limits-{service,formats,
  native,ui,sdk}.log`, `.deps/quality-limits.result` and
  `.deps/quality-limits-ui.result` (both `0`). No new package freeze or console
  playback/performance acceptance is claimed.

## Phone natural-EOF command priority — 2026-10-11

Automatic advance to an already-loaded phone playlist item now checks for queued
Stop, replacement playlist and disconnect before scheduling. The sequence-free
launch remains interruptible during resolution and native selection binding.
Retired attempts cannot bind or acknowledge success; intervening volume/status
commands retain their original order. Explicit user-requested launches retain
the existing ordered-command behavior.

Validation:
- Real receiver/phone adapter ASan/UBSan suite passes, including twelve new
  combinations: three interrupting commands before EOF, before taking the next
  launch, during resolution, and after binding but before successful completion.
  Replacement playback retains its requested position and occurrence. Ordinary
  volume/query traffic does not cancel automatic advance.
- The same regression against pre-change `b2d92f0` fails because the unwanted
  next launch remains pending. This establishes the behavioral regression.
- Extracted shipping phone failure/wait/input checks pass. PS5 SDK compilation
  of `tube_phone`, `ProviderHostScreen` and `PlayerScreen` passes (one existing
  dangling-else warning in the host screen).
- Ignored evidence: `.deps/eof-interruption-{native,host,sdk,before}.log` and
  `.deps/eof-interruption.result` (`0`). No console connection, package freeze
  or physical-phone acceptance is claimed for this batch.

## AV1 SDR software decode integration — 2026-10-11

EVO's pinned PS5 recipe already builds libdav1d and registers FFmpeg's libdav1d
AV1 decoder. Its generic software backend opens the selected codec through
avcodec_find_decoder; this batch connects the missing companion selection and
native profile preference. AVC, VP9 and AV1 SDR now map to progressive, separate
track DASH and paired live-HLS paths. At equal resolution Automatic keeps AVC
before VP9 before AV1. Explicit AV1 restricts selection to AV1. HDR, DRM,
unsupported transports and the selected height bound remain filtered. AV1 uses
software decode; no claim of PS5 real-time performance follows from this change.

Validation: 105 companion tests pass, including AV1 MP4/WebM codec metadata,
refresh bindings, progressive/HLS mapping, preference ordering, and exclusions.
All fifteen provider sanitizer regressions pass; actual native resolve requests
carry codec=av1 and profile persistence/reload retains the preference. The
provider and software decoder PS5 object targets pass; the installed PS5
libavcodec archive defines ff_libdav1d_decoder. Packaging now refuses an archive
missing that implementation (`.deps/av1-native.result`, exit 0).

The offline probe generates redistributable AV1/AAC media, starts a loopback-only
real fixture companion, resolves it and reads its DASH/relay URLs with libdav1d.
It decodes 240 frames, decodes 59 frames after a four-second seek, and hashes the
first sought frame against the local original, with equality. The server shuts
down in the probe's finally path (`.deps/av1-probe-final.log`, terminal exit 0).
Run with `PYTHONPATH=companion .venv/bin/python scripts/probe_av1.py`; host FFmpeg
needs libaom-av1 for fixture generation and libdav1d for decoding.

No console connection or new full package was made. Representative PS5 AV1
bitrate/frame-rate, memory, rendering, sync, speed/recovery and long-session
acceptance remain required at the final-candidate stage. The console is shared
with another agent; its previous exclusive reservation no longer applies.

## Per-profile seek intervals — 2026-10-11

The pinned SmartTube AppDialogUtil exposes intervals of 1, 2, 3, 5, 7, 10, 15,
20, 30 and 60 seconds. These choices now persist per profile in the PS5 settings
browser and in-player options picker. Missing/legacy preferences retain ten
seconds; unsupported, corrupt and oversized values cannot become active settings.
The shipping PlayerScreen uses the controller's interval for both directions.
Held seek retains EVO's acceleration curve, scaled by the selected interval;
non-YouTube sources keep their original ten-second base. Existing confirmation,
pause ownership, duration clamps and live-source restrictions remain in force.

All fifteen provider sanitizer regressions pass, covering the interval choices,
invalid values, distinct profile values, persistence/reload, legacy/corrupt-row
fallback and full preference-table behavior. Actual RmlUi chooser navigation
passes. Shipping controller/FSM tests exercise every interval, both tap directions
and held acceleration, with other-source fallback. The affected provider,
PlaybackController, PlayerScreen, AudioTrackPickerScreen and RmlUi sources compile
with the PS5 SDK (`.deps/seek-interval.result`, exit 0; extended tap extraction in
`.deps/seek-interval-controller-final.log`). Existing SDK header/UI warnings remain.
No new full package or console acceptance is claimed.

## Historical replay continuation paging — 2026-10-11

The stationary replay reader no longer discards dense replay intervals merely
because they contain more than 256 messages. An opt-in, owner-scoped page index
consumes pending replay events in order, keeping each response within both the
message and byte budgets. Further upstream continuations are fetched on demand,
with at most four requests per operation and the existing 4,096-action/1 MiB
pending-buffer guard. Future events stay withheld until the selected time reaches
them. Pages are read in sequence; seeks (including a page-zero restart at the same
time) reset the timeline. Repeated requests return the same delivered page, and a
failed fetch leaves the prior cursor, messages and pending events unchanged.
This is bounded access to upstream replay history, not an unbounded local archive.

The PS5 reader sends the requested page and validates the returned page number,
time, scope and message bounds. Triangle loads the next page; L1 / R1 seek by one
minute and reset to page zero. Pending requests block further page actions. The
footer/empty view advertise these controls. Existing live chat and synchronized
player chat retain their rolling behavior. Older companion responses remain
readable but do not advertise page navigation.

Evidence: 58 worker tests pass, including 700 messages in one dense interval,
1,500 across five upstream continuations, exact sequence/no loss, byte bounds,
repeated requests, same-time restart, short seeks, profile ownership and staged
failure recovery. All fifteen shipping provider sanitizer regressions pass;
actual RmlUi tests exercise Triangle, pending/terminal page behavior, time reset,
existing live refresh/focus/scroll, and literal message rendering. The provider,
RmlUi host and ProviderHostScreen compile with the PS5 SDK; existing header/UI
warnings remain (`.deps/chat-history-native.result`, exit 0). The updated RML was
rechecked with the built UI executable (`.deps/chat-history-ui-final.log`).
Service/API results are retained in `.deps/chat-history-{worker,api-final}.log`.
No console traffic, full package or hardware acceptance is claimed. Real YouTube
history continuity and long sessions still need acceptance on the final candidate.

## Natural-EOF phone playlist window continuation — 2026-10-11

Drained phone playback at a loaded-window boundary now retains the exact global
occurrence and its final position/duration while requesting a bounded refill.
The existing PlayerScreen completion guard still requires presentation drain and
no pending seek/scrub before this path runs; it stops the decoder and returns to
the provider browser. A matching successor starts at zero without the unavailable
video's five-second delay. The previous thirty-second bound, terminal response,
scope/cancellation rules and sparse/duplicate occurrence checks are shared.
A still-open finished selection does not cancel the wait; replay/seek, a new
selection (including the same video ID), or unrelated media does.

The service distinguishes a playing boundary probe from its later ended state.
It permits one ended-state recheck and records that state to prevent reload loops.
Validation: 74 remote service/playlist tests and real receiver/phone ASan/UBSan
checks passed, including a delayed wire refill after actual startup acknowledgment,
finished-clock feedback, immediate successor launch, cancellation, timeout, same-ID
selection replacement and failed monotonic clock. The test fixture's first delayed
reply used acknowledgment zero after consuming command one; the real receiver
correctly rejected it. The corrected acknowledgment-one fixture passes.
Phone adapter, generated PlayerScreen and Application compiled with the PS5 SDK
(`.deps/phone-eof-refill.result`, exit 0). Existing header/unused-variable warnings
remain. No console connection or new full-package claim; hardware acceptance and
broader restricted-video classification remain open.

## Unavailable phone window continuation — 2026-10-11

When the last supplied occurrence is unavailable, the adapter retains its
receiver/profile, playlist, click token and global index for up to thirty seconds.
It reports an ended occurrence with no bound media or fabricated playback clock.
Matching refill windows advance strictly beyond that occurrence, preserve sparse
indices and duplicate IDs, and retain the original five-second minimum delay.
A matching window without a successor terminates immediately; no response times
out. Stop/replacement/disconnect, user cancellation, scope changes and new local
playback retire the wait. Late or mismatched refills cannot restart it. Saved
local queue/history records remain untouched.

The service can refill a large supplied playlist using this ended anchor without
claiming previous/next media is playing. An unchanged loader result is returned
as an explicit terminal window for ended feedback; checked-index ownership still
prevents a reload loop. Active queue updates now require the existing phone-owned
selection and playlist/click scope, preventing late refills from adopting unrelated
local playback.

Validation: 73 remote service/playlist tests passed; the real native receiver and
phone adapter passed ASan/UBSan, including duplicate sparse continuation, minimum
delay, timeout, terminal response, cancellation, profile/local-playback retirement,
Stop with intervening volume, wrong click token, ended feedback and stale tickets.
Extracted shipping host blocks passed for all three unavailable dispositions and
any-button cancellation. The phone adapter and ProviderHostScreen compiled with
the PS5 SDK (`.deps/phone-refill-final.result`, exit 0). The host retains existing
SDK/header warnings. No new full package or console acceptance is claimed. Normal
successful-EOF window-boundary continuation and broader restricted-video error
classification remain separate gaps.

## Offline inactivity sleep timer — 2026-10-10

The pinned SmartTube VideoStateController pauses after configured inactivity and
restarts its timer on input/new media. ProsperoTube now implements that behavior
with saved half-hour choices from disabled through ten hours in both provider
settings and the in-player options menu. The actual main
loop checks expiry after input handling, pauses once, preserves media/queue and
shows a notice. Startup/close work is gated. Physical controller and deliberate
phone commands restart inactivity; query/status traffic and automatic queue
refills do not. Elapsed runtime is reset after application restart, not persisted.

Shipping timer sanitizer tests cover choices, persistence, exact expiry, reset,
disabled/idle state, once-only firing, new media and clock regression. Actual
phone adapter tests cover activity classification. Provider/phone/Application
compile with the PS5 SDK (`.deps/sleep-timer-batch.result`, exit 0). The complete
provider regression batch passes all twelve executables, and the committed
standalone timer test passes (`.deps/offline-provider-final.result`, exit 0).
Actual generated chat/HUD option tests remain passing alongside the new timer
rows; connected provider/phone/Application/picker compilation passes
(`.deps/sleep-timer-connected-final.result`, exit 0). Candidate `8b41ff55`
built and froze successfully: 126 files, eboot SHA-256 prefix `cadba35dfb6a47b3`
(`.deps/offline-sleep-timer-ps5-build.result` and
`.deps/offline-sleep-timer-freeze.result`, exit 0).
PS5 menu/pause and system screensaver behavior remain unverified. No console
connection.

Replay candidate `a9a6f582` built and froze successfully: 126 files, eboot SHA-256
prefix `5c7306c9307786cb`. It was not deployed. The replay implementation remains
retained-window browsing; unlimited historical-message pagination is still open.

## Offline replay reader — 2026-10-10

The browser now opens chat replay from top/newest comments with Triangle. L1/R1
seek by one minute while D-pad browses the retained message snapshot. The reader
supplies an explicit fractional timeline position, remains stationary between
seeks, reuses the owned cursor for ended rewinds and preserves position through
mode changes. A seek cannot race an in-flight update, and a response for another
time is rejected. Native replay composition is rejected before opening a prompt.
This does not implement unlimited historical-message pagination.

The new shipping-provider sanitizer case and existing player/sidebar tests pass.
Existing native chat/compose regressions and the actual generated RmlUi reader
pass, including opening replay, forward/backward time controls and rejected rapid
seeks; connected provider, provider-screen and RML objects compile with the PS5
SDK. The final reader also passes mode-change position retention and the updated
shipping RML (`.deps/replay-reader-ui-final.result`, exit 0). All 55 worker and
82 companion chat/receiver regressions passed (`.deps/offline-service-final.result`,
exit 0). Evidence: `.deps/replay-reader-batch.result` and
`.deps/replay-reader-final.result` (both exit 0). The earlier UI fixture crash was
an empty navigation-stack access after setup reset, corrected in the fixture;
no console run occurred. Packaging and hardware acceptance remain open.

The prior recovery/sparse candidate `bc3ff150` built and froze: 216 objects, 126
files, eboot SHA-256 prefix `443a3fa6532b45a9`. Build/freeze exited 0. All 55
InnerTube worker tests passed after the sparse-window change. No deployment.

## Caption recovery and sparse queue batch — 2026-10-10

Candidate `879b1281` completed PS5 packaging (216 objects, 126 frozen files).
Its bounded console case passed paused secondary selection/render/clear and
retention across the 1.25x decoder reopen. The scanout includes the loading
indicator; moving playback at that rate is outside this case. Exact teardown and
original state restoration passed. See `HARDWARE_ACCEPTANCE.md`.

Fresh-stream recovery now retains both chosen caption sidecars only for the same
validated source/token/profile/video recovery. Missing, empty or no-longer-listed
sidecars are discarded. The generated controller restores both delays and the
primary timing ratio only after successful current-context startup. Provider
caption and generated recovery handoff tests and PS5 provider/screen compilation
passed; all 11 provider regression executables also reported passes. The missing-sidecar case also passes. Sparse queue validation passed six
InnerTube adapter tests, 70 companion tests, actual receiver/player sanitizer
checks (including invalid maps, duplicate occurrences, refill and EOF), and
connected PS5 provider, receiver, phone adapter and provider-screen compilation
(`.deps/sparse-native-batch.result` and `.deps/sparse-player-final.result`, exit 0).
Full packaging is the remaining offline gate for this batch.

No further console connection is authorized until the user reopens testing.
Fresh-stream caption recovery and sparse queue hardware acceptance remain open.
## Provider selection ownership for secondary captions — 2026-10-10

The corrected `63d45c23` console case also stopped after French selection: the
engine-derived SECONDARY tag never appeared. Startup/pairing and paused native
playback passed; secondary application and the speed-reopen scenario remain
unaccepted. Exact owned teardown, healthy services and original provider/sidecar
restoration passed; the fixture stopped, the lock was released and no relaunch
followed. Evidence: `.deps/console/secondary-caption-63d45c23-hardware/`,
`.deps/secondary-corrected-hardware.result` (exit 1).

Offline tracing found a concrete incompatibility: PlaybackController deliberately
clears the legacy filesystem path for provider URLs, while the new secondary
loader required that path. Host tests had populated a local path and missed this.
The buffer now binds to a validated source/profile/video selection serial for
YouTube, retaining path ownership for local media. Empty unscoped media, retired
profiles and same-ID new selections cannot inherit that buffer. No signed media
URL is copied into the legacy filesystem state.

Actual engine tests now reproduce the empty provider path and pass 110 checks;
the 98-check application/startup bridge also uses an empty provider path, and
provider scope/staging tests pass. Full packaging and hardware acceptance of this
fix remain open.
Connected PS5 provider, Application, picker and subtitle-engine objects also
compile (`.deps/secondary-batch.result`, exit 0; engine/provider logs now include
the selection-owned fix). No new console run followed the failed case.

## Secondary-caption console observation gate — 2026-10-10

Candidate `225b5858` passed full installed-byte verification, 130-second settling
and full readback, native startup/first frame, UI pairing and paused AVC/AAC
playback. Its primary-only scanout visibly shows the English fixture caption.
The case focused French and sent Square, then stopped because its expected
secondary-caption toast was absent from the UI snapshot. Secondary application
and appearance are unproven. Exact owned GPU/process/video/mount release, healthy
services and original provider/sidecar restoration passed. The fixture stopped
and the shared lock was released; no relaunch followed. Evidence:
`.deps/console/secondary-caption-225b5858-hardware/`,
`.deps/secondary-hardware.result` (exit 1).

The native widget already draws PRIMARY/SECONDARY tags, but its developer focus
snapshot previously omitted them; the case incorrectly depended on a toast.
The overlay now exposes the existing tag in focused text, retaining the original
focusable labels. The corrected case waits for the engine-derived SECONDARY tag
and its removal, then captures both player states for visual inspection. This
observer change does not establish hardware acceptance.
The actual generated snapshot function passes compiled ASan/UBSan tag/focus
checks (`scripts/test_caption_picker_snapshot.py`), and the modified native
widget compiles with the PS5 SDK (`.deps/caption-picker-snapshot.result`, exit 0).

## Caption preservation across decoder reopens — 2026-10-10

The connected startup bridge reloads the provider-scoped second-language sidecar
after decoder teardown. A failed reload is reported while retaining primary cues.
Speed reopens now preserve primary delay, primary timing ratio and secondary delay
alongside the existing source, tracks, pause and position. The extracted shipping
speed-reopen test and actual subtitle application/startup bridge pass; the latter
now covers 98 checks (`.deps/secondary-reopen-{speed,engine}.log`). The provider
staging tests and connected PS5 provider/PlaybackController compilation also pass
(`.deps/secondary-reopen.result`, exit 0). Full packaging and PS5 reopen
acceptance of this follow-up remain open.
The combined reopen/snapshot candidate `63d45c23` completed the full PS5 build
and immutable freeze: 216 objects, 126 files, build
`prosperotube-63d45c23_1011-0235`, eboot SHA-256 prefix `1b315b42e399a9d8`
(`.deps/secondary-reopen-{ps5-build,freeze}.*`, exit 0).

## Independent secondary YouTube captions — 2026-10-10

The YouTube subtitle picker now uses Square to choose or clear a second language.
Primary and secondary downloads stage in separate alternating sidecars; validated
source/profile/selection ownership and the existing bounded request lifecycle apply
to both. Metadata changes only after actual engine application. A failed secondary
download/parse preserves both active lines. Taking the secondary language as the
primary clears its duplicate designation.

Pinned EVO's bounded SRT reader now accepts a destination cue buffer, preserving
the original primary loader. A separately allocated secondary buffer is published
only after successful parsing. It renders against the media clock with its own
delay, survives primary-caption changes, and retires on media teardown. The actual
generated engine passes 104 ASan/UBSan checks, including repeated replacements,
failed files, primary preservation, delay, forward seek/rewind and media identity.
The actual primary/secondary application bridge passes 92 checks. The provider
staging/scope tests also pass. Evidence: `scripts/test_secondary_captions.sh`,
`scripts/test_subtitles.sh`, `scripts/test_caption_switch.sh` and
`.deps/secondary-{engine,primary,provider}-tests.log` (the expanded 104-check engine
case also passed directly). Full packaging and PS5 secondary-caption visual/input
acceptance remain open.
The final connected PS5 Application/provider/picker/subtitle objects, all 11
provider regressions, caption-identity tests and shipping controller/checked-exit
tests passed (`.deps/secondary-final.result`, exit 0). Candidate `225b5858` completed
the full PS5 build and immutable freeze: 216 objects, 126 files, build
`prosperotube-225b5858_1011-0221`, eboot SHA-256 prefix `88be5396c1b84f33`
(`.deps/secondary-{ps5-build,freeze}.*`, exit 0).

## Whole-app close on phone disconnect — 2026-10-10

The saved preference cycles keep playing / stop phone playback / close the app,
retaining legacy stop choices. Close follows SmartTube's whole-app semantics,
including browsing; retired receiver commands cannot trigger it. Successful phone
launches bind the provider selection before acknowledgment, so an immediate manual
reopen of the same ID does not inherit remote queue/stop ownership.

The connected Application retires media, voice, captions, provider/auth operations
and the receiver before render park. It calls the ProsperoLight-proven PS5 self-exit
API once, gated by checked GPU fence retirement. Busy or missing fences retain
outstanding expectations and cannot publish a safe-close checkpoint; refusal
leaves the app parked. Real GPU drain and Application park/exit blocks pass compiled
ASan/UBSan tests (`scripts/test_phone_exit.py`). The shipping controller dispatch
test also passes. All 11 provider regressions and actual phone/receiver sanitizer
checks pass, and connected PS5 Application/provider/player/AGC objects compile
(`.deps/phone-exit-connected-{prepare,ps5-objects}.*`, exit 0). An initial repeated
prepare exposed the new Application header missing from the pinned reset list;
the header is now reset, and the subsequent complete object check passes.
Candidate `9340ab5a` completed the full PS5 build and immutable freeze: 216
objects, 126 files, build `prosperotube-9340ab5a_1011-0159`, eboot SHA-256 prefix
`e6581d19263cf41d` (`.deps/phone-exit-{ps5-build,freeze}.*`, exit 0).
The single bounded PS5 case passed installed-byte verification, 130-second
settling and full readback, native startup/first frame, UI pairing and saved close
preference, phone-command AVC/AAC playback and pause. Receiver disconnect reached
checked GPU retirement and `sceSystemServiceLoadExec("exit", nullptr)` without a
host close payload. Exact process/video-owner/mount release, healthy declared
services and byte-for-byte original provider/sidecar restoration passed. Evidence:
`.deps/console/phone-exit-9340ab5a-hardware/` and `.deps/phone-exit-hardware.*`
(exit 0). The owned fixture stopped and the shared lock was released. This
establishes native fixture self-exit; real-phone disconnect, browsing-only close
and physical motion/sound remain unassessed by this case.

## Partial phone queue expansion — 2026-10-10

The expansion source `c1d9d299` completed the full PS5 build and immutable freeze:
216 objects, 126 files, eboot SHA-256 prefix `6e62d3601814320a`
(`.deps/remote-expansion-{ps5-build,freeze}.*`, exit 0). This package remains
undeployed; it does not add console acceptance to the host service probes.

The companion now connects confirmed Lounge playback to the selected profile's
WatchNext library client. Raw WEB/TV panel parsing retains occurrence indices and
duplicates in bounded native windows. Infinite Radio/Mix responses may reset
service indices; these are rebased and merged with preceding phone queue items.
Stale results after replacement, stop, receiver failure or changed player identity
are discarded; unavailable expansion preserves the supplied queue. Owned tasks
are canceled and joined at shutdown, and failures back off without failing the
receiver. Non-Guest PS5 registration supplies the selected profile explicitly;
Guest registration preserves its previous request shape.

The batch passed 54 InnerTube tests, 125 companion tests, actual native
receiver/player sanitizer checks and connected PS5 object compilation. The
shipping worker bundle was rebuilt. A real Guest Mix returned 25 IDs initially
and a second window at global index 20; current ID and occurrence mapping were
checked through the shipping JSON-lines worker. A first probe exposed deep video
menus, now excluded from panel traversal; a second exposed Radio index resets,
now covered by explicit tests. Evidence: `.deps/remote-expansion-python.*`,
`.deps/remote-playlist-public.json`, `.deps/phone-window-tests.*`,
`.deps/voice-capability-connected-ps5-objects.*`. Real private/TV playlists,
inaccessible/sparse entries and PS5 expansion acceptance remain open.

## Local speech-recognition service — 2026-10-10

The native PS5 microphone worker passed ASan/UBSan lifecycle tests and strict PS5
SDK object compilation (`.deps/microphone-tests.result` and
`.deps/microphone-ps5-object.result`, exit 0). Tests exercise minimum/maximum
recordings, exact AudioIn routing/format, user/open/input/close failures, silence,
in-flight cancellation, short and valid push-to-talk finish, PCM wiping, repeated
polling, busy admission and shutdown/restart. AudioIn calls occur on the owned
worker. This proves offline capture lifecycle, not actual DualSense input or the
connected voice-search feature; the worker is not packaged yet.

The native capture/upload/job pipeline also passed ASan/UBSan fault tests and
strict PS5 SDK object compilation (`.deps/voice-driver-tests.*` and
`.deps/voice-driver-ps5-object.*`, exit 0). Coverage includes explicit little-endian
base64 and padding, repeat polling, result consumption, UTF-8 transcripts and
invalid/oversized queries, capture/request failure, recording bounds, deadlines,
profile/credential changes, phone-owner retirement, stale create/poll callbacks,
request-pool saturation and owned-job cleanup with original credentials. This
pipeline still requires packaging and native UI/search integration; no new
console acceptance is claimed.

Connected voice controls compiled with the actual PS5 Application,
ProviderHostScreen, provider, phone adapter, capture and transcription objects
(`.deps/voice-connected-ps5-objects.*`, exit 0). Local hold/release L3, Circle
cancellation, phone voice initiation/cancellation, playback pause, source/profile
retirement before network callbacks, screen-exit retirement and the existing
search handler are connected in the overlay. Phone adapter, transcription driver
and capture sanitizer tests passed (`.deps/voice-{phone,driver-connected,
microphone-connected}-tests.*`, exit 0). Full packaging and physical input remain
open for this object check.

All ten shipping provider regressions passed after the search/scope changes
(`.deps/voice-provider-regressions.*`, exit 0). The new case verifies exact paired
source/token/profile copying, no partial credential output, rejection after a
source change, and a 200-code-point/four-byte UTF-8 query reaching the actual
companion request without truncation. Existing library, SponsorBlock, DeArrow,
recovery, speed, Music, discovery and chat paths retain their passes.

Candidate `578ad430` then built and froze with 216 objects and 126 files,
build `prosperotube-578ad430_1011-0105`, eboot prefix `2cc2432456e43ba7`
(`.deps/voice-controls-{ps5-build,freeze}.*`, exit 0). The converter included the
native `libSceAudioIn` imports; no import stub is packaged. This candidate has
local and phone command handling, but predates availability-gated microphone
advertisement. It has not been deployed or tested with a physical microphone.

The availability/capability batch passed 140 companion regressions
(`.deps/voice-capability-service-tests.*`, exit 0), native voice lifecycle and
phone tests (`.deps/voice-capability-{driver,phone,microphone}-tests.*`, exit 0),
and connected PS5 object compilation (`.deps/voice-capability-connected-ps5-objects.*`,
exit 0). Tests cover health probing/retry/refresh, stale health results after scope
changes, old-client/disabled-ASR behavior, both capability gates, actual Lounge
bind parameters, strict booleans, idempotent binds, identity conflicts, receiver
retirement/rebind and preservation of explicit retry after failure. The final
failed-receiver edit is covered by the phone test. The full availability-gated
package completed all 216 object builds and froze 126 files as
`prosperotube-0b06bc66_1011-0111`, eboot SHA-256 prefix `4b3fd861c59b9048`
(`.deps/voice-capability-{ps5-build,freeze}.*`, exit 0). Hardware acceptance is
tracked separately.

Large phone-supplied queues now retain up to 10,000 IDs on the companion and
deliver bounded 128-ID windows to the native player. Confirmed player feedback
refills windows, preserving global indices and duplicate-video occurrences
without reopening playback. Replacement, stop and receiver retirement clear
retained queues. The batch passed 142 companion checks, native remote/player
ASan/UBSan tests and compilation of connected PS5 objects
(`.deps/phone-window-{service-tests,tests}.*` and
`.deps/voice-capability-connected-ps5-objects.*`, exit 0). Partial-queue service
expansion and hardware acceptance of window refills remain open.

The window batch then completed the full PS5 build and immutable freeze:
source `c2e8f032`, 216 objects, 126 files, eboot SHA-256 prefix
`a636ecbced4f1ecd` (`.deps/phone-window-{ps5-build,freeze}.*`, exit 0).
The controller routing check now includes the actual voice input block and
verifies local start/release/cancel, unavailable admission, chat precedence,
keyboard/browser ownership and screenshot routing under ASan/UBSan
(`scripts/test_controller_dispatch.py`, exit 0).

The `0b06bc66` console case established availability-gated startup/UI/capability
negotiation, then stopped on a malformed test playlist packet. Capture was not
attempted. Exact teardown and original data restoration passed; see
`docs/HARDWARE_ACCEPTANCE.md` for the failed fixture classification and wrapper
exit-code limitation. No launch retry occurred.

The authenticated voice upload/job/poll/delete API now invokes a locally configured
whisper.cpp executable and GGML model. It validates 0.5–10 seconds of 16 kHz mono
S16 PCM, permits one transcription at a time, enforces bounded streaming uploads
and a recognition deadline, and retires owned processes and temporary files on
failure, timeout, cancellation, expiry and shutdown. Audio validation errors are
bounded and do not echo the recording. Existing API bodies retain their 8 KiB cap.

Voice/companion/job-capacity/receiver/device-auth/profile-removal regressions passed
134 tests (`.deps/voice-service-tests.*`, exit 0), including real subprocess
termination, silence/bounds, repeated results and cleanup. The pinned whisper.cpp
v1.9.5 host build and setup/model hash verification passed
(`.deps/voice-whisper-build.*`, `.deps/voice-setup-verified.*`, exit 0). Real CPU
recognition of eight seconds from upstream's public speech sample passed through
the authenticated API with repeated polling and deletion
(`.deps/voice-local-probe.*`, exit 0). See [configuration and contract](VOICE_SEARCH.md).
PS5 microphone capture and native search integration remain open, and microphone
capability is not advertised. No console voice acceptance is claimed.

## Caption language selection and playback ownership — 2026-10-10

The actual subtitle picker and phone adapter share a scoped, asynchronous caption
switch. Up to 256 validated tracks retain human/automatic identity. Downloads use
alternating inactive SRT files; only successful parsing and selection by EVO's
subtitle engine updates the current language. The bridge preserves pause, clock
and timing adjustments without reopening media. File, transport, timeout and
scope failures preserve the previous track, and late callbacks cannot overwrite
a new attempt. Manual reopening of the same ID retires a pending switch.

Shipping provider and phone adapter ASan/UBSan tests passed
(`.deps/caption-switch-tests.*`, exit 0). The real subtitle engine passed 83 checks
including successful French selection and failed-track rollback
(`.deps/caption-switch-engine-tests.*`, exit 0); 44 companion tests passed,
including catalogs beyond 32 tracks and human-track precedence
(`.deps/caption-switch-companion-tests.*`, exit 0). Connected provider/driver/adapter,
Application, PlayerScreen and SubtitlePickerScreen compiled with the pinned PS5 SDK
(`.deps/caption-switch-ps5-objects.*`, exit 0). All nine provider regressions and
PS5 pairing/storage sanitizer tests passed after the final scope edits
(`.deps/caption-switch-native-regression.*` and
`.deps/caption-switch-phone-provider.*`, exit 0). Candidate `b1adb57c` built and
froze successfully: 214 objects, 126 files, build
`prosperotube-b1adb57c_1011-0024`, eboot prefix `1af06866881f2a9d`
(`.deps/caption-selection-{ps5-build,freeze}.*`, exit 0). The isolated fixture
resolved English/French tracks and served valid converted SRT. Both console cases
stopped at the initial running-title check, before any upload, binding or launch
(`.deps/caption-phone-hardware.*` and `.deps/caption-phone-fallback-hardware.*`).
A fresh idle-console case passed native pairing and video startup, paused
French/English/off/restore caption selection without moving the paused clock,
play/pause, fractional seek, volume/mute, next/previous, queue replacement, EOF
advance, stop, both disconnect preferences and saved local queue preservation
(`.deps/caption-phone-available-hardware.*`, exit 0). Exact owned GPU/process/title
release and service health passed; original provider and caption sidecar bytes
were restored. The owned fixture server was stopped and the shared lock released.
Real phone casting, physical sound/caption appearance and secondary YouTube
caption selection remain open.

Remote playback ownership survives receiver retirement so natural EOF cannot
consume a saved local queue item. Manual reopening of the same video starts a new
logical selection and releases that ownership. Recovery retains the selection.
Phone tests cover retired-receiver EOF, one-time completion and same-ID reopening.

- 2026-10-10 | casting | 0b02a137 | PS5 | failed: unsupported screenshot NLST after pairing/remote start; exact release and original data restore passed | .deps/phone-player-rng-* | use supported MLSD

The actual UI now generates and durably saves the PS5 pairing identity and screen;
the fixture receiver registered and acknowledged native playback at 13.25 seconds
with playlist index 5 and next-item feedback. Rendering telemetry continued.
The adapter's screenshot listing failed before the remaining commands. There was
no relaunch. Bounded cleanup verified the exact owned PID/app ID, GPU park,
video/title/mount release, healthy services and byte-exact original state
restoration (`.deps/phone-player-rng-cleanup.*`, exit 0). The adapter has been
corrected offline to use the protocol's supported MLSD listing. Real phone and
remaining command acceptance are still open; see [hardware acceptance](HARDWARE_ACCEPTANCE.md).

## Downloaded caption identity — 2026-10-10

Phone feedback and current-track toggles now use the language captured when the
caption was fetched, rather than the preference for the next video. Changing
that preference cannot relabel the playing track. New resolve, failed download
and provider restart retire the identity. Shipping provider ASan/UBSan tests pass
(`scripts/test_caption_identity.sh`, `.deps/caption-identity-tests.*`, exit 0),
and the provider compiled with the pinned PS5 SDK
(`.deps/caption-identity-ps5-object.*`, exit 0). This correction is not in frozen
runtime candidate `0b02a137`; full caption-language switching remains open.

## PS5 pairing entropy correction — 2026-10-10

- 2026-10-10 | casting | 79ceee4d | PS5 | failed: Link a phone SIGSYS during identity generation; OS released owned app, original data restored | `.deps/phone-player-pairing-hardware.*` | checked PS5 RNG

The revised case enabled pairing through the actual UI after full candidate byte
verification and settling. SIGSYS stopped the owned app before receiver registration
or playback. The saved debug ELF maps the caller to `remote_configure`; its identity
generation calls `arc4random_buf`. No launch retry followed. Captured OS logs confirm
owned app/video/mount release. A separate idle-first cleanup verified services and
restored the exact original provider bytes (`.deps/phone-player-crash-restoration.*`,
exit 0).

Identity generation now uses ProsperoLight's PS5 `kern.rng_pseudo` sysctl pattern,
checks returned lengths and wipes output on failure. Failed entropy cannot advertise
an identity or retry every frame; explicit pairing retry can recover. The PS5 branch
is exercised by provider tests, with short/oversized/error/block-boundary tests under
ASan/UBSan (`.deps/phone-rng-tests.*`, exit 0). Connected PS5 objects compiled
(`.deps/phone-rng-ps5-objects.*`, exit 0). Lounge no longer advertises microphone
support before capture/search exists; 40 backend tests passed
(`.deps/phone-player-capabilities-tests.*`). The corrected `f0b9f41c` PS5 package
built and froze: 126 files, build `prosperotube-f0b9f41c_1010-2345`, eboot prefix
`6a956afa089dc830` (`.deps/phone-pairing-rng-{ps5-build,freeze}.*`, exit 0).
Remote/companion regressions passed 83 tests (`.deps/phone-rng-companion-regression.*`).
Corrected runtime and real-phone acceptance remain pending.

The next batch adds a saved default-continue / stop-on-disconnect preference to
the actual phone settings and Application adapter. Stop only applies to the current
phone-owned video, clears its ephemeral queue and uses normal player/browser
teardown; unrelated local playback is preserved. Explicit remote disconnect events
trigger the choice; profile changes and network errors are not treated as commands.
ASan/UBSan tests cover default continuation, opted-in stop, queue retirement,
unrelated media and preference restart persistence (`.deps/phone-disconnect-tests.*`,
exit 0). This adapts SmartTube's optional finish-on-disconnect behavior to stopping
PS5 playback; automatic app exit is not implemented. All nine native provider
regressions passed (`.deps/phone-disconnect-native-regression.*`, exit 0).
Candidate `0b02a137` built/froze successfully: 126 files, build
`prosperotube-0b02a137_1010-2349`, eboot prefix `94333ca3a1e86586`
(`.deps/phone-disconnect-{ps5-build,freeze}.*`, exit 0). Runtime acceptance remains
pending at that offline stage; neither corrected candidate had yet been installed
or launched. Subsequent `0b02a137` fixture evidence is recorded above.

## Phone player and ephemeral queue adapter — 2026-10-10

- 2026-10-10 | casting | c2503cbb | PS5 | failed: rendering, owned release/state restoration passed; fixture never enabled pairing | `.deps/phone-player-hardware.*` | test UI pairing first

Candidate `c2503cbb` built/froze successfully (126 files,
`prosperotube-c2503cbb_1010-2314`; `.deps/phone-player-{ps5-build,freeze}.*`).
The bounded console case verified all bytes again after 130 seconds, rendered
with Lapy storage ready, then stopped on the missing initial command ack. Its
fixture had enabled=true but no valid identity, which initialization deliberately
ignores. No receiver registered or video started. Exact GPU/app release, healthy
services and original provider-state restoration passed. This is a fixture
correction, not evidence of working phone playback.

Follow-up source checks cover repeated video occurrences at an explicit playlist
index and retiring changed UI profiles before resolver callbacks. Shipping
adapter/provider tests and connected PS5 objects passed:
`.deps/phone-player-duplicate-tests.*`,
`.deps/phone-player-corrected-provider-tests.*`, and
`.deps/phone-player-corrected-scope-ps5-objects.*`. Revised hardware must enable
pairing through the actual UI before injecting commands.

The native adapter connects typed Lounge commands to the shipping controller,
provider resolver/fallback worker and PCM output. It preserves fractional offsets,
acks only successful startup, rejects stale tickets, and keeps remote queues out
of saved local state. Queue windows retain index/ctt, queue updates do not reopen
playback, and proven EOF advances remotely without removing local queue items.
Seeks use the existing checked admission path and preserve pause. Main/audio
volume is atomic and applies S16 gain without changing clock/drain accounting.

`scripts/test_phone_player.sh` passed ASan/UBSan with the shipping adapter, real
driver and cJSON: initial device/query commands, deferred start ack, pause, seek,
volume, next/previous, queue update, global index, EOF, D-pad, captions, stop,
retired profiles/tickets, failed startup/seek and full-scale/mute PCM samples.
Evidence: `.deps/phone-player-tests.{log,result}`. The generated provider, driver,
adapter, full ProviderHostScreen/Application and audio output compiled with the
pinned PS5 SDK (`.deps/phone-player-ps5-objects.log`). Complete packaging and actual
phone/PS5 acceptance are pending; no hardware success is inferred from host tests.

## Native pairing settings and identity storage — 2026-10-10

The provider now drives Lounge independently of browser request cancellation.
Phone pairing preferences offer explicit enable, code generation and reset;
the RML host refreshes the code screen while preserving its navigation stack.
Only a durably saved UUID can be advertised; the returned screen ID is saved
before pairing continues. Fields use the existing atomic, Lapy-backed
`/data/prosperotube/youtube.json` path. Code/session secrets are not persisted.
Reset, shutdown and source/profile changes preserve owned late-callback cleanup.
Write failures display a local-data warning and prevent advertisement; future or
corrupt storage is preserved.

`scripts/test_phone_provider.sh` passed ASan/UBSan, covering code formatting,
stable restart identity, blocked registration save, failed reset persistence,
successful reset, fresh identity, reset during registration and future-file
protection (`.deps/phone-provider-tests.{log,result}`, exit 0). Driver tests,
all nine native provider regressions and sign-in/chat/compose regressions passed.
The existing real RML callback/binding test also passed. The provider, driver and
RML host object targets compiled with the pinned PS5 SDK; no full new package or
console run is claimed. Evidence: `.deps/phone-native-regression-final.*`,
`.deps/phone-account-chat-regression.*`, `.deps/phone-rml-ui-tests.*`,
`.deps/phone-driver-tests.*` and `.deps/phone-native-ps5-objects.*`.

Player/queue command execution and state reporting remain unconnected. Native
random-source runtime, actual pairing, UI readability and phone acceptance need
PS5 verification after the complete integration. No console was contacted; the
last frozen playback candidate remains `9ac95e69`.

## Native Lounge driver foundation — 2026-10-10

`tube_remote.c` now implements asynchronous native registration, code requests,
polling, typed command delivery, explicit owner-tagged acknowledgments, player
state reports, failure/retry and shutdown. Whole responses are validated before
queue commit. Receiving continues while commands wait; in-flight poll responses
cannot replay already-applied commands, alter the outstanding prefix or cross a
provider/profile/identity scope. Late registration is closed using its captured
source/token before the next receiver starts. No protocol strings are logged.

`scripts/test_remote_native.sh` passed ASan/UBSan with the shipping C driver and
cJSON: all incoming command types, UTF-8 phone name, pending-command lease poll,
applied-prefix removal, state payload, source snapshots, late-start cleanup,
same-sequence/different-owner rejection, malformed/gapped command batches and
shutdown. Generated native overlay preparation passed and included the new source
in the PS5 Makefile. Its `addons/src/tube_remote.o` target compiled with the pinned
PS5 SDK (exit 0; `.deps/remote-native-ps5-object.{log,result}`); the only warnings
are the SDK math header's qualified `_Generic` associations for `isfinite`.

This is driver and object evidence, not a new full package or PS5 execution.
The provider does not call the driver yet, and pairing settings, Lapy identity
persistence and player/queue command execution remain open. No console traffic
occurred and the last frozen native candidate remains unchanged.

## YouTube Lounge receiver backend — 2026-10-10

Original companion transport now registers a Lounge screen, generates the phone
TV code, parses UTF-16-length BrowserChannel frames, receives typed commands and
sends playback/volume/queue/caption state through the authenticated `/v1/remote`
API. It starts only on explicit enable, independently of the library worker.
Command delivery requires native acknowledgment; duplicate wire IDs are ignored,
missing IDs and overflow fail closed. Lease expiry, ordered/cached state updates,
in-flight cancellation/failure, private protocol state, bounded termination and
app shutdown have offline coverage.

Forty protocol/API tests passed. Full companion regression passed 273 tests
(`.deps/lounge-full-api.{log,result}`, exit 0). A live temporary receiver on
YouTube registered, obtained a valid TV code, received loungeStatus/getNowPlaying,
and successfully posted paused state; local cleanup passed. Evidence:
`.deps/lounge-live-result.json`. The probe records only booleans and command names,
never the pairing code or Lounge credentials. No phone was linked or console
contacted. This establishes backend compatibility, not casting to PS5. Native
pairing/reset settings, Lapy identity storage and command execution remain open;
the native build/frozen candidate is unchanged. See `docs/REMOTE_SERVICE.md`.

## Playback-clock chat replay — 2026-10-10

The companion now reads replay through `live_chat/get_live_chat_replay`, sends
`currentPlayerState.playerOffsetMs` and preserves raw player-seek continuations
that the pinned parser does not expose. The request format was checked against
[yt-dlp's replay downloader](https://github.com/yt-dlp/yt-dlp/blob/master/yt_dlp/downloader/youtube_live_chat.py)
and the pinned YouTube.js replay node. Replay requires `chat_position_sec`;
messages/deletions wait for their video offsets, pause holds the timeline, and
rewinds or large jumps reset rows and use the server seek continuation. Replay
is read-only, scoped to its profile and cursor, with bounded actions/buffering,
four upstream requests at most per update and no background timers. Failed
responses preserve the last committed timeline. Exact request repeats are cached;
older cursors cannot seek a newer view.

The native player sends its displayed position, accepts only a matching replay
response timestamp, shows CHAT REPLAY in both HUDs, and clears old rows on a seek.
An in-flight pre-seek result supplies the next scoped cursor but cannot repopulate
rows; the next request uses the new video position. A rewind can reopen an ended
timeline. The browser reader still has no replay clock and directs users to play
the video; full replay-history browsing remains open.

Validation: 49 worker tests passed; the bundle rebuilt. Full companion regression
passed 233 tests; focused chat/library/fixture regression passed 24. Native provider,
player chat, extracted shipping HUI/RML and chat options passed ASan/UBSan
(`.deps/chat-replay-native.{log,result}`, exit 0), including fractional position,
forward seek, ended rewind, delayed pre-seek suppression and invalid clock cleanup.

Live Guest checks used two public recordings, including
[NASA's Artemis I broadcast](https://www.youtube.com/watch?v=CMLD0Lp0JBg): 120-second
and 5-second views contained only messages at/before the requested time. NASA
returned 19 and eight messages respectively; another public replay returned 24
and 20. One other video reported unavailable chat. The rebuilt bundle also passed
through Python LibraryService at positions 120, paused 120 and rewind 5, including
clean close. Evidence: `.deps/chat-replay-live-result.json`,
`.deps/chat-replay-process-result.json` and `.deps/verify-chat-replay-live.py`.
These checks prove service behavior, not PS5 replay rendering, audio/motion or
account acceptance. Native candidate `6097d274` passed PS5 build/freeze:
126 files, build `prosperotube-6097d274_1010-2211`, eboot SHA-256 prefix
`09fef4dd66beda0c` (`.deps/chat-replay-{ps5-build,freeze}.{log,result}`, exit 0).

Final refinements cover paused seeks through more than four sparse pages: bounded
catch-up continues on subsequent explicit polls until a future action or EOF,
then pause avoids extra upstream reads. Signed pre-stream offsets remain visible
at frame zero. The native retry anchor now follows the decoder's settled position
if a paused seek lands just before its requested target; future/pre-seek rows
remain suppressed. Forty-nine worker tests and the final native/HUD/menu sanitizer
check passed (`.deps/chat-replay-final-native.{log,result}`, exit 0). The first
frozen native candidate predates this settled-position refinement. Corrected
candidate `9ac95e69` passed PS5 build/freeze: 126 files, build
`prosperotube-9ac95e69_1010-2216`, eboot SHA-256 prefix `1130e1801750a6c7`
(`.deps/chat-replay-final-{ps5-build,freeze}.{log,result}`, both exit 0).
The latest rebuilt bundle repeated the Python process replay check successfully.
No console work was performed in this replay batch; PS5 rendering/timing and
physical appearance acceptance remain pending. Deploy the matching new companion
and bundle: an older running companion rejects the new playback-clock field.

## In-player live-chat controls — 2026-10-10

R2 Playback Options now offers Hide / Left / Right before the speed list for
YouTube video sources. The saved choice has a SELECTED badge; existing quality,
audio and speed focus behavior is preserved. Live sources can show the controls;
Music mode and other providers do not. A stale selection rechecks availability.
The action changes chat state without reopening media. Hiding closes its player
cursor; the native setter retains rows/context on side changes and selecting a
side after a terminal error explicitly retries. Leaving the player screen for
Playback Options closes its existing context; returning restarts scoped chat
without reopening media. The preferences screen uses the same setter.

Provider/chat and speed-memory tests plus shipping HUD/menu extraction and
controller speed-reopen tests passed ASan/UBSan
(`.deps/player-chat-controls-tests.{log,result}`, exit 0), including invalid
modes, saved selection, retained rows on side changes, same-side error retry,
live/non-YouTube/Music gating and stale selection. The final extracted HUD/menu check
also passed after moving chat rows ahead of the speed list
(`.deps/player-chat-controls-final.{log,result}`, exit 0). Committed candidate
`88f28929` passed PS5 build/freeze: 126 files, build
`prosperotube-88f28929_1010-2153`, eboot SHA-256 prefix `10cbdbd3ed118f8c`
(`.deps/player-chat-controls-ps5-build.{log,result}` and
`.deps/player-chat-controls-freeze.{log,result}`, both exit 0).

Console acceptance remains unverified. The first primary case stopped at its
fresh-state guard without changing saved state. The fallback case and subsequent
primary case, adapted to preserve original state bytes, both detected a title
starting during upload and stopped before binding the fixture or launching.
Installations may contain a subset of the new files; a future case must verify
all frozen bytes and settle before launch. Original provider state was not
modified. The isolated fixture was stopped and the shared lock released.
Evidence: `.deps/player-chat-controls-{hardware,fallback-hardware,preserved-hardware}`
and the corresponding ignored case directories. These are deferred acceptance
cases, not passes. Chat replay remains unimplemented.

## Player live-chat sidebar — 2026-10-10

The pinned SmartTube `ChatController` places live chat beside playing video.
ProsperoTube now has a saved Off/Left/Right sidebar preference under YouTube
preferences. The player requests top chat while Playing/Paused/Seeking/Scrubbing;
the sidebar hides during stats, scrubbing and Music mode. Both shipping HUDs draw
the latest six author/message previews independently of the controls fade.
The existing browser reader retains full text and history navigation.

Foreground requests take priority. The sidebar never cancels a resolve/library
request, owns one scoped chat context, stops polling after a terminal error/end,
and closes its context when the player leaves. Late responses after closing are
discarded and their remote cursor is closed. Long text is clipped at UTF-8
boundaries, including a partial codepoint in an upstream fixed-size item. The
worker now matches the pinned SmartTube author filter and Unicode lookalikes.

Worker regression: 42 passed; bundle rebuilt. Native provider/sidebar regression
exited 0 (`.deps/player-chat-native.{log,result}`). Final sidebar plus extracted
shipping HUI/RML blocks passed ASan/UBSan (`.deps/player-chat-final.{log,result}`,
exit 0), covering ownership, delayed polling, foreground priority, six-row bound,
failure handling, late cursor cleanup, UTF-8, placement, escaped text, stable DOM
updates and suppression. Committed candidate `112dadf8` passed the PS5 build and
freeze: 126 files, build `prosperotube-112dadf8_1010-2146`, eboot SHA-256 prefix
`08df0ecd51c02b74` (`.deps/player-chat-ps5-build.{log,result}` and
`.deps/player-chat-freeze.{log,result}`, both exit 0). The combined six-section,
speed and sidebar lifecycle/screenshot case stopped at its first read-only
running-title check on both consoles (`.deps/player-chat-hardware` and
`.deps/player-chat-fallback-hardware`): no upload, launch or state write occurred.
The isolated fixture was stopped and the shared lock released. Player-sidebar
hardware acceptance, physical readability, in-player toggle controls and chat
replay remain open.

## Companion job admission — 2026-10-10

The six-section PS5 case exposed a busy response after rapid browsing: completed
jobs were retained for two minutes and counted against the 16-job concurrency
limit. Gaming/News/Sports/Live/Movies opened their fixture shelves and video lists;
Music's row request was rejected. Matching-build GPU park, exact owned teardown,
healthy services and restoration of the original absence of provider state passed.
Evidence: `.deps/console/six-topics-hardware/verified-failure.json`; the functional
case exited 1 and the independent teardown/partial-navigation verifier passed.

Admission now counts only in-flight tasks. Up to 80 job records are retained for
repeat polling, with delivered completed results evicted first. Abandoned results
use the existing sign-in/chat cleanup. Invalid submissions do not evict results.
The API/auth/chat/library regression passed 83 tests
(`.deps/job-capacity-tests.{log,result}`). The final capacity-only run passed three
tests (`.deps/job-capacity-final.{log,result}`), covering 140
successive jobs, retained/evicted polling, 16 real pending jobs, cancellation and
abandoned-chat cleanup. The exact six-section job sequence also passed through
the shipping fixture API (eight fixture tests passed;
`.deps/job-capacity-six-section.{log,result}`, exit 0). Corrected companion
`991728f` paired with unchanged native candidate `fb3bf1aa` was prepared for a
retest. Both authorized consoles had other running titles at the first read-only
idle check, so neither case changed files or launched. Both owned fixture servers
were stopped. Combined console acceptance with the corrected companion is pending.

## Remaining TV topic sections — 2026-10-10

Sports, Live and Movies now have native Discover entries and use the reference
`FEtopics_sports/live/movies` TV endpoints. Section capabilities preserve their
origin and account scope. Channel and Movies storefront folders retain nested
shelves; movie video identities use the ordinary playback resolver. An empty
storefront returns YouTube's explanation as `catalog_unavailable`, rather than
an unsupported-layout error. This adds browsing, not purchase/DRM playback.

Live Guest checks returned Sports: four shelves / 18 items / three live markers;
Live: three shelves / 15 items / ten live markers; Movies: three shelves / seven
items, one nested storefront shelf and one empty purchase-library explanation.
Evidence: `.deps/browse-reference/extra-adapter-result.json` and
`.deps/extra-topics-live-adapter.{log,result}` (exit 0). Counts can vary upstream.
Worker regression: 41 passed, including nested storefront/channel shelves,
continuations and empty-storefront reporting. Python library/API regression:
18 passed, with all six section identities exercised through the shipping API.
Native section/Music/provider ASan/UBSan regression exited 0
(`.deps/extra-topics-native.{log,result}`); the worker bundle rebuilt successfully.
Candidate `fb3bf1aa` passed PS5 build and immutable freeze (126 files; eboot prefix
`178637c2e00add14`). Evidence: `.deps/extra-topics-{ps5-build,freeze}.{log,result}`,
both exit 0. No console deployment was performed for this candidate.
PS5 navigation/playback acceptance for these three new entries remains open.

## Personalized Music row — 2026-10-10

Signed-in account profiles now prepend Liked music to the Music section. The
request follows pinned MediaServiceCore `4d9767733aa9` `BrowseService2` /
`BrowseApiHelper`: TV liked-Music continuation, then WEB `VLLM` fallback. Row
capabilities and continuation cursors retain profile ownership and client type.
WEB playlist metadata and append/reload continuation layouts are supported;
unplayable playlist entries are excluded. Guest and expired sessions make no
personalized request; unavailable or empty rows preserve the public shelves.

Worker regression: 39 passed (`.deps/liked-music-worker.{log,result}`), including
TV success, WEB fallback/continuation, cross-profile rejection, missing/empty
rows and expired sign-in. Final topic suite: seven passed after layout cleanup
(`.deps/liked-music-topics-final.{log,result}`). Live Guest adapter checks returned
four initial shelves and another shelf page for each public section, plus a
Music playlist (`.deps/liked-music-live-guest.{log,result}`, exit 0). The worker
bundle was rebuilt. Real-account personalized data and PS5 account-row acceptance
remain unverified; no regular companion restart or console action was performed.

## Curated TV sections — 2026-10-10

Music/Gaming/News now use the TV `FEtopics_*` browse endpoints rather than
search placeholders. Shelves remain folders with server-issued capabilities;
row/playlist navigation, local 24-item pagination and upstream continuations
retain section and profile scope. Music origin survives nested row/playlist
navigation. Row retention is bounded to 128 capabilities / 8 MiB of source
data with expiry, and upstream endpoints stay on the companion.

Reference: pinned MediaServiceCore `4d9767733aa9`, `BrowseApiHelper` and
`BrowseService2.getMusic/getGaming/getNews`; SmartTube prepends a personalized
liked-Music row, which remains unimplemented. Live Guest checks returned four
initial shelves and another shelf page for each section, playable row contents,
and a 14-item Music playlist. A tracking-only response at continuation exhaustion
is an empty terminal page; unsupported initial layouts remain errors. Evidence:
`.deps/browse-reference/live-adapter-result.json`.

Worker tests: 35 passed. Python library/API tests: 18 passed, including fixture
section capabilities and the job path. Native section routing, row IDs,
pagination and nested Music playback/queue origin passed ASan/UBSan. The console
fixture gate is recorded below; personalized account coverage remains open.

Final native section/Music/provider regression exited 0 (`.deps/topics-native.log`).
Candidate `57766c09` passed PS5 build/freeze: 126 files, eboot prefix
`82262e0635707fc7`, build `prosperotube-57766c09_1010-2053`. Evidence:
`.deps/topics-{ps5-build,freeze}.{log,result}`, both exit 0. The companion worker
was rebuilt. The regular companion remains unchanged.

The frozen candidate subsequently passed a combined PS5 case using an isolated
generated AVC/AAC companion. Music, Gaming and News each opened their shelf and
video list. Music playback started at 1× and offered Restore 1.75×; opening the
same video through ordinary search used the saved 1.75× preference. Sampled
media/host ratios were 0.963 and 1.686 respectively, within the 0.15 tolerance
for the once-per-second status samples. The saved global preference remained
175. Playback published 2,680 frames with three late drops across both opens.
Normal Stop, matching-build GPU park, exact process/video-owner/mount release,
healthy services and restoration of the original absence of provider state
passed. The owned fixture companion was stopped. Evidence:
`.deps/console/topics-hardware/verified-result.json`; case and independent
verifier exited 0. This accepts native section wiring and Music speed behavior
with fixture data; physical picture/sound and real-account sections remain open.

## Music-origin speed behavior — 2026-10-10

The explicit Music section has its own navigation identity. New playback from
that section starts at 1×, following pinned SmartTube `Video.belongsToMusic()` /
`VideoStateController.restoreSpeedAndPositionIfNeeded`. Ordinary searches for
"music" and history/favorites do not acquire this exception. Resolve captures
the origin before asynchronous work; queue insertion persists it per profile,
including manual queue playback and autoplay. Explicit speed changes remain
available and recovery keeps the selected session speed. Saved global/channel/
video preferences remain intact. The Music catalog is still search-backed;
SmartTube's curated Music feed is not implemented by this change.

The real-provider Music-context ASan/UBSan test passed normalization, explicit
speed changes, recovery, canceled async results, queue reload/autoplay and
profile isolation. All seven provider suites reported passes in
`.deps/music-context-provider.log`; the outer command's exit-code capture was
malformed. The final Music/speed-memory/recovery run used a corrected wrapper
and exited 0 (`.deps/music-context-final.{log,result}`), including source setup
clearing the old navigation context. PS5 acceptance remains pending.

Candidate `3b6f0e67` passed PS5 build and freeze (126 files; eboot prefix
`6a6c8d693d02b739`). Evidence: `.deps/music-context-{ps5-build,freeze}.{log,result}`,
both exit 0. No console deployment was performed for this candidate.

## SponsorBlock transparent review — 2026-10-10

Both shipping HUD paths now show a persistent translucent skip/keep card outside
the playback controls' fade group. Pending review keeps rendering active with
the actual media position after the ordinary controls disappear. Scrubbing and
diagnostics retain their Cross/Circle input; live/music states suppress the card.
The repeated review toast has been removed. Existing skip and error feedback stays.

Passed ASan/UBSan: `test_native.sh sponsor`, `test_sponsor_player.sh`, and
`test_sponsor_review.sh`. The latter extracts the shipping HUI draw/focus and
fallback DOM blocks and checks visibility, expiry, suppression, stable DOM
updates and text escaping. This is offline evidence, not console acceptance.

Source `e8486d38` passed the PS5 build and immutable freeze (126 files;
eboot prefix `c14090d6f9e367ef`; build `prosperotube-e8486d38_1010-2024`).
Evidence: `.deps/sponsor-review-{ps5-build,freeze}.{log,result}`, both exit 0.
The same frozen candidate subsequently passed a PS5 generated AVC/AAC case with
known segment times. Review remained focused at 16.64 seconds after controls
faded; Circle kept playback at 17.66 seconds, Cross on the next review sought to
55.68 seconds, and an untouched review expired by 78.85 seconds. Normal Stop
returned to the provider. Build-scoped logs recorded 1,824 published frames,
zero late drops and one successful seek with no failures. GPU park, exact
process/video-owner/mount release, service health and restoration of the original
absence of provider state passed. The owned fixture server was stopped and lock
released. This does not prove real SponsorBlock service behavior or physical
appearance/sound. Evidence: `.deps/console/sponsor-review-hardware/verified-result.json`
and `.deps/sponsor-review-hardware.{log,result}` (exit 0).

## PS5 speed controls acceptance — 2026-10-10

Frozen `17455475` passed a bounded public Big Buck Bunny case on the fallback
console after idle checks, full installation verification, a 130-second settling
period and confirmed title registration. A temporary Guest configuration was
used because this console had no saved provider state. The existing companion
was unchanged. The primary console was busy and was not modified this turn.

The corrected case passed initial 1.25× playback, a paused R2 toggle to normal
speed, resume, restoration of the last 1.25× speed, forward/backward seeks, Stop
and persistence of the selected/last speed. Position stayed exactly 43.04 s
across the pause hold and paused reopen. Sampled progression ratios were 1.211,
0.973 and 1.301 (1 Hz position telemetry; respective tolerances ±0.10/0.15/0.15).
Three playback opens published 5,035 frames with 108 late drops across the case;
the last session reported four successful seeks and zero failed seeks.
These results do not establish physical smoothness, pitch or perceived sync.
Per-channel/video memory remains host-tested rather than console-accepted.

GPU park, exact process/app/video-owner/mount release and service health passed.
The temporary provider configuration was deleted, restoring its original
absence, and the shared lock was released. Evidence:
`.deps/speed-controls-fresh-aligned.{log,result}` (exit 0) and
`.deps/console/speed-controls-fresh-aligned/verified-result.json`. The verifier
limits accumulated console logs to the final matching boot.

The preceding fresh-install case measured 1.245× but stopped at an inconclusive
pause check: an event-driven UI snapshot was compared with an independently
sampled older position file. It is retained as exit 1. A nonstandard positive
FTP delete reply (`226`) also raised in Python; a separate idle-first read
verified the temporary configuration was absent. The corrected adapter waits
for a position timestamp strictly after the paused UI event and handles that
positive reply with an absence check. Identical frozen bytes were rerun without
upload; no playback code changed to obtain the pass.

## Remembered speed and quick toggle — 2026-10-10

Reference: SmartTube `2c60453007df`, `PlayerData` and `VideoStateController`.
Four mutually exclusive memory modes now cover all videos, each channel, each
video and the current playback only. New stores default to channel memory;
legacy stores with a saved global speed retain all-videos mode. Unknown channels
or videos use 1×. Normal speed removes a scoped override. Channel/video entries
are keyed by profile and retained independently of mode switching. The bounded
list keeps the 1,024 most recently changed overrides without evicting favorites
or library records. Invalid and duplicate persisted entries are rejected.

The selected session rate survives track/speed reopen and recovery, while a new
playback applies the chosen memory mode. Settings selecting a global speed
explicitly enable all-videos mode. Successful in-player changes store against
the copied source's video identity. The last non-normal rate persists; the R2
picker focuses a normal/last-speed toggle when available. Live playback still
uses 1×. The reference's music exception uses `Video.belongsToMusic()` (the
originating Music group), not title/category guessing; retaining that browsing
context remains open.

The actual provider passed ASan/UBSan for channel/video/session/global restore,
profile isolation, reload, legacy migration, invalid stored values, last-speed
retention and capacity/eviction. The existing provider regression also passed
(`.deps/speed-memory-provider.log`). Hardware behavior remains unaccepted.
The generated controller handoff test also passed with an assertion on the
video identity passed to memory storage (`.deps/speed-memory-switch.log`).
PS5 candidate `17455475` passed build and immutable freeze: 212 objects, 126
files, build `prosperotube-17455475_1010-2002`, eboot prefix `c89ff35e59e6ac5f`.
Evidence: `.deps/speed-memory-{ps5-build,freeze}.{log,result}` (both exit 0).
No console connection or deployment was attempted for this candidate.

## In-player speed changes — 2026-10-10

R2 playback options now offer all 30 reference speeds for a ready on-demand
YouTube session. Selection reopens the copied source at its displayed fractional
position, preserving pause and audio/subtitle choices. A scoped override applies
the new rate only after workers stop. Success saves the preference; failure
clears all temporary intents without saving it. Live/unknown-duration sources,
EOF, scrubbing, seeking, discard and prebuffer work reject a competing change.
Selecting the active speed is a no-op.

The internal reopen suppresses the provider's terminal STOP report so history
sync survives. A failed reopen reports one terminal stop using the saved source
and position; a subsequent user Stop after success reports normally. The host
test extracts both the actual generated controller methods and stop-report
branch, with an instrumented open boundary. It covers source identity,
fractional position, pause, external/embedded captions, audio selection,
successful/no-op/failed changes and competing-work gates. Provider tests also
cover the speed enumeration and persisted setter. Both passed ASan/UBSan:
`.deps/speed-switch-final.log` and `.deps/speed-switch-provider.log`.
Console acceptance remains
open; the previous busy-console results below still apply.

Candidate `2512c069` passed the PS5 build (212 objects) and immutable freeze
(126 files), build `prosperotube-2512c069_1010-1953`, eboot prefix
`82fd27d8acbcf7a9`. Evidence:
`.deps/speed-switch-{ps5-build,freeze}.{log,result}` (both exit 0).
No console connection or installation was attempted for this candidate.

## Playback-speed player integration — 2026-10-10

Decoded PCM and the resampler EOF tail now pass through the tempo stage before
the existing bounded audio queue. Each block records its rate and valid sample
count; successful output advances the media clock by samples × rate / 48000.
Seek epochs discard delayed tempo data. Rate changes are accepted only while
both audio workers are stopped. Settings persist all 30 reference speeds and
apply them on the next on-demand YouTube open; live and other sources use 1×.
SponsorBlock receives the actual session rate.

The shipping presentation clock preserves media position over pause, resume and
rate changes, retains rate over reset/reanchor, converts media waits to host
time, and follows an audio master without scaling it twice. Its fallback anchor
tracks audio so a longer video tail can continue after audio drains.
`scripts/test_rate_clock.sh` passed ASan/UBSan with mocked host/audio time.
The actual generated audio path passed ASan/UBSan with pinned FFmpeg 7.1.1 at
0.25, 0.5, 1.25, 2 and 4×: queue accounting, EOF, live-rate rejection and stale
seek-data disposal. Evidence: `.deps/rate-clock-final.log` and
`.deps/rate-audio-integrated.log`.

All five native provider suites passed, including speed-page boundaries,
persisted selection, exact labels and invalid choices, alongside recovery,
SponsorBlock, paid-content and DeArrow regressions (`.deps/rate-provider.log`).
The generated player tick/pad ASan/UBSan suite also passed, asserting that
SponsorBlock receives the session rate (`.deps/rate-sponsor.log`).

PS5 candidate `3cb46c5e` passed compilation of 212 objects, packaging and
immutable freeze: 126 files, build `prosperotube-3cb46c5e_1010-1942`, eboot
prefix `e11b6e015c5d7b6f`. Evidence:
`.deps/rate-player-{ps5-build,freeze}.{log,result}` (both exit 0).

Hardware speed/pitch/sync acceptance and changing speed during an active video
remain open; the earlier foundation section describes its historical state.

The bounded 1.25× console case did not launch: the primary console was initially
idle, then another title appeared during delta upload. The adapter stopped at
its per-file running-title check. The fallback was already busy at its first
read-only check. No title was closed, no test input or temporary speed preference
was sent, and the shared lock was released. Primary installation is partially
updated and must complete a full frozen-file comparison and settling period
before any later launch. Evidence: `.deps/rate-player-hardware.log` and
`.deps/rate-player-hardware-fallback.log` (both no-run, exit 1).

## Playback-speed audio stage — 2026-10-10

`tube_audio_tempo` is a single-owner interleaved S16/48 kHz stage using the
existing pinned FFmpeg 7.1.1 `atempo` filter. It supports the 30 reference speeds
from 0.25× through 4×, cascading factors within 0.5–2 to avoid the filter's
sample-skipping path above 2. Unity speed bypasses filtering byte-for-byte.
Natural EOF drains once; callback cancellation is latched, and destroying a
seek epoch discards its delayed samples. The graph uses one thread and bounded
input chunks. Invalid channels/rates and missing filters fail closed.

The real FFmpeg host test passed ASan/UBSan across all reference speeds and 1–8
channels, varied chunk sizes, sine pitch/channel separation (1.5% tolerance),
four-second input duration (100 ms output tolerance), unity bytes, empty/repeated
EOF, cancellation and epoch disposal. Evidence: `.deps/tempo-native-final.log`.
Run `PROSPEROTUBE_TEMPO_FFMPEG=<built-FFmpeg-source> bash scripts/test_audio_tempo.sh`
or use installed `libavfilter`/`libavutil` development packages. The pinned host
build was reconfigured to enable `atempo`; `.deps/tempo-host-build.log` records it.

The native build now enables `atempo`, links `libavfilter`, compiles the adapter
and invalidates older FFmpeg SDK stamps. PS5 candidate `9829b316` passed the
FFmpeg rebuild, all 212 native objects, package and immutable freeze (126 files,
eboot prefix `242ad70b513f417d`). The installed PS5 archive contains `ff_af_atempo`
and its configuration enables `CONFIG_ATEMPO_FILTER`. Evidence:
`.deps/tempo-stage-{ps5-build,freeze}.{log,result}` (both exit 0). No console was
connected or changed for this batch. This is the
audio-stage foundation, not working player speed selection: queue media-clock
accounting, video-only pacing, rate changes, persisted controls and hardware
sound/sync acceptance still require integration. The stage is not yet called by
the player's audio path; existing playback behavior is unchanged.

## Retained live-chat history — 2026-10-10

The native reader requests `chat_history: true`, opening at the newest message
and allowing existing L1/R1 paging through up to 256 retained messages. Worker
snapshots also cap serialized message data at 768 KiB, preserving full text while
remaining below the 1 MiB worker line limit. Legacy requests retain 24-message
responses. Authoritative snapshots apply moderation to retained history; stable
message identities preserve selection and text scroll across updates. Native
chat text has separate bounded RAM storage, cleared on context changes.

Validation: 32 worker tests passed, including retained-history moderation and
large Unicode response bounds; 17 targeted companion tests passed and the four
API tests passed again after adding the history-field forwarding assertion.
Both native chat/compose executables passed ASan/UBSan. The actual generated RML
provider test passed newest-message opening, paging into older history,
selection/text-scroll preservation after updates, removal and mode/setup cleanup.
The initial UI test's first-page assumption was corrected for newest-message
opening; its failed log is retained. The worker bundle built with the cached Linux
Node runtime after a Windows/Linux esbuild mismatch and absent system Node were
identified. Evidence: `.deps/chat-history-*`. Native PS5 candidate `639b015a`
passed build, package and immutable freeze (126 files, eboot prefix
`64f9d8b3b6bffab4`). A bounded PS5 case with an isolated generated-fixture
companion then passed: newest-message opening, L1 paging to an older message,
stable selection across eight observations while snapshots grew from 24 to 31
messages, and top-to-live switching with context retirement. Original pairing
and history were restored byte-for-byte after GPU park and exact process/title/
mount release. Services remained healthy, the owned lock was removed, and the
isolated fixture server was stopped. The public companion was left running.
Evidence: `.deps/chat-history-hardware.{log,result}` (exit 0),
`.deps/console/chat-history-hardware/{history-focus,fixture-request-summary,restored}.json`.
This establishes automated fixture/controller behavior, not physical readability,
real-account chat or moderation on hardware. No messages were posted and no video
played in this case. Replay and history preceding chat opening remain open.

## Native WebP artwork — 2026-10-10

The existing PS5 FFmpeg 7.1.1 build already enables WebP decoding and parsing;
provider artwork was rejecting RIFF/WebP before reaching it. The overlay accepts
both RIFF and WEBP signatures, writes `.webp` cache entries, and rediscovers those
entries on subsequent launches. Cache writes now close their file even after a
short write, preventing the existing failure-path descriptor leak.

The generated shipping sniff/download/cache-request/decode functions passed host
ASan/UBSan with real FFmpeg: lossy/lossless WebP, premultiplied alpha, 480-pixel
scaling, persistent cache reuse without a second network request, PNG/JPEG,
malformed input and injected write failure. Run `python3 scripts/test_artwork.py`;
evidence is `.deps/artwork-native.log`. No new decoder library is required.

Frozen native candidate `327d8606` (`prosperotube-327d8606_1010-1851`,
211 objects, 126 files, eboot prefix `69879753599e55bd`) passed PS5 installation,
settled readback, startup and public Big Buck Bunny search. Its 135,454-byte
RIFF/WebP thumbnail decoded to 480×270; the matching `.webp` cache entry was
read back from `/data/prosperotube/providers/youtube/art`. Guest pairing/history,
GPU park, exact process/title/mount release and service health passed. The owned
lock was removed. This case did not play video or assess the thumbnail's physical
appearance. Evidence: `.deps/webp-artwork-console.{log,result}` (exit 0),
`.deps/console/webp-artwork/artwork-result.json` and `cached-thumbnail.webp`.

The same batch corrected offline chat fixtures that returned more than the native
reader's 24-message limit after sending. Both immediate and timed snapshots now
retain that limit. Sixteen targeted chat/API/fixture tests passed, including one
and thirty sends, duplicate-intent confirmation, immutable earlier snapshots and
cached poll responses. No real messages were sent; scrollback remains unfinished.

## Indexed full-file DASH resume — 2026-10-10

The captured PS5 initial-resume stall led to a controlled FFmpeg 7.1.1 comparison.
A three-minute seek in a four-minute fragmented-MP4 fixture transferred 12,695,973
bytes before the patch versus 942,088 afterward (15,577,449 total media bytes);
local seek/decode time fell from 1.698 s to 0.101 s. These are local host timings,
not a measured PS5 speedup. The probe decodes video and passes audio packets.
Segmented DASH and separate initialization-section layouts also passed.
Evidence: `.deps/dash-seek-{pinned-baseline,patched-*}.json`.

The guarded FFmpeg overlay advertises seekability only for complete single-file
VOD backed by a byte-seekable input, and forwards size/position requests correctly.
The PS5 build applies it after extracting the pinned FFmpeg source and includes
its SHA-256 in the installed-library marker, forcing stale libraries to rebuild.
Initial minimal host configurations exposed an FFmpeg H.264/HEVC film-grain link
dependency; the successful comparison uses the same corrected build on both sides.
PS5 build/freeze passed: `prosperotube-1e523316_1010-1834`, 126 files,
eboot prefix `f2561db427571559`. Automated hardware acceptance passed after
correcting an evidence assertion: the initial resume increments the seek counter,
so resume plus forward/backward controls requires `seek_req=3 ok=3 fail=0`, not 2.
The original runner exit 1 and logs remain; an offline verifier checked the same
captured run, exact PID/app release and GPU park without another console launch.

Selection-to-first-frame at the saved 161-second position was **5.367 s**; the
native initial-seek interval fell from 65.734 s to **0.338 s**. The initial active
state arrived in 3.010 s, before preroll completed, and is not used as first-picture
evidence. Playback published 2,042 frames (7 late drops), both controller seeks
resumed advancing, and clean title/mount release and service health passed.
Physical sound/motion were not assessed. Evidence:
`.deps/console/dash-index-resume/verified-result.json`, `.deps/dash-index-console.log`.
The same run also exposed unsupported WebP thumbnail data in the existing artwork
loader; browsing-image support is the next distinct fix.

An additional WebM/VP9 fixture passed: 15 decoded frames and audio packet delivery,
813,382 bytes transferred from 10,249,991 total (`.deps/dash-seek-patched-webm.json`).
A transport deliberately ignoring byte ranges failed in both original and patched
builds; this remains an existing unsupported seek case (`.deps/dash-seek-*-no-range.json`).

The first PS5 library build was intentionally stopped after measuring repeated
LLVM discovery at 0.568 s versus 0.029 s for the same resolved LLVM 18 executable.
Restarting with explicit `LLVM_CONFIG` completed; the build script now resolves
that path once unless the caller supplies one. No SDK files or compiler version
were changed. Cancellation evidence: `.deps/dash-index-cancelled-slow-discovery.json`;
build/freeze logs: `.deps/dash-index-*`.

## DeArrow / SponsorBlock / section tracking batch — 2026-10-10

- Implemented independent DeArrow titles/thumbnails, original metadata persistence,
  profile/source isolation, queue/recovery presentation, bounded hash-prefix API
  cache and image capabilities with original-image fallback. Pinned MediaServiceCore
  `DeArrowService` selects the first non-original submission; API trust checks apply.
- SponsorBlock now uses the reference 2-second × rate start window, end+3-second
  grouping across skip/message actions, final grouped action, ungrouped prompts,
  and no-repeat consumption of ignored/declined matches. The current engine stays
  at 1x; actual playback-speed controls remain a separate gap.
- All 224 companion tests passed (`.deps/parity-batch-full-python`), including
  the 71 targeted cases (`.deps/parity-batch-service`); all five native
  executables and generated player tick/pad paths passed ASan/UBSan
  (`.deps/parity-batch-native-combined`, `.deps/parity-batch-player`). Earlier
  late-position fixtures failed under the new reference window and were corrected
  while preserving minimum-duration and competing-highlight assertions.
- Live public DeArrow lookup matched Big Buck Bunny and returned a replacement
  title (`.deps/dearrow-public-bunny-smoke.json`). PS5 rendering remains unverified.
  Shared native code now compiles once per suite; the plan tracks all sections in
  one table. Candidate `4a840c58` passed all 211 PS5 objects, packaging and
  immutable freeze: `prosperotube-4a840c58_1010-1756`, 126 files, eboot
  `cf9dbbc6a68230b1` (`.deps/parity-batch-ps5-build`, `.deps/parity-batch-freeze`).

- Hardware regression: all 126 files verified before/after settling; matching
  startup, native UI and public resolution passed. Saved-position startup exceeded
  the 60-second test deadline: the log isolates a 65.734-second interval between
  native audio initialization and initial seek flush. The same owned process later
  played from 124 to 159.46 seconds with `active=1`, `fatal=0`; decoded frames and
  presentation were observed, but smooth motion/audio were not assessed. The failed
  timed criterion remains recorded (`.deps/parity-batch-console`). Cleanup verified
  the same PID/app/build, parked the GPU, confirmed exact title/mount release and
  healthy services, and released the lock (`.deps/parity-batch-cleanup`). No relaunch.
  Slow initial resume is the next playback investigation; DeArrow and SponsorBlock
  hardware behavior remains pending.

## Optional paid-promotion notices — 2026-10-10

The companion captures YouTube's `paidContentOverlayRenderer.text` from player
responses with the exact requested video ID. Both guest and TV OAuth extractors
share the bounded metadata hook; no extra network request is added. Localized
simpleText/runs are normalized to at most 240 UTF-8 bytes, with malformed optional
metadata ignored. Direct and job resolve responses include `paid_content_text`.

The native preference is off by default and saved under the existing Lapy-backed
app directory. A successful YouTube start consumes the notice once, independent
of SponsorBlock enablement, excluded channels or segment-server availability.
Video/source/profile/playing-instance guards reject stale notices; stream recovery
does not repeat a consumed notice. The generated browser displays the notice only
after successful playback navigation, on the main UI thread.

Reference behavior was read from pinned SmartTube
[`VideoLoaderController`](https://github.com/yuliskov/SmartTube/blob/2c60453007dfc3775ff525c3837c2d1d4e95aab9/common/src/main/java/com/liskovsoft/smartyoutubetv2/common/app/models/playback/controllers/VideoLoaderController.java)
and `SponsorBlockData` (default false), plus pinned MediaServiceCore
[`VideoInfo`](https://github.com/yuliskov/MediaServiceCore/blob/4d9767733aa92269b9d24f24844a7fa1bb5196c1/youtubeapi/src/main/java/com/liskovsoft/youtubeapi/videoinfo/models/VideoInfo.java)
and `YouTubeMediaItemFormatInfo`. This is a promotion disclosure, not a
paywall/entitlement flow. The implementation is original.

Companion tests passed: initial 16 cases including both resolve routes, then 15
paid-content cases after bounding malformed Unicode and run lengths. Evidence:
`.deps/paid-content-companion{,-final}.{log,result}`. Actual shipping native
provider, recovery, SponsorBlock and paid-notice executables passed ASan/UBSan,
including default off, persistence, once-only start, recovery and stale scopes:
`.deps/paid-content-native.{log,result}` (0). The generated shipping successful-start
UI block also passed ASan/UBSan: `.deps/paid-content-ui.{log,result}` (0).
The affected guest/account/extractor/HLS regression passed all 34 tests:
`.deps/paid-content-extractor-regression.{log,result}` (0). Complete PS5 build,
linking, packaging and immutable freeze passed: `prosperotube-4a0b4572_1010-1624`,
211 objects, 126 files, eboot SHA prefix `d9e42d51fb458b80`. Evidence:
`.deps/paid-content-ps5-build.{log,result}` and
`.deps/paid-content-freeze.{log,result}` (all 0). The candidate is undeployed;
paid-notice hardware acceptance remains pending. The installed validated build
remains unchanged.

## Public 1080p60 viewing acceptance — 2026-10-10

The user watched public Blender video `aqz-KE-bpKQ` (635 seconds) through the
unchanged validated native `5cc941f0` and existing Windows companion. The console
was idle at the first title check; all 126 installed files matched the immutable
candidate, registration and final idle checks passed, and exactly one launch was
sent. No installation, settings, pairing or companion change was made.

Native `sceVideodec2` opened AVC at 1920x1080/60 fps; native `sceAudiodec` opened
AAC stereo at 44100 Hz. Playback advanced from 1.07 seconds through the bounded
120-second observation. The shipping sweep recorded 7457 decoded/published frames,
zero late/seek drops and zero decoder fatal, with realtime verdict. The user
reported: "Video and sound work smoothly and stay in sync". This establishes
physical sound, motion and synchronization for this clip and bounded session.
It does not establish all codecs, HDR, controller feel, long sessions or parity.

Stop confirmation returned to the provider, Guest history persisted, matching
GPU park was established, and the exact launched PID/app released its title,
video ownership and mount. Declared services remained reachable; the owned lock
was removed. The adapter's final classification was partial-pass pending human
observation; the retained user observation completes this case's acceptance.
Evidence: `.deps/public-bunny-watch-console.{log,result}` (0),
`.deps/console/public-bunny-5cc941f0-watch/` including physical observation,
shipping sweep, saved provider state, parked checkpoint and title release.

## Idle retry after asynchronous paused-seek failure — 2026-10-10

A new R3 intent can release an old point-request guard once seek, discard and
prebuffer work are idle, permitting a retry when an accepted seek failed later
without moving a paused clock. Busy gates preserve the previous request. Provider
scope still rejects finished/stale/source/profile/live contexts; no automatic
retry or worker-thread provider mutation was added.

The demux seek flags are atomic. The getter observes pending/in-progress under
the same mutex that transfers a request, preventing an intermediate clear from
appearing idle. Success, failed seek, invalid timeline conversion and worker-busy
refusal release ownership only after hold/clock/pause restoration finishes.

Actual native provider/recovery/SponsorBlock executables, extracted shipping
player/global-dispatch/seek-overload paths and a threaded getter test passed
ASan/UBSan. The player test also queues a paused jump, rejects additional input
during seek/discard/prebuffer and permits a paused retry after those states clear.
Evidence: `.deps/highlight-idle-retry-{native,player,dispatch,seek-pause,ownership}`
logs/results (all 0), plus `.deps/highlight-idle-retry-player-final` (0).

The existing seek-handoff suite now extracts both actual controller overloads.
All four executables passed: request/clock ownership, resume timeline, decoded
audio acceptance and actual FFmpeg demux seeks with real playback hold/clock
functions. Cases cover paused failed seeks for audio-only and A/V sources, invalid
timebase, unparked worker refusal, clock-base retention and release ordering.
The first new paused-clock assertion failed because its fixture clock had never
started; that result (134) is retained. The corrected fixture asserts that pause
was established before submitting the request. Final evidence:
`.deps/highlight-idle-seek-handoff-final.{log,result}` (0) and
`.deps/highlight-idle-seek-ownership-final.{log,result}` (0).
The complete PS5 build passed all 211 objects, linking, packaging and immutable
freeze: `prosperotube-86ed6e62_1010-1607`, 126 files, eboot SHA prefix
`eca1964984193b69`. Evidence: `.deps/seek-idle-retry-ps5-build.{log,result}`
and `.deps/seek-idle-retry-freeze.{log,result}` (all 0). This candidate remains
undeployed; hardware acceptance of the new retry/status paths is pending.

## Atomic diagnostic status publication — 2026-10-10

The status publisher now stages `dev_status.tmp` under the same Lapy-backed app
directory, checks formatted output and close, then atomically renames it over
`dev_status`. Creation, write, close or rename failure keeps the previous complete
snapshot. Published/staging paths use distinct buffers, preserving rename
arguments and preventing a partial write from exposing a false park checkpoint.

`scripts/test_atomic_status.py` extracts the actual generated path resolver and
status function. Its ASan/UBSan run observes the published file while staging is
empty and partially written, injects each I/O failure and checks complete active
and parked publication. Passed evidence: `.deps/atomic-status-prepare.{log,result}`
and `.deps/atomic-status-test.{log,result}` (both results 0).

Committed `66014522` also passed all 211 PS5 objects, linking, packaging and
immutable freeze: `prosperotube-66014522_1010-1548`, 126 files, eboot SHA-256 prefix
`55786d4f8693fadf`. Evidence: `.deps/atomic-status-ps5-build.{log,result}` and
`.deps/atomic-status-freeze.{log,result}` (both results 0). This package remains
undeployed; the accepted `5cc941f0` package stays installed. No further console
launch followed the playback-control milestone.

A fresh read-only Windows extraction also verified the public Blender video
`aqz-KE-bpKQ` (Big Buck Bunny), duration 635 seconds, 53 formats and AVC choices
from 144p30 through 1080p60. The strict-UTF-8 worker returned successfully with
no stderr. Evidence: `.deps/windows-public-bunny-worker.result` (0),
`.deps/console/windows-public-bunny-summary.json` and private raw worker output.
This supplies a broader test candidate; it does not establish native PS5 decoding
or playback of that video.

## Current PS5 package: history, pairing, pause and seek — 2026-10-10

Native `5cc941f0` built all 211 objects, linked and packaged successfully, then
froze 126 files: `prosperotube-5cc941f0_1010-1529`, eboot SHA-256 prefix
`f5a6b083d7e0d59c`. Build/freeze evidence:
`.deps/highlight-retry-ps5-build.{log,result}` and
`.deps/highlight-retry-freeze.{log,result}` (both results 0).

The reserved-console case passed both initial idle checks, all installed-byte
checks before and after settling, registration and final idle check, then
launched exactly once. Matching startup, Lapy storage, UI and first frame passed.
The first adapter stopped because Down selected Followed channels in the
four-column root instead of History. A same-process continuation verified the
launched PID/app identity, corrected navigation and reloaded saved Guest history.
It then stopped on a strict status guard; the rejected bytes were not retained.
A read-only inspection of the same owned process established real public video
completion (`18.87/19`, 15 fps, fatal=0, EOF=1, complete=1). The status writer
currently truncates its file in place, making an empty read plausible; this is
not proof of the rejected read's exact contents. Both original results remain 1.

A bounded replay continuation in that same title process passed:

- Saved Guest history and stored pairing, without re-pairing.
- Actual YouTube AVC playback and native AAC initialization.
- Cross pause at 2.39 seconds, position held for two seconds, and resumed progress.
- Right seek from 3.58 seconds toward 13.58; sampled presentation reached 14.48.
  The matching engine recorded `seek_req=1 ok=1 fail=0`, first frame after seek
  551 ms, 133 published frames, zero late drops and 46 seek-discarded frames.
- EOF/completion at 18.90/19, Stop confirmation and return to History.
- Saved version-2 Guest record (`played=true`, position=0, duration=19).
- Matching GPU park, exact-title close, PID/app-bound kill/video-owner/mount
  release and reachable FTP/klog/elf-loader services; the owned lock was removed.

Evidence: `.deps/history-pause-seek-highlight-replay.{log,result}` (result 0) and
`.deps/console/history-pause-seek-5cc941f0-replay-continuation/`. Earlier failed
adapters and read-only inspection remain in the adjacent source-specific folders.
No second title launch, source rebuild or companion change occurred. Physical
sound/controller feel, broader formats/long sessions and SponsorBlock R3 point
actions on hardware remain unverified. Full SmartTube equivalence remains open.

## Highlight retry and engine pause ownership — 2026-10-10

Explicit highlight/review seeks now report whether the mailbox accepted the
request. A refused point request releases only its matching guard, allowing a
paused retry without moving the clock. An unrelated failure cannot release it.
Automatic skips cannot overwrite a pending explicit point request; arrival or
clock movement releases the guard. The automatic tick reads the engine pause
state so a stale controller FSM cannot initiate a skip while the engine is paused.
The legacy one-argument seek continues to resume scrubbing playback.

The three actual native provider/recovery/SponsorBlock executables, extracted
shipping player input/tick paths, combined global/provider/player dispatcher,
and extracted seek overloads all passed ASan/UBSan. Tests cover refusal return
values, paused retries, mismatched failure targets, competing automatic skips,
engine/FSM pause disagreement and invalid requests. Evidence:
`.deps/highlight-retry-{native,player,dispatch,seek-pause}.log` and matching
`.result` files (all 0). PS5 package and console acceptance follow separately.

## Explicit highlight jumps and paused seek intent — 2026-10-10

R3 in the player now selects the nearest future SponsorBlock highlight point
with silent/message/ask/disabled actions. Points remain zero-width markers;
they never create automatic intervals from zero to the highlight. Manual intent
ignores automatic minimum/repeat policy. Point review has its own start position,
survives before the target, expires at the point, and does not consume overlapping
interval review. Duplicate requests before seek clock movement are guarded;
finished/stale/source/profile/live contexts and invalid or endpoint targets are
rejected. Player R3 is routed through the global dispatcher; player L3 and
both sticks on other screens retain capture. Keyboard/browser ownership is kept.

An explicit pause flag now reaches the shipping controller's seek mailbox.
Highlight jumps and accepted reviews preserve the engine's paused state, including
request refusal; the one-argument scrub/legacy seek contract still resumes.
Confirmation cannot submit a competing seek during scrub/seek/prebuffer ownership,
while Circle can cancel review. Resolving a former live video as VOD now refreshes
its live flag before optional metadata eligibility is decided.

The final unchanged-source run passed the three shipping native provider,
recovery and SponsorBlock ASan/UBSan executables; extracted shipping player paths;
combined global dispatch/provider bindings/player highlight input; and both
shipping controller seek overloads with mocked mailbox/engine functions.
Coverage includes unsorted points, disabled colors, a two-second manual jump with
60-second automatic minimum, confirmation/decline, overlap, duplicate requests,
archive transition, source/profile invalidation, endpoint/non-finite input,
playback gates, pause on success/refusal and unchanged legacy behavior.
Evidence: `.deps/highlight-{native,player,dispatch,seek-pause}.log` and matching
`.result` files (all 0). New-package/physical controller acceptance remains pending.

Reference review used SmartTube `2c60453007dfc3775ff525c3837c2d1d4e95aab9`
SponsorBlockController/SponsorBlockData and MediaServiceCore
`4d9767733aa92269b9d24f24844a7fa1bb5196c1`
[YouTubeSponsorSegment](https://github.com/yuliskov/MediaServiceCore/blob/4d9767733aa92269b9d24f24844a7fa1bb5196c1/youtubeapi/src/main/java/com/liskovsoft/youtubeapi/service/data/YouTubeSponsorSegment.java)
and Segment. They preserve point coordinates; R3 is the explicit PS5 control
mapping. This does not establish complete SmartTube SponsorBlock parity.

## Provider stick controls survive global dispatch — 2026-10-10

The committed `d4d5b66d` PS5 candidate passed all 211 objects, linking, signed
containers, packaging and immutable freeze: `prosperotube-d4d5b66d_1010-1450`,
126 files, eboot SHA-256 prefix `2f50242646332878`. Evidence:
`.deps/controller-dispatch-ps5-build.log` / `.result` (0) and
`.deps/controller-dispatch-freeze.log` / `.result` (0). Console acceptance of
this new package remains pending.

The generated application now reserves L3/R3 for the provider screen rather
than consuming them for screenshot capture. Keyboard and system-browser input
also retain their buttons. Screenshot capture remains available on other
screens, including the player.

`scripts/test_controller_dispatch.py` extracts and compiles the shipping global
dispatch block together with the shipping YouTube provider stick bindings.
ASan/UBSan passed L3 composition, R3 discussion, Touchpad comments, L2/R2,
simultaneous-stick precedence, unchanged press/hold/release forwarding, screenshot
success/failure on player/settings, other-provider forwarding, keyboard/browser
ownership and idle input. The screen manager, capture and discussion callbacks
are mocked; physical pad/chat behavior and the new package remain unaccepted.
Evidence: `.deps/controller-prepare.log`, `.deps/controller-dispatch.log` and
`.deps/controller-dispatch.result` (0).

## First actual YouTube playback on PS5 — 2026-10-10

The fresh reserved `.40` case used unchanged native build
`prosperotube-b9b5c916_1010-1402` and corrected Windows companion `c7d6f35`.
Both idle checks, both full installed-byte verifications, settling, registration,
Lapy storage, UI and first frame passed. Native pairing and direct public lookup
returned `jNQXAC9IVRw` ("Me at the zoo"). The resident Sony AVC decoder opened
320x240 at 15 fps; native AAC initialized for stereo 44.1 kHz with an AudioOut
handle. Playback progressed from 0 to 18.87 seconds, reporting `eof=1`,
`complete=1`, `fatal=0`. All 133 sampled statuses had no decoder fatal; 130 were
active. Shipping engine stop statistics report 284 frames in/converted/published
and zero late drops. The 3840x2160 scanout capture visibly contains the video,
English captions and native OSD. Physical audible sound remains unverified.

The original adapter retains result 1: its final-position threshold required
within 0.10 seconds of metadata duration 19, while the last presented frame is
0.13 seconds short (two frames at this source's 15 fps). This is a verifier
failure, not evidence that playback failed. Independent read-only analysis of
the saved samples proves progress and final EOF/completion within 2.1 frames.
Evidence: `.deps/public-video-utf8-reserved-console.log`,
`.deps/console/public-video-utf8-b9b5c916-fallback/`, and
`.deps/verify-public-video-end.py`.

A bounded cleanup verified our launched process identity, requested quit,
proved matching GPU park, then sent one exact-title close. Matching kill,
video-owner removal and app-mount completion, idle titles and reachable services
passed. Saved version-2 pairing and Guest history contain this video with
`played=true`, `position=0`, `duration=19`. Evidence and capture:
`.deps/console/public-video-utf8-b9-cleanup/` (including `youtube-scanout.bmp`).
No account mutation or real chat message occurred. History reload, pause/seek,
other formats, physical audio/controller pacing and long sessions remain open.

## Windows public resolver encoding corrected — 2026-10-10

The first real public-video case on `.40` passed immutable installed-byte
checks, settling, final idle, Lapy storage and native UI startup. Pairing saved a
version-2 configuration under `/data/prosperotube`, but public search returned
`invalid_response`; no playback started. Original case result remains 1:
`.deps/public-video-reserved-console.log` and
`.deps/console/public-video-b9b5c916-fallback/`.

A separate bounded cleanup verified that only our launched PID 121/app ID
`0x8018` remained, requested graceful quit, proved matching `parked=1`, then
sent one exact-title close. Matching kernel kill/video-owner/mount-end records,
absence of running titles and reachable services passed. Evidence:
`.deps/console/public-video-b9-cleanup/`. This cleanup does not accept playback.

The shipping Windows worker reproduced the fault locally: exit 0, 105,427 output
bytes, invalid UTF-8 but valid CP1252 JSON for `jNQXAC9IVRw` with 24 formats.
The worker now explicitly configures protocol input/output as strict UTF-8,
independent of the Windows system code page. A real redirected-stream regression
round-trips Japanese, accented text and emoji with CP1252 initial streams and
keeps extractor output separate. All 18 resolver-boundary tests passed on Windows.
Fresh actual public extraction then returned valid UTF-8 JSON and 24 formats.
Console playback with this companion correction remains the next case; native
package bytes remain the accepted `b9b5c916` build.

## PS5 storage startup and first graceful close — 2026-10-10

Source `b9b5c916` passed all 211 PS5 objects, linking, signed containers,
packaging and immutable freeze: `prosperotube-b9b5c916_1010-1402`, 126 files,
eboot SHA-256 prefix `91e48da19d2a2c14`. Build and freeze result files are 0.
The reserved `.40` case verified all installed bytes twice before one launch.
Lapy observed one stopped thread and two credential references, completed the
credential/root transition and donor balance, and reported success. Required
storage, resident AVC/HEVC/VP9/HEVC10 creation, native UI and first frame passed.

The app reached `parked=1` before the exact-title close. The original adapter
retains result 1 because it required a ShadowMount release phrase absent on this
console. Captured kernel records instead bind PID 106/app ID `0x4018` to the
title's kill, video-owner removal and completed `EndAppMount`. A separate fresh
idle-first read-only case confirmed no title sandbox and reachable FTP, klog and
elfldr. Evidence: `.deps/lapy-warmup-reserved-console.log`,
`.deps/console/storage-lapy-warmup-b9b5c916-fallback/` and
`.deps/console/warmup-release-readonly-b9b5c916/`.

No preferences were changed; `youtube.json` was absent and a second launch was
not attempted. Configuration persistence across relaunch, actual YouTube
playback, physical sound/pacing and controller acceptance remain pending. The
next case's release predicate requires all three matching kernel records and
absence of the running title; positive/wrong-identity/missing-record offline
checks passed, as did initial-FTP/SELF-mode ordering checks.

Review also found that the application's screenshot handler consumes L3/R3
before provider screen input. Standalone input tests do not prove those physical
chat controls; dispatch integration remains to be corrected and tested.

## Native warmup before single-thread Lapy request — 2026-10-10

The complete diagnostic app build/package/freeze passed for source `146945b5`:
`prosperotube-146945b5_1010-0612`, 126 files, eboot SHA-256 prefix
`5c55d9800fcf741b`. Evidence: `.deps/lapy-stop-ps5-build.log` / `.result` (0)
and `.deps/lapy-stop-freeze.log` / `.result` (0).

The reserved fallback case passed idle-first installation, both complete byte
readbacks, settling, registration and final idle check. One launch again failed
the storage gate; the adapter stopped without input/close/relaunch and released
its owned lock. A separate idle-first read-only case retrieved the matching
diagnostic helper log. Evidence: `.deps/lapy-stop-reserved-console.log` /
`.result` (1), `.deps/console/lapy-stop-helper-readonly-146945b/`.
The 2,680-byte helper log SHA-256 is
`acbd341e849fef0404add54027e84c907a49deea34b49af61bcb5835ec89264d`.

Both stopped snapshots report `fd_refs=1`, `threads=92`, `suspended=92`,
`cred_refs=92`, matching proc/fd/credential/prison/root/jail/cwd and thread-list
identities, and `stable=1`. The helper requires `cred_refs=2` there and a
single-thread private credential later. Its `target_stop error=16` rejection
therefore precedes root/credential mutation, with `detached=1`. These values
establish the incompatible startup state, not which individual preload created
each worker. FTP retrieval passed after failure; comprehensive service-health
and clean app teardown acceptance remain absent.

The startup overlay now follows ProsperoLight's query-only VideoDec2 warmup
approach (`src/main.cpp`, `src/moonlight_stream.cpp` in the inspected reference):
load VideoDec2 and query compute/selected codec memory information before Lapy,
without creating compute queues, allocating decoder memory or creating resident
decoders. Cache that module result across the credential transition. Resident
video decoder and AGC/display creation now follow successful storage proof.
Existing audio-library and dialog preloads retain their ordering; a fresh
changed-candidate console case must establish whether they meet the remaining
single-thread precondition. This is not yet a proven startup fix.

The extracted generated preload body passed host ASan/UBSan for AVC-only,
secondary codecs and 10-bit configurations, checking query-only calls,
configuration fields, idempotence, failed module loading and optional query
failure. Actual generated application-order assertions verify that preloads
precede Lapy and display/resident creation follows it. Evidence:
`.deps/native-preload.log` / `.result` (0). The existing generated startup-log
gate regression also passed, preserving buffered logging before storage proof.
Sony module/decoder behavior remains mocked in these host tests.

## Reserved console startup and Lapy stop diagnosis — 2026-10-10

Frozen `prosperotube-58390ef6_1010-0550` passed the initial idle check,
126-file stored-byte upload/readback, 130-second settling, a second complete
readback, registration and final idle check on `192.168.4.40`. One launch reached
the matching build's `main()`. Lapy returned `apply_failed` (7), startup stopped
at the required `/data/prosperotube` gate, and the system recorded user-process
SIGSYS. The adapter stopped without input, close or relaunch and released its
owned shared lock. Evidence: `.deps/marker-storage-reserved-console.log` and
`.result` (1), `.deps/console/storage-marker-58390ef6-fallback/klog.txt`.

A separate idle-first, read-only case retrieved the matching helper log and
released its lock. Evidence: `.deps/console/marker-helper-readonly-58390ef6/`.
The 2,331-byte log SHA-256 is
`62501a31de0592c27689a4a66a82a59496148d7556f3cc0aa9b0253c6c70c944`.
Helper build `2daddf913d859dd6358295c6cf46573e7196ef478ccd67d3acec730b0860b5d9`
recorded firmware `12700001`, successful helper/root-layout initialization,
then `stage=target_stop error=16 roots_moved=0 cred_changed=0 detached=1`.
This establishes rejection before credential/root mutation. It does not identify
which stop predicate failed: credential references, stable snapshot fields,
private filedesc, and suspended thread counts are checked together. The log
does not prove the app was multithreaded or that a particular preload caused it.

The optional `PROSPEROTUBE_LAPY_STOP_DIAGNOSTICS=1` build adds a snapshot-field
record at that check using only already-captured snapshots. It builds in a
separate source copy, leaves the pinned checkout untouched, retains the exact
upstream return predicate and records upstream/compiled daemon hashes in the
packaged manifest. It requires the existing diagnostic result-file option.
It changes no stop, credential, root-transfer or retry policy. Console acceptance
of this new diagnostic variant remains pending.

The diagnostic helper compiled with the pinned PS5 SDK and upstream `-Werror`
flags: build `8d7cb08a28bac6d657bba735f959c0c1ca1cc1149da3055e83a9586f03a4ed6a`,
ELF SHA-256 `d4a67ecfe222cff5972f3555e76a6046b6b908096a4fccc474bd8b50be39031b`.
The verification case proved that removing the single inserted log block
reconstructs the exact original source, that the original stop predicate remains
once, that the pinned checkout is clean, and that packaged ELF/protocol/source
hashes match. Evidence: `.deps/lapy-stop-diagnostic-build.log` and
`.deps/lapy-stop-diagnostic-verify.result` (0). Python syntax and shell parsing
checks passed. The initial shell wrapper's result capture was malformed; its
exit status is not used as build evidence. The separately verified compiler
artifact and manifest establish helper build success, not console acceptance.

## SponsorBlock timeline markers and playback lifetime — 2026-10-10

Source `58390ef6` adds independent persisted timeline-color toggles for the nine
pinned SmartTube categories. Legacy settings default all marker colors on,
including filler while its automatic skip action remains disabled. Fetches
include the union of action and marker categories. Category RGB values match
the pinned SmartTube color resources.

The companion explicitly requests `skip` and, when selected, `poi` action types.
Highlight points must have equal start/end times and `poi` action type; ordinary
intervals must have positive duration and `skip` action type. This follows the
[API types](https://wiki.sponsor.ajay.app/w/Types) and
[hashed-prefix API](https://wiki.sponsor.ajay.app/w/API_Docs). Malformed top-level
payloads/entries, mismatched action/category pairs, boolean or out-of-range
timestamps and invalid point intervals are ignored. Highlight points do not
become automatic skip intervals; their jump/action behavior remains open.

The shipping PS5 UI-kit OSD and RML fallback draw the same normalized markers.
Intervals clip to known duration, and point markers remain visible inside both
ends of the track. Marker reads require the selected playing instance, profile,
source, master switch and exclusion policy; invalid/unknown duration and live
playback produce no markers. Source/profile changes invalidate old data, even
when switching back. Ordinary browsing-request cancellation no longer discards
the playing video's valid metadata, including a pending matching response.

The three actual native provider/recovery/SponsorBlock ASan/UBSan executables
passed: `.deps/sponsor-marker-native.log` / `.result` (0). The additional pending-
response cancellation assertion passed in the focused actual-provider run:
`.deps/sponsor-marker-policy.log` / `.result` (0). Tests cover default marker
fetches, disabled actions with visible markers, all nine settings rows, persisted
toggles/reload, point validation, clipping/capacity, playing-instance ownership,
profile switching and retained data across browsing cancellation. Extracted
shipping UI-kit and RML draw blocks passed ASan/UBSan with mocked draw-list/DOM
functions: `.deps/sponsor-marker-render.log` / `.result` (0). Point/interval
geometry, right-end containment, unchanged DOM reuse and clearing were checked.
The companion module passed 43 tests, with its inherited Starlette deprecation
warning: `.deps/sponsor-marker-companion.log` / `.result` (0).

All 211 PS5 objects, linking, signed containers, package and immutable freeze
passed: build `prosperotube-58390ef6_1010-0550`, 126 files, eboot prefix
`23101bbd1b1a7f4d`. Evidence: `.deps/sponsor-marker-ps5-build.log` / `.result` (0)
and `.deps/sponsor-marker-freeze.log` / `.result` (0). Bundled attribution matches
the source bytes. Its subsequent reserved-console launch failed at storage
initialization as recorded above; physical marker rendering, controller behavior
and playback remain unverified.

Separately, the prior `c758351c` candidate passed the first idle check on the
primary console, full 126-file stored-byte upload/readback, 130-second settling,
second full readback and title registration. A title started before the final
launch check, so no launch/input/close was sent. The owned lock was released.
Evidence: `.deps/sponsor-end-storage-primary-console.log` / `.result` (1).
This proves installation of those earlier bytes, not Lapy execution or playback.

## SponsorBlock skips to the known end — 2026-10-10

Source `c758351c` handles acknowledged automatic or confirmed skips reaching or
overrunning a known video duration. The player completes deliberately, stops
playback and returns to browsing; normal queue autoplay can select the next
item. It does not submit a demux seek to an unavailable endpoint frame. Natural
EOF still requires the existing decoder/output retirement proof. A subsequent
stop report cannot restore a resume position inside the skipped outro.

Completion authorization is bound to the resolved/playing instance, active
profile, optional-service scope and exact acknowledged target. Unknown/invalid
duration, stale resolutions, another playing video, live sources, declined
prompts and policy/exclusion changes cannot complete the item. Minimum duration
uses the playable remainder when segment metadata extends beyond the known end.

The three actual provider/recovery/SponsorBlock ASan/UBSan executables passed:
`.deps/sponsor-end-native.log` / `.result` (0). Tests cover end/overrun targets,
one-time completion with autoplay off, accepted/declined confirmation, queue
transition, watched/resume state after stop, stale contexts, exclusion changes
and clipped minimum duration. The generated PS5 `PlayerScreen` tick/input paths
were extracted and compiled under ASan/UBSan, with mocked playback/navigation/
pad functions: `.deps/sponsor-end-player.log` / `.result` (0). Those paths cover
silent/message actions, Cross/Circle precedence, stop/return versus in-video
seek, rejected endpoint completion, and pause/scrub/live/seek/buffering gates.
These tests do not establish physical controller behavior or native teardown.

All 211 PS5 objects, linking, signed containers, package and immutable freeze
passed: build `prosperotube-c758351c_1010-0527`, 126 files, eboot SHA-256 prefix
`fbaa644e95bda5b7`. Evidence: `.deps/sponsor-end-ps5-build.log` / `.result` (0)
and `.deps/sponsor-end-freeze.log` / `.result` (0). The frozen package's
`assets/prosperotube-notices/THIRD_PARTY_NOTICES.md` matches the committed source
byte for byte, including the SponsorBlock database/API attribution added after
the prior candidate. Older frozen candidates were not modified.

The reserved-console startup/storage case stopped at its first FTP title check
because a title was running. No other service probe, upload, launch, input or
close followed, and the owned lock was released. Evidence:
`.deps/sponsor-end-storage-reserved-console.log` / `.result` (1). The candidate
is undeployed/unlaunched. Lapy PR50 execution, durable storage and real YouTube
playback on PS5 remain unverified. SponsorBlock timeline colors/highlights,
paid-content notices and full settings/action/default equivalence remain open.

## SponsorBlock category policy — 2026-10-10

Source `1b0a0dca` adds eight skip-category actions: silent skip, skip with a
message, ask before skipping, and do nothing. Same-action overlapping intervals
merge; disabled categories and different actions do not extend that merge.
The category defaults follow pinned SmartTube `SponsorBlockData`: message skips,
filler disabled, five seconds minimum remaining skip, and repeat skipping allowed.
The existing opt-in master switch is retained. Persisted version-2 data gains
optional named category actions, minimum duration, no-repeat and excluded-channel
settings. Legacy files load defaults without losing their existing master flag.

The native player presents confirmation through its existing toast and Cross/
Circle controls; Circle wins if both buttons arrive together. Expired, canceled
and out-of-scope prompts cannot submit a seek. Buffering, seek preroll and live
sources do not trigger actions. Fetches use a distinct resolve serial plus
generation/profile guards, so delayed responses cannot replace another video's
segments. Exclusions are bounded to 128 channels; current/followed channels and
previous exclusions can be paged, toggled and persisted.

All three shipping native provider/recovery/SponsorBlock executables passed
ASan/UBSan: `.deps/sponsor-policy-native.log` / `.result` (0). The dedicated
SponsorBlock test drives actual catalog/settings, queued HTTP resolution and
callbacks, persistence/reload, mixed-category merging, minimum durations,
confirmation acceptance/decline, repeat guards, exclusions, live suppression,
invalid metadata and delayed responses. The companion test module passed 38
tests, including every requested category and boolean timestamp rejection:
`.deps/sponsor-policy-companion.log` / `.result` (0). Its inherited Starlette
deprecation warning remains.

A read-only live host case exercised the shipping companion endpoint against
the real [SponsorBlock API](https://wiki.sponsor.ajay.app/w/API_Docs). Two
four-character-prefix GETs returned HTTP 200 and JSON arrays for all eight
requested categories. A public record present in that response was selected
after fixed sample IDs returned no matching skips; the shipping filter returned
one valid segment. No account, mutation, media download or console was used.
Evidence: `.deps/sponsor-policy-public.json`, `.log` / `.result` (0). Empty sample
and invalid-ID diagnostics are retained separately; they were not accepted as
proof of nonempty filtering.

The complete PS5 build/link/package/freeze passed: build
`prosperotube-1b0a0dca_1010-0504`, 126 files, eboot prefix `b297f87e76c87752`.
Evidence: `.deps/sponsor-policy-ps5-build.log` / `.result` (0) and
`.deps/sponsor-policy-freeze.log` / `.result` (0). The reserved-console case
stopped at its first FTP running-title check. No other service probe, upload,
launch, input or close followed; the owned lock was released. Evidence:
`.deps/sponsor-policy-storage-reserved-console.log` / `.result` (1).

The candidate remains undeployed/unlaunched. On-console prompt behavior, timeline
colors/highlights, paid-content notices, precise outro-to-end behavior and full
settings/default equivalence remain incomplete or unverified. The new database
attribution notice was added after this build and must be bundled in the next
package; the `1b0a0dca` freeze remains immutable.

## Audio seek and EOF lifecycle — 2026-10-10

Sources `bf6852cd` and `c2779195` fix in-place audio-only seeks acquiring a
video presentation hold that no video frame could release. Audio discard also
covers zero and sub-50ms targets. Floating-point uncertainty at exact PCM sample
boundaries no longer trims an extra sample. Invalid native PCM byte counts,
channel counts and frame allocations mark audio failure; the player presents an
audio error before leaving playback.

EOF with an unreached audio seek target is no longer retired as successful
audio output. A video may nevertheless continue silently after an indexed audio
track has ended: known stream start/duration metadata is compared in the common
media timeline before allowing that case. Unknown duration/timing cannot excuse
an unreached seek. This is a bounded failure rule, not proof of parity for all
unknown-duration sources.

The actual generated EVO audio decoder loop, resampler, ring, EOF drain and
output code passed ASan/UBSan. Scripted native-decoder PCM proves all preroll
packets reach the decoder, retained sample contents are correct, counted output
excludes padding, invalid byte counts become fatal, and unreached EOF cannot
become drained. Sony Audiodec and AudioOut are mocked; native codec priming and
audible output remain unverified. Real host AAC decode/EOF and known-duration
audio-tail metadata checks also passed. Evidence:
`.deps/audio-seek-lifecycle.log` / `.result` (0).

The shipping demux request/process methods passed ASan/UBSan with actual FFmpeg
AAC/Opus/HLS seeking and the real `pp_playback` hold/clock. Tests cover audio-only
playing/paused seeks, zero/sub-50ms targets, nonzero origins, video discard and
failed-seek base retention. The presentation frames in this process test are
synthetic; actual AVC/AAC/Opus decode remains covered by the existing resume
tests in the same suite. Evidence: `.deps/audio-only-seek-process.log` / `.result`
(0). The Opus fixture retains its previously documented seek header warning.

The final `c2779195` PS5 build, link, packaging and freeze passed: 126 files,
eboot SHA-256 prefix `c09f0aff45b24d48`. Evidence:
`.deps/audio-seek-lifecycle-ps5-build.log` / `.result` (0) and
`.deps/audio-seek-lifecycle-freeze.log` / `.result` (0). The preceding
`bf6852cd` package also passed; its evidence is preserved under
`.deps/audio-seek-lifecycle-bf6852cd-*`. Neither candidate was deployed or
launched. This milestone made no console connection.

## Accurate resume — 2026-10-10

Source `c6ec8804` preserves decoder preroll while suppressing presentation
before the requested resume position. The initial video discard gate is armed
after `pp_playback_on_file_open`, before consumers start; audio-only playback
does not acquire a video hold. Audio packets reach the decoder normally, and
decoded PCM is discarded or trimmed before resampling. Both normal decode and
FFmpeg EOF drain use the same acceptance helper. Unknown initial audio timing
is bounded; malformed frames mark the existing audio failure state, which is
now included in the player's fatal query.

The comparison uses the pinned SmartTube source: `VideoStateController` restores
saved positions through `ExoPlayerController.setPositionMs`; ExoPlayer defaults
to `SeekParameters.EXACT`. Its optional TextureView sync-seek workaround is a
separate behavior and is not implemented by this change.

Shipping resume/UI/request methods and the audio acceptance helper passed
ASan/UBSan tests. Actual host AVC decode starts at the first frame at or after
4.25 seconds; actual AAC and Opus decode starts within one input sample of
4.250001 seconds. Interleaved/native-style S16 and planar S16 tests verify sample
contents, copy-on-write behavior, timestamp inference and bounded missing timing.
Audio-only resume, refused seeks and nonzero container origins are covered.
The Opus fixture emits a demux/decode header warning during seeking; the asserted
decoded-frame checks still pass. Actual AVC/VP9 playback/drain regressions also
pass. Evidence: `.deps/accurate-resume-test.log` / `.result` (0) and
`.deps/accurate-resume-drain.log` / `.result` (0). Host FFmpeg is 8.x; the PS5
recipe remains 7.1.1. These results do not prove native Audiodec timing or sound.

The complete PS5 build, link, packaging and freeze passed: build
`prosperotube-c6ec8804_1010-0423`, 126 files, eboot SHA-256 prefix
`370b64741f7917ae`. Evidence: `.deps/accurate-resume-ps5-build.log` / `.result`
(0) and `.deps/accurate-resume-freeze.log` / `.result` (0).

The reserved-console case stopped at its first minimal FTP running-title check
because a title was running. No other service probe, upload, launch, input or
close followed; the owned lock was released. Evidence:
`.deps/accurate-resume-storage-reserved-console.log` / `.result` (1). The frozen
candidate remains undeployed and unlaunched. Lapy PR50 execution, durable storage
and real YouTube playback on PS5 remain pending.

Host: Windows/WSL Ubuntu, LLVM 18.1.8, Python 3.14. Native implementation
revision: `ecacf92` plus the packaged-notices change. This record concerns
offline behavior and packaging, not console or live-service acceptance.

* Companion: 30 automated tests passed. One upstream Starlette TestClient
  deprecation warning remains; no test failed. Tests include authentication,
  pairing expiry/rate limits, stable catalog identities/link browsing, format
  eligibility, session bounds/expiry, range relay, credential scoping across
  redirects, caption timing, asynchronous jobs, cancellation and SponsorBlock.
* Shipping native provider: AddressSanitizer/UndefinedBehaviorSanitizer passed
  with leak checking. Registry isolation, persistence, local favorites/queue/
  follows/history, pairing/jobs, cancellation, origin rejection, chapters and
  transitive SponsorBlock overlaps are checked with mocked native HTTP.
* Real HTTP fixture probe: AVC/AAC DASH, VP9/Opus DASH and finite HLS each
  exposed both audio/video streams, decoded successfully, and passed a host
  seek/decode probe. Timed SRT captions and local PNG thumbnails passed.
  Detailed generated evidence: `output/validation/companion-media.json`.
* Shipping TV RML: host RmlUi/EVO renderer checked eight-card bounds, D-pad
  focus, stable selection, empty/loading/error states. PNG previews were
  inspected. RmlUi emits transient array-binding warnings when fixture rows
  shrink; the rendered states and selection assertions pass.
* PS5 packaging: all 210 native objects compiled; native app converted and
  signed containers inspected with valid integrity. MkPFS packed/verified the
  `.ffpfsc`; folder-title ZIP was generated. Final artifacts/checksums are in
  `dist/`. The app uses `PPSA99142` and independent storage, with original art.

No deployment, console launch, PS5 frame/audio measurement, Google login,
live YouTube extraction or live SponsorBlock call was performed. Follow
HARDWARE_ACCEPTANCE.md before describing the app as hardware-validated.

## PS5 parity integration — 2026-10-09

On the PS5-only branch at `7c63593`, 41 Python tests, 14 Node tests, and the
native provider ASan/UBSan suite with leak checking passed. Library coverage
includes profile isolation, modern cards, bounded continuation and replies;
native mocked HTTP covers profile/feed/playlist navigation. These are offline
checks and do not establish live account behavior or full SmartTube parity.

Candidate `bc3a686` compiled 210 PS5 objects and passed native container and PFS
packaging checks with development controls enabled. Its commands, status and
logs use the application's own data directory. Normal builds keep development
controls disabled. The exact candidate is frozen separately from release output;
console acceptance is recorded in HARDWARE_ACCEPTANCE.md.

## Audio tail completion — 2026-10-09

Revision `0af336d` adds bounded FFmpeg decoder and resampler EOF flushing,
counted padding for the final PCM block, and an epoch-scoped audio drain signal.
The actual generated EVO audio implementation and playback clock were linked
with host FFmpeg 8 and exercised under ASan/UBSan. The cases passed for real AAC
decoder EOF, a 44.1 kHz resampler tail, partial-block padding, output failure,
stale seek epochs, producer parking during seek, and video continuing after the
audio clock stops. Evidence: `.deps/audio-drain-current.log`.

Natural demux EOF is now distinguished from failed input recovery. Known-duration
queue completion additionally requires audio drain. These tests mock AudioOut
and do not prove audible hardware output, video/presentation drain, or completion
of unknown-duration streams. Those remain explicit acceptance and implementation
gaps; the full SmartTube parity goal remains open.

The PS5 build passed compilation of 211 objects, linking, native container
validation and PFS/ZIP packaging. Candidate `0af336d4-audio-drain` is frozen
with 126 files and eboot SHA-256 prefix `374108a2168bc7cb`; its embedded build ID
is `prosperotube-0af336d4_1009-1854`. Evidence:
`.deps/audio-drain-ps5-build.log`. No console deployment or launch was performed.

## Decoder EOF and final-frame retirement — 2026-10-09

Revision `5b98769b` makes decoder EOF distinct from starvation and failure. The
native flush resumes after reorder-buffer backpressure and requires a successful
empty flush before reporting drained. Its unchanged generated flush body passed
sanitizer cases for terminal EOF, backpressure, errors and bounded runaway output
with Sony calls mocked. The actual EVO decode loop, FFmpeg AVC/VP9 decoders,
presentation clock and locked frame snapshot passed sanitizer cases for buffered
tail frames, unknown duration, final-frame retirement and display interval,
audio/input failure gates, post-EOF decoder error, held-frame timestamps, and
stale acknowledgments after seek/reopen. Evidence:
`.deps/video-drain-snapshot-test.log`. Audio and provider regressions also passed
in `.deps/video-drain-audio-regression.log` and
`.deps/video-drain-provider-regression.log`.

The renderer now acknowledges the snapshot's epoch/serial only after a successful
video draw, GPU fence retirement and completed VideoOut flip. Automatic queue
completion requires natural input EOF, audio drain, decoder drain and retirement
of the final published frame for its display interval. It no longer relies on
metadata duration. Development status exposes `complete=` separately from raw
`eof=`. The host injects acknowledgment at the presentation boundary; actual Sony
flush, audible AudioOut and GPU/VideoOut retirement still need PS5 acceptance.

Follow-up `0a2167c` declares the seek reset through the playback header and latches
publication/conversion failure so an unpublishable tail cannot complete playback.
The strengthened actual-engine sanitizer case passed in
`.deps/video-drain-publication-test.log`. The initial PS5 compile exposed the
missing demux declaration; that failed build was not deployed.

The corrected PS5 build passed compilation of 211 objects, linking, native
container validation and PFS/ZIP packaging. Candidate `0a2167ce-video-drain` is
frozen with 126 files and eboot SHA-256 prefix `bafde456039f717d`. Embedded build ID:
`prosperotube-0a2167ce-dirty_1009-1916`; the suffix reflects pending validation
documentation only, with shipping sources committed at `0a2167ce`. Evidence:
`.deps/video-drain-ps5-fixed-build.log`.

The bounded storage/startup case's first console operation found a running title
and stopped before service probes, upload, launch, close or input. Its owned lock
was released. Evidence: `.deps/console/storage-0a2167ce/`. This candidate remains
undeployed; Lapy storage/relaunch and the new playback gates remain unverified
on PS5.

Device authorization backend validation: 89 Python tests and 22 Node tests passed.
Coverage includes authenticated API privacy, polling intervals/slow-down, expiry,
single-flight behavior, cancellation while queued or in flight, rejection of
late credentials, atomic persistence, malformed file preservation, concurrent
writer rejection, dynamic profile registration and private refresh updates.
Pinned YouTube.js TVHTML5 session and refresh paths are exercised with mocked
network responses. Evidence: `.deps/auth-verified-python.log`,
`.deps/auth-final-node.log`, `.deps/auth-worker-build.log`. Follow-up asynchronous
job cases passed in `.deps/auth-jobs-full-python.log`, including nonblocking
requests, delivered/abandoned start ownership and late responses after DELETE.
The first backend commit inadvertently included unfinished job edits while its
shell was still committing. The follow-up corrects these edits and passed the
full suite; the intermediate revision was not deployed.

The explicit public probe obtained a real YouTube device-code response, canceled
locally, stored no credentials and closed its worker. It never polled the token
endpoint, authorized an account or contacted the console. Evidence:
`.deps/auth-device-public-probe.json` and `.deps/auth-device-public-probe.log`.
This establishes public initiation only; real sign-in, private TV catalogs,
OAuth playback and native sign-in acceptance remain pending. No native sources
changed in this stage, so the frozen PS5 candidate remains `0a2167ce`.

The next native sign-in stage passed actual shipping provider ASan/UBSan cases:
pairing/job initiation, automatic bounded polling/single-flight, stable pending
revision/focus, authorized profile selection, cancellation during initiation or
polling, late original-source cleanup, expiry/denial, malformed URL/interval
rejection and absence of codes/session references/tokens in saved PS5 state.
Evidence: `.deps/auth-native-verified-test.log`. Existing provider regressions
passed in `.deps/auth-native-regression.log`. The updated shipping RML rendered
on the host, with sign-in reader/footer visibility and cancel-row activation
checked; `output/preview/device-sign-in.png` was inspected. Evidence:
`.deps/auth-native-final-preview.log`. This proves offline behavior/layout only;
PS5 and real account authorization remain unverified.

Native sign-in candidate `a07e1733` compiled 211 PS5 objects and passed linking,
native container validation and PFS/ZIP packaging. Frozen candidate:
`a07e1733-device-auth`, 126 files, eboot SHA-256 prefix `0b9789b1b61704c4`;
clean build ID `prosperotube-a07e1733_1009-1957`. The Lapy helper remains packaged.
Evidence: `.deps/auth-native-ps5-build.log`, `.deps/auth-native-freeze.log`.

The bounded storage/startup case's first console operation found a running title.
It stopped before probes, upload, launch, close or input and released its owned
lock. Evidence: `.deps/console/storage-a07e1733/` and
`.deps/console/storage-device-auth-run.log`. The candidate remains undeployed;
Lapy storage/relaunch, playback gates and native sign-in require PS5 acceptance.

OAuth resolution adapter: 95 Python and 24 Node tests passed. New cases exercise
the actual pinned yt-dlp TV player request/context, authenticated bounded transport,
redirect/origin rejection, TV script selection, normalized AVC/AAC adaptive formats
and DASH capability creation; they use synthetic responses and no real account.
Profile/cache isolation, TV search continuations, near-expiry token refresh and
private preflight are covered. A real bundled Node worker runs with external fetch
disabled; its private credential result is verified and HTTP attempts to access
that operation are rejected. Evidence: `.deps/oauth-playback-verified-python.log`,
`.deps/oauth-playback-verified-node.log`, `.deps/oauth-playback-worker-build.log`.

The explicit public TV player probe obtained LOGIN_REQUIRED with zero formats
for `jNQXAC9IVRw`. It made one public API request, with no account authorization,
token polling, media download or console traffic. Evidence:
`.deps/tv-player-verified-public-probe.json` and its matching log. This does not
verify account-only streams or current TV signature challenges. No native sources
changed in this stage; the frozen native candidate remains `a07e1733` and undeployed.

The follow-up TV timestamp correction exercises the actual pinned timestamp
parser/cache with distinct synthetic TV variants and mismatching WEB/config/
caller override timestamps. The actual TV player request is checked for the
matching script timestamp in its playback context. Missing script timestamps
remain missing instead of using WEB STS. This is identity/request regression
coverage, not live cipher verification. Evidence:
`.deps/oauth-tv-timestamp-test.log` and `.deps/oauth-tv-timestamp-full-python.log`.

TV evaluator follow-up: 100 Python tests passed, including real WSL/Linux
resolver/evaluator-descendant cleanup on cancellation, timeout and success.
The shared slot is released, failures stay uncached, and a descendant cannot
outlive its isolated worker group. Public probe transport tests cover anonymous
GETs, exact TV origins/paths, response bounds and redirect rejection. Evidence:
`.deps/tv-evaluator-process-verified-test.log`, `.deps/tv-evaluator-full-python.log`.
The Node library implementation is unchanged; its 24-test suite remains passing.

`scripts/probe_tv_signature.py` fetched the real public TV configuration using
the pinned TV user agent, then its matching 2,452,196-byte `tv-player-ias.js`.
The actual pinned timestamp parser and Node/EJS evaluator passed synthetic
signature and `n` challenges. Script SHA-256 prefix: `276ad984607a69bb`.
No account authorization, token poll, Bearer header, media download or console
traffic occurred in this probe. Evidence: `.deps/tv-signature-public-probe.json`
and `.deps/tv-signature-public-probe.log`. This is compatibility evidence for
that script with synthetic inputs, not real signed-media or account playback.

- 2026-10-09 | Lapy | a07e1733 | PS5 6.02 | no-run: first title check busy; no further operations; owned lock released | .deps/console/storage-a07e1733-second/ | storage/startup acceptance

Account removal: 109 Python tests passed. Coverage includes private store reload,
foreign lock/write preservation, post-replacement directory-sync uncertainty,
real bundled Node retirement with fetch disabled, resolver caches and real child
process invalidation, queued extraction rejection, stale authorization/job
results, removal jobs retaining their own confirmation, relay capability/active
stream invalidation without an open-stream TTL regression, peer/Guest availability,
fixture API behavior and shutdown reaping of discarded pending jobs.
Evidence: `.deps/account-remove-full-python.log`,
`.deps/account-remove-verified-focused-python.log`.

Native ASan/UBSan cases passed for full review, one-time removal, profile-bound
confirmation, mismatching confirmations, Guest reset, persisted selection and
authoritative missing-profile reconciliation. Malformed/Guest-missing lists
preserve selection. Existing provider regressions passed. Evidence:
`.deps/account-remove-native-verified-test.log`,
`.deps/account-remove-native-regression.log`. The shipping RML removal review
rendered on the host and was inspected; evidence:
`.deps/account-remove-native-preview.log`, `output/preview/account-removal.png`.
These establish offline implementation only; PS5 and real-account acceptance
remain pending. The Node service source is unchanged, with 24 tests previously
passing; account removal is serialized in the Python companion.

Account-removal PS5 candidate `0e75c5ea` compiled 211 objects and passed linking,
native container validation and PFS/ZIP packaging. Its clean build ID is
`prosperotube-0e75c5ea_1009-2106`. The immutable 126-file candidate is stored at
`/home/denis/.cache/prosperotube/candidates/0e75c5e-account-remove/PPSA99142`,
with eboot SHA-256 prefix `862761034df260c3`. The Lapy helper and notices are
packaged, and PS5 storage remains `/data/prosperotube`. Evidence:
`.deps/account-remove-ps5-build.log`, `.deps/account-remove-freeze.log`.
No console operations occurred in this stage; this candidate remains undeployed.

OAuth host-probe follow-up: 111 Python tests passed. The existing loopback
API/relay/FFmpeg probe now accepts an explicitly configured OAuth profile and
private companion store. Shipping API tests verify private credential preflight,
profile routing, Guest separation, unavailable-profile rejection and response
privacy. Missing/incomplete/Guest selections fail, corrupt stores remain intact,
and nonexistent paths do not create empty state. Evidence:
`.deps/oauth-probe-test.log`, `.deps/oauth-probe-full-python.log`.
These tests use synthetic account credentials and media metadata; no real
authorization, upstream account playback, media download or console traffic
occurred. Native sources are unchanged from the frozen `0e75c5ea` candidate,
whose complete checksum manifest also passed verification.

Comment ratings: 116 Python and 26 Node tests passed. Shipping API/job fixture
cases cover comment/reply rating capabilities, Guest/peer isolation, removal,
changed-intent rejection, expiration and cursor-kind separation. Actual Python
subprocess cases prove confirmed-result reuse and uncertainty retention across
timeout, cancellation, mismatching responses and worker retirement. The pinned
CommentView/NavigationEndpoint/HTTP client sends the retained synthetic command
with TVHTML5 OAuth context; opaque capability responses exclude command keys.
Evidence: `.deps/comment-rating-python-test.log`,
`.deps/comment-rating-full-python.log`, `.deps/comment-rating-node-test.log`.

Shipping-provider ASan/UBSan regressions passed for full comment review,
one-time submissions, mismatching confirmations, rating removal and stale-profile
contexts. The shipping RML rendered; comment-rating review and the updated
discussion footer were inspected. Native sign-in/removal regressions also passed.
Evidence: `.deps/comment-rating-native-verified.log`, `.deps/comment-rating-auth-regression.log`,
`.deps/comment-rating-preview.log`, `output/preview/comment-rating.png`.
No real account mutation or console traffic occurred; live vote readback and
PS5 controls remain unverified.

Comment-rating candidate `0b5f0712` compiled 211 PS5 objects and passed linking,
native container validation and PFS/ZIP packaging. Its clean build ID is
`prosperotube-0b5f0712_1009-2129`. The immutable 126-file candidate is
`/home/denis/.cache/prosperotube/candidates/0b5f071-comment-ratings/PPSA99142`,
with eboot SHA-256 prefix `5487ccd4e7593cd7`; the packaged Lapy helper remains
included. Evidence: `.deps/comment-rating-ps5-build.log`,
`.deps/comment-rating-freeze.log`. The bounded console case's first title check
found a running title and stopped immediately, before probes, upload, launch,
close or input; its owned lock was released. This candidate is undeployed.

- 2026-10-09 | Lapy | 0b5f0712 | PS5 6.02 | no-run: first title check busy; owned lock released | .deps/console/storage-0b5f071/ | storage/startup acceptance

### Live HLS relay and idle capabilities (2026-10-09)

The current public NASA Space Station stream `awQzjn72bI0` passed the shipping
companion pairing/resolve/relay path with `--transport hls --expect-live`. Host
FFmpeg found H.264 video and AAC audio and decoded two seconds at the live edge.
Evidence: `.deps/live-split-public-youtube.json` and its matching log. This was
anonymous public YouTube traffic with no account authorization or console traffic.
The earlier candidate `DfEr5XCFNWM` failed resolution; the first current-stream
case returned `unsupported_media` because split audio/video HLS was not paired.

The pinned yt-dlp master parser now retains explicit audio codec/group relations
from the original response. A second master fetch was rejected as an approach
because refreshed signed URLs did not exactly match the first response. The
shipping adapter does not add that fetch, infer AAC from format IDs, or pair
unrelated manifests. It applies to public/cookie and TV OAuth extractors; real
OAuth live-stream acceptance remains pending.

Active delivery renews the 30-minute idle capability lease. Rolling HLS resource
registries retain the current graph and retire older unreferenced windows at
capacity; invalid/oversized replacements leave the previous graph intact.
Sixteen focused tests passed, including the actual pinned HLS parser, synthetic
rolling windows, shipping HTTP lease renewal, OAuth regressions and real worker
process-group cleanup. The cleanup test's marker is now published atomically to
avoid observing an empty file during the timeout case. Evidence:
`.deps/live-split-focused.log`. Signed upstream URL renewal, long-duration/DVR
behavior and PS5 decoding/audio remain separate acceptance gates.

The final full Python suite passed 121 tests (one known Starlette deprecation
warning): `.deps/live-split-full-python.log`. Native sources were unchanged, so
this companion revision did not produce or deploy a new PS5 package.

### Request-driven live-chat service (2026-10-09)

The service reader now opens top/live chat, polls at the server interval, applies
message replacement and moderator item/author deletions, and closes all references
to a context. Messages and continuations are bounded and profile-isolated. This
is a service implementation; native display, replay timing, posting and complete
scrollback remain unfinished.

All 29 JavaScript tests passed using the installed Windows Node runtime, including
the actual pinned InnerTube chat HTTP transport and parser. They exercise literal
text and paid messages, normalization/privacy, bounded snapshots, cached retries,
early-poll suppression, mode selection, failed updates, capacity, expiry and
close. Evidence: `.deps/live-chat-node-windows.log`. The earlier WSL test run
passed 28 tests before the paid-message/snapshot case was added. The rebuilt
worker passed bundling and `git diff --check` (`.deps/live-chat-build.result`).

The full Python suite passed 122 tests, including authenticated API validation,
asynchronous chat jobs, polling and closure: `.deps/live-chat-full-python.log`.
The first API test expected HTTP 202; it was corrected to the shipping job API's
HTTP 200 contract. A WSL connection timeout prevented the first public probe from
starting; the VM later responded without a forced restart.

Anonymous public chat probes for `awQzjn72bI0` and site-linked `X4VbdwhkE10`
returned `chat_unavailable`/404 at open. These are negative service results, not
chat success or proof that the service works for all current chat layouts.
Evidence: `.deps/live-chat-public.json`, `.deps/live-chat-lofi.json`. No console
traffic, account authorization, chat posts or message-menu actions occurred.

Resolving the official channel's `/@LofiGirl/live` URL returned current live
video `1-LpQekNa9g`. Its anonymous top-chat service probe passed open, one
server-paced poll and close, with 24 messages in both returned snapshots and
`ended: false`. Evidence: `.deps/live-chat-current.json` and matching log. Only
counts, IDs and stable result metadata were persisted; this is public host
service acceptance, not native display, OAuth chat or PS5 acceptance.

The same verified-current video also passed explicit `--mode live`: open,
server-paced poll and close each completed, both snapshots contained 24 messages,
and the chat remained active. Evidence: `.deps/live-chat-live-mode.json` and log.

### Native recent live-chat reader (2026-10-09)

The native C reader passed ASan/UBSan checks for pairing, direct library polls,
server intervals, single-flight requests, 24-message bounds, stable message
identities, mode changes, ended/failed states, old-source late cleanup and saved
state privacy. Evidence: `.deps/native-chat-test.log`. The full Python suite
passed 124 tests, including abandoned chat-job cleanup and rolling profile-bound
offline fixtures: `.deps/native-chat-full-python.log`.

The actual generated EVO RequestPage, RefreshChat, ItemsCallback, ApplyItems and
focus methods passed a host integration check with the shipping C reader and
RML. It moves selection while a request is pending, retains completion-time
selection/scroll, handles selected-message removal, switches top/live mode and
closes chat on setup. Literal text cannot create markup elements. Evidence:
`.deps/chat-ui-test.log`. The test uses mocked HTTP/time and a host focus/render
pump; it does not establish console controller timing or rendering acceptance.
Replay, posting, complete scrollback, real account chat and PS5 chat remain
unverified or unfinished.

Existing provider and device-auth/removal sanitizer regressions passed:
`.deps/chat-native-regression.log`, `.deps/chat-auth-regression.log`.
The shipping RML preview passed its binding, navigation, literal-text and
reader-scroll assertions; `output/preview/live-chat.png` was inspected for
reader and chat-footer visibility. Evidence: `.deps/chat-preview.log`.

Candidate `30a1bef8` compiled 211 PS5 objects and passed linking, native container
validation and PFS/ZIP packaging. Clean build ID:
`prosperotube-30a1bef8_1010-0015`. The frozen folder contains 126 files, with
eboot hash prefix `b92428ceaf99e560`, under
`candidates/30a1bef8-native-chat/PPSA99142` in the local build cache. Evidence:
`.deps/native-chat-ps5-build.log`, `.deps/native-chat-freeze.log`.

The bounded Lapy storage/startup case found an active title on its first console
operation, the minimal read-only FTP running-title check. It stopped before
service probes, upload, launch, close or input, and released its owned lock.
The candidate remains undeployed. Evidence: `.deps/native-chat-console.log`.

- 2026-10-09 | native chat / Lapy storage | 30a1bef8 | PS5 | no-run: first title check busy | .deps/native-chat-console.log | continue offline, await idle console

### VOD expired-link renewal (2026-10-09)

The shipping relay now performs one cache-bypassing, same-profile resolution
after an expired VOD HTTP/DASH request. It preserves session links/manifests,
atomically maps exact format roots, and shares representation observations with
older in-flight responses. Resumed ranges verify strong HTTP identity, total
size, requested bounds and content coding before any renewed body is forwarded.
The policy and remaining paths are detailed in `docs/PROTOCOL.md`.

The full Python suite passed 146 tests: `.deps/media-refresh-python.log`.
Coverage includes ETag/Last-Modified/Latin-1 tag handling, first-open renewal,
both-track mapping, concurrent single-flight requests, cooldown, cancellation,
profile invalidation, changed/ambiguous formats, changed duration/video/entity,
ignored/wrong/oversized ranges and repeated upstream expiry. Actual OAuth
preflight/base-extractor tests verify fresh extraction bypasses its cache while
retaining the selected profile and private token IPC.

Public video `jNQXAC9IVRw` passed AVC/AAC and VP9/AAC DASH cases at 240p: actual
API pairing/resolution/ffprobe, one local injected expired-link response, exactly
one fresh resolution, then real audio/video decode and seek through the same
relay URLs. Evidence: `.deps/media-refresh-public.json`, matching log, and
`.deps/media-refresh-public-vp9.json`, matching log. The reports explicitly mark
the simulated expiry. These are host proofs with real YouTube media; naturally
expired links, uninterrupted mid-playback continuity, OAuth media and PS5 recovery
still need acceptance. Live HLS renewal and controlled native restart remain
unfinished. No native source/build or console traffic changed in this stage.

### Native retry exhaustion classification (2026-10-09)

Repeated premature EOF responses could previously pass the natural-EOF check
after the demuxer's ten read-recovery attempts. The generated engine now
publishes one atomic input state (active, natural EOF, terminal failure).
Exhausted retries, other terminal read errors, and demux packet-allocation
failure cannot authorize completion. Open/seek and newly received packets reset
the input state through the existing reset path.

`scripts/test_video_drain.sh` passed ASan/UBSan checks. The AVC and VP9 tests
extract the exact generated demux classifier and retry-finality function,
exercise premature EOF through the retry limit, and feed their classification
to the real decode/render completion gate after final-frame retirement.
EOF with a latched I/O error, exhausted EOF with and without a latched error,
ordinary read failure, allocation failure, reset, and clean EOF are covered.
Evidence: `.deps/input-failure-test.log`, exit result 0. This proves host
classification and completion behavior; controlled native restart and PS5
network interruption, queue/watch-state and sound acceptance remain pending.

- 2026-10-09 | retry exhaustion | 0f961373 | PS5 build | pass: 211 objects, linking, native containers, PFS/ZIP; frozen 126 files, eboot 3edf184947505bd1 | .deps/input-failure-ps5-build.log; .deps/input-failure-freeze.log | controlled restart, PS5 acceptance

Clean build ID: `prosperotube-0f961373_1010-0043`. Both build and freeze returned
0; the candidate remains undeployed. No console connections were made.

### Controlled native restart (2026-10-09)

The native player now routes terminal input failures through the existing
stop/resolve/launch lifecycle, with a RAM-only, same-video/profile/source/token
recovery context. Each explicit selection permits three restarts. The provider
retains fractional VOD position, caption visibility and the selected format;
live reopens use the live edge. Paused, scrubbing and seeking playback waits.
Missing/ambiguous replacement formats fail rather than opening another quality.
Cancellation and scope changes invalidate pending recovery. A changed context
after open stops the new session before navigation. Open failure returns to the
browser; decoder fatal errors retain their independent stop path.

Full Python suite: 147 passed, one known Starlette deprecation warning.
`scripts/test_native.sh` passed original provider and recovery ASan/UBSan
regressions. Shipping provider/HTTP jobs verify `fresh` resolution, exact
fractional resume, live-edge position, unique format matching, queue retention,
budget retention across reopens, cancellation and profile/source invalidation.
Evidence: `.deps/recovery-python.log`, `.deps/recovery-native.log`; both exit 0.
The Python API test verifies fresh dispatch through both direct and job routes.
These are offline proofs; hardware restart,
caption/audio/position continuity, controller feel and network interruption
acceptance remain pending. No console traffic occurred.

The exact generated resolve callback, launch worker, pending record and
post-open context guard also passed a host ASan/UBSan regression. The test uses
real pthread creation and joins, with mocked playback-engine/UI boundaries.
It verifies stale callback rejection, selection of the matched choice, fractional
resume, live-edge restart, headers, caption restoration, failed open and stopping
an opened session after context invalidation. Reproduce with
`bash scripts/test_recovery_handoff.sh`. It does not prove actual PS5 teardown,
demux seek, audible output or physical controller behavior.

- 2026-10-09 | native recovery | 17f1b904 | PS5 | pass: build/package/freeze, undeployed | .deps/recovery-ps5-build.log; .deps/recovery-freeze.log | interruption/continuity acceptance

All 211 PS5 objects, linking, native container inspection and PFS/ZIP packaging
passed. Clean build ID `prosperotube-17f1b904_1010-0100`; immutable candidate
contains 126 files with eboot SHA-256 prefix `e0c8e041e3dc43c4`. Build and freeze
both returned 0. The exact-title Lapy helper is packaged and verified; no console
connections occurred during the offline build/test. Host handoff evidence:
`.deps/recovery-handoff.log`, `.deps/recovery-handoff-promoted.log` (both exit 0).

- 2026-10-09 | recovery / Lapy startup | 17f1b904 | PS5 | no-run: first title check busy | .deps/recovery-console.log | continue offline

The bounded storage/startup case stopped at its first console operation, the
minimal read-only FTP running-title check. No further service probes, upload,
launch, close or input occurred. Its owned lock was released. The candidate
remains undeployed; PS5 recovery and storage acceptance remain pending.

### Paired live HLS root renewal (2026-10-09)

The shipping relay now renews expired live playlist roots under the owning
profile, retaining local manifest/capability URLs. Exact unique format identity
and the refreshed audio-group/master pairing are checked before atomic root
replacement. Segments, keys and initialization objects are not silently
remapped. Dynamic roots omit byte ranges and immutable VOD validators. Late
responses cannot restore the previous root URL.

Full Python suite: 160 passed, one known Starlette deprecation warning; result
0 in `.deps/live-refresh-python-final.result`, log
`.deps/live-refresh-python-final.log`. Tests exercise shipping GET/HEAD relay,
fresh scoped headers, stable manifests, concurrent failures, cooldown, changed
formats/pairing, late responses, cancellation and removed-session rejection.

Real public YouTube NASA stream `awQzjn72bI0`, AVC/AAC split live HLS at 240p,
passed pairing, resolution, ffprobe and two-second audio/video live-edge decode
after one locally injected root 403 and exactly one fresh resolution through
the unchanged local relay manifest. Evidence: `.deps/live-refresh-public.json`,
`.deps/live-refresh-public.log`, `.deps/live-refresh-public.result` (0).
Reproduce with `scripts/probe_youtube.py --transport hls --expect-live
--simulate-hls-expiry` and the required video/runtime options in BUILD.md.
No account profile or console traffic was used. This is host renewal evidence;
natural expiry, long-session continuity, child-resource/DVR recovery, audible
audio and PS5 playback remain separate acceptance requirements. Native sources
are unchanged; the frozen recovery candidate remains undeployed.

### Sustained decode across live root expiry (2026-10-09)

The probe now checks the final FFmpeg decoded timestamp instead of accepting
exit status alone. Missing/malformed progress and early successful endings fail.
An optional delayed injection forwards the first root requests normally, then
expires exactly the requested playlist refresh during the same decode process.
Full Python suite: 167 passed, known Starlette warning only; evidence
`.deps/live-sustained-python.log` and `.deps/live-sustained-python.result` (0).

The real public NASA AVC/AAC split HLS case passed with `--decode-seconds 60
--simulate-hls-expiry --hls-expiry-after-requests 3`: final decoded timestamp
60.01 seconds, exactly one injected expiry on root request three and one fresh
resolution. The same FFmpeg process used the unchanged local manifest throughout
the decode. Evidence: `.deps/live-sustained-public.json`,
`.deps/live-sustained-public.log`, `.deps/live-sustained-public.result` (0).
This extends the two-second startup-renewal case to an ongoing host decode;
it does not establish PS5 pacing/audio, natural signed-link expiry, 30-minute
lease continuity or full DVR behavior. No account profile or console traffic.

### Companion live-chat message intents (2026-10-09)

Added account-only `chat_send` with an owned active chat cursor, exact video ID,
unique intent and bounded Unicode text. The actual pinned YouTube.js sender
creates the request; one matching returned text-message action is required for
confirmation. No automatic reader starts and no optimistic message is inserted.
Companion guards survive Node loss, timeout and cancellation without replaying
an uncertain send; changed text/video/cursor cannot reuse an intent. Confirmed
retries return allowlisted metadata. Native compose/review/cancel and real-account
posting acceptance remain pending. No real messages were sent.

Full Python suite: 172 passed, known Starlette warning only; Node worker suite:
31 passed; worker bundle build: pass. All three returned 0. Evidence:
`.deps/chat-send-python.log`, `.deps/chat-send-node.log`,
`.deps/chat-send-bundle.log` and corresponding `.result` files. Tests use the
shipping direct/job APIs, actual worker subprocess loss, and the actual pinned
sender/parser with fetch replaced. Unicode request text, WEB request shape,
profile/video ownership, one-time confirmation, blank/control/oversized text,
worker restart, timeout and cancellation are covered. These are offline proofs,
not authorization or YouTube acceptance of an actual account message.

- 2026-10-09 | recovery / Lapy startup | 17f1b904 | PS5 | no-run: current first title check busy | .deps/storage-current-console.log | continue offline

The current frozen recovery candidate case again stopped at its first read-only
running-title check. No other probe, deployment, launch, close or input occurred;
the owned lock was released. The frozen candidate remains undeployed.

### Native chat compose/review/send (2026-10-09)

L3 in an active chat opens a RAM-only message prompt, retaining its owned reader
context. Review displays the selected profile, video and full literal text.
Cross creates one chat-send job; repeated confirmation does not submit or cancel
another job. Returning to chat, leaving the browser or profile/source changes
invalidate the draft and stale keyboard callbacks. The confirmed response must
echo the video and intent and include a message ID. Uncertain sends cannot reuse
the action. UTF-8 validation bounds input to 200 scalar characters / 800 bytes,
rejecting blank, control, malformed, surrogate and overlong forms. R3 and
Touchpad retain their reader-mode/comments functions.

Shipping reader and compose/review provider ASan/UBSan tests passed, as did
actual generated RML callbacks and broader provider/recovery regressions.
Evidence: `.deps/chat-compose-native.log`, `.deps/chat-compose-ui.log`,
`.deps/chat-compose-provider.log` and corresponding `.result` files (all 0).
The RML case verifies L3 navigation, review state, full RAM text, no early HTTP
send and literal markup. Existing RML warnings remain in the host log. These
tests use deterministic HTTP; physical PS5 virtual keyboard/controller behavior,
real-account sending and hardware continuity remain unverified.

- 2026-10-09 | fallback availability | read-only | pass: idle running-title list, no other probes or changes; owned lock released | .deps/fallback-idle.json | recheck idle before deployment

- 2026-10-09 | native chat / PS5 package | 9a7609c8 | PS5 | pass: 211 objects, link, native container, package and freeze | .deps/chat-compose-ps5-build.log; .deps/chat-compose-freeze.log | storage/startup acceptance

Build ID `prosperotube-9a7609c8_1010-0141`; frozen candidate contains 126 files,
eboot SHA-256 prefix `08b98295d73f6689`. Build and freeze returned 0.

- 2026-10-09 | fallback Lapy startup | 9a7609c8 | PS5, firmware unreported | failed: Lapy helper apply_failed, startup stopped | .deps/chat-compose-fallback-console.log; .deps/console/storage-chat-compose-9a7609c8-fallback/klog.txt | diagnose offline, no identical-byte retry

The idle-first case uploaded and read back all 126 frozen files, verified them
again after the 130-second settle, established exact title registration and
rechecked idle before sending one launch. The script returned 1 with an
inconclusive missing-checkpoint result. Captured klog strengthens that result:
the matching build reached main, configured 3840x2160 display and native IME,
then reported `Lapy: path=helper status=7; durable storage unavailable` and
`ProsperoTube requires writable /data/prosperotube; startup stopped`.
Status 7 is `elevation::Status::apply_failed`. The system subsequently captured
a title coredump and terminated it. The exact helper failure stage and shutdown
behavior remain to be diagnosed; no root-layout cause is assumed from this code.
No application status/UI/storage-ready checkpoint was produced. No input,
second launch or close payload was sent; the owned lock was released. No further
console traffic followed the failure. Storage persistence, clean teardown,
service health after termination, playback, audio and chat remain unverified.

### Startup logger / Lapy precondition correction (2026-10-09)

Closer inspection of the captured fallback klog identifies SIGSYS (signal 12)
after `eboot.bin calls exit() exit_value=1`; it does not establish a GPU fault.
The application returns from failed initialization without its normal run-loop
shutdown. That failure lifecycle remains a separate hardware acceptance item.

The generated boot logger now refuses to open its file, start the writer or
start the log server until `evo_jailbreak_is_open()` confirms the complete
storage proof. Previously a writable sandbox-mounted path could activate those
threads before Lapy; the pinned helper requires a single-threaded target with a
private credential. The native video watchdog does not start in the initial
probe: its start calls belong to decoder renewal and playback. No claim is made
that the logger was the exact cause of the captured status 7. Helper stage logs
were unavailable in that case. No further console connection or retry occurred.

`scripts/test_startup_log_gate.py` exercises the actual generated flush body:
repeated unproved calls retain buffered messages without disk/thread/server
effects; proved startup opens the log and enables those sinks, and subsequent
flushes drain without reopening. Run after `scripts/prepare_native.py`.

Generated-flush regression and both existing/resident/helper and mounted-data
listing regressions passed; wrapper result 0. Evidence:
`.deps/startup-log-test.log`, `.deps/startup-log-elevation.log` and
`.deps/startup-log.result`. This is host evidence. A new PS5 build and an
idle-first console case with helper-stage capture remain required.

- 2026-10-09 | startup log gate / PS5 build | bb5672a0 | pass: 211 objects, link, package and immutable freeze; 126 files, eboot prefix f9aca772ad4cf6e9 | .deps/startup-gate-ps5-build.log; .deps/startup-gate-freeze.log | storage acceptance
- 2026-10-09 | primary storage/startup | bb5672a0 | PS5, firmware unreported in this case | failed: helper apply_failed (7), startup stopped, exit(1) followed by SIGSYS | .deps/startup-gate-console.log; .deps/console/storage-startup-gate-bb5672a-primary/klog.txt | helper-stage diagnostics offline

Exact build `prosperotube-bb5672a0_1010-0206` reached main, 3840x2160 display,
IME and browser preloads, then returned helper status 7. The logging gate alone
does not resolve startup. Full upload/readback, the 130-second settle with a
second full readback, exact registration and a fresh idle check passed before
one launch. The harness stopped on the matching storage-failure breadcrumb;
no input, close payload, second launch or subsequent console connection was
sent. Case result 1; process terminal; owned lock absent. No storage-ready,
persistence, graceful teardown, post-failure service-health or playback claim.
The case metadata retained the earlier storage-integration wording; the source
change being tested here is the boot-logger gate. Cross-console comparison does
not isolate a root cause.

An earlier separate idle-first read-only discovery found the primary idle and
its existing global ps5log endpoint configured for console loopback. No logging
configuration was changed. The helper's stage logs still were not captured;
the next diagnostic must expose them without disturbing shared configuration.
Evidence: `.deps/lapy-log-discovery.json` and `.deps/lapy-log-discovery.result`
(0). Do not repeat this frozen candidate unchanged.

### Lapy PR #50 integration (2026-10-09)

Pinned `57fcc19b7f57e46c823d6f2a6c6233ebf3288079`, the GitHub API-verified head
of https://github.com/mpereiraesaa/PS5-Lapy-JB-Daemon/pull/50. The upstream fix
stops targets before raw kernel-pointer snapshots, checks their title identity
around the stop, and requires exact post-donor root-reference balance before
detach. Its unmodified ELV1 protocol still matches the client hash. The prior
pin and failed frozen candidates remain separate comparison artifacts.

Upstream six native ASan/UBSan cases and 53 Python tests passed; both existing
ProsperoTube elevation client regression cases passed. Evidence:
`.deps/lapy-pr50-native.log`, `.deps/lapy-pr50-python.log`,
`.deps/lapy-pr50-client.log` and corresponding `.result` files (all 0).

Both exact-title PS5 helper builds passed `-Wall -Wextra -Werror` and manifest,
protocol and ELF validation. Default helper SHA-256:
`3f26c687c0cc3a7d5047f2a0b7f0d80886e92ac49c59c301d03eeed8488e1640`.
Diagnostic helper SHA-256:
`bf1d7b1017a6c078b81c5267152dfc985389f4836d288c34351243de98a2b0e1`.
Diagnostic records use the upstream result-file option at
`/data/prosperotube/lapy-helper.log`; normal builds omit it. The full upstream
manifest is checked for the exact optional path and absence of a test-only
snapshot delay. The packaged manifest records the source and logging path.
Evidence: `.deps/lapy-pr50-default-build.log` / `.result` (0),
`.deps/lapy-pr50-diagnostic-build.json` and upstream helper build logs/manifests.

This is offline helper integration evidence. No console traffic occurred during
this update. A new complete PS5 build/freeze and guarded application case are
still required; PR #50 does not itself prove the cause or resolution of our
captured status 7 failures. Diagnostic installation must create the app data
directory before launch and retain helper logs. Shared logging settings remain
unchanged. No custom logging adapter is incorporated.

- 2026-10-09 | Lapy PR50 / complete PS5 package | ec358d07 | pass: 211 objects, link, package and immutable freeze; 126 files, eboot prefix 08748ab3bd47c1c2 | .deps/lapy-pr50-ps5-build.log; .deps/lapy-pr50-freeze.log | guarded application acceptance
- 2026-10-09 | Lapy PR50 / primary console | ec358d07 | no-run: first FTP connection refused before running-title check | .deps/lapy-pr50-console.log; .deps/console/storage-lapy-pr50-ec358d07-primary/milestones.txt | continue offline

Exact build `prosperotube-ec358d07_1010-0225`; package helper SHA-256
`bf1d7b1017a6c078b81c5267152dfc985389f4836d288c34351243de98a2b0e1`.
Build and freeze returned 0. The console wrapper returned 1 and is terminal.
The first console operation, FTP connection on port 2121, was refused; it did
not establish idle or busy state. No other service probe, app directory creation,
upload, launch, close, input or retry occurred. The owned lock was released.
The frozen candidate remains undeployed; helper-stage, storage, persistence,
teardown and playback acceptance remain pending. No fallback case was started
after this transport failure.

### Offline fixture chat sending (2026-10-09)

Generated HLS chat now accepts selected offline-account messages through the
shipping library and job endpoints. API tests confirm literal markup and Unicode
content, a single polling copy, identical direct/job repeat confirmations, and
rejection of Guest, foreign-profile, changed-intent and closed contexts. Injected
clock tests cover 200-character limits, invalid controls/surrogates, rolling
polls, 256 retained intents, expiry and profile-removal cleanup.

Focused fixture suite: six passed. Full companion Python suite: 174 passed,
with the existing Starlette TestClient deprecation warning. Evidence:
`.deps/fixture-chat-send-focused.log` / `.result` and
`.deps/fixture-chat-send-full.log` / `.result` (both 0). No console traffic or
real YouTube posting occurred. Native source and the undeployed PR50 package
remain unchanged; PS5 compose/review/send acceptance is still required.

### Buffered HLS seek after origin expiry (2026-10-09)

The shipping relay now retains bounded, session-owned copies of completely
downloaded HLS resources. HTTP tests cover exact full/range/suffix reads and HEAD
metadata after origin expiry, continued fresh root fetching, session isolation,
expiry, account-removal callback cleanup, size/count/staging limits, eviction,
incomplete lengths, encoded/partial responses and interrupted streams. Buffered
bodies revalidate session ownership and profile availability between 64-KiB
chunks, preserving account-removal retirement behavior.

The generated finite-HLS host probe decoded 8.0 seconds with audio and video,
retained four resources (977224 bytes), then used output seeking to 3 seconds and decoded
2.0 seconds after synthetic origin segments began returning 403. It made zero
origin segment requests during the second decode. Evidence:
`.deps/hls-buffer-decode.json`, `.log` and `.result` (0).

Full companion suite: 182 passed, with the existing Starlette TestClient warning.
Evidence: `.deps/hls-buffer-full.log` / `.result` (0). Focused relay suite:
26 passed (`.deps/hls-buffer-focused.log` / `.result`, 0). No YouTube or console
traffic occurred. Native source and the frozen PR50 package are unchanged.
Never-downloaded/evicted expired resources, real rolling live/DVR windows,
long-session continuity and PS5 seek/playback acceptance remain incomplete.

HLS sequence numbers alone cannot establish matching bytes across different
playlists ([RFC 8216 section 4.3.3.2](https://www.rfc-editor.org/rfc/rfc8216.html#section-4.3.3.2)).
This buffer reuses actual complete downloads rather than inferring replacement
media identity from matching sequence numbers.

### Input-seek proof and native seek ownership (2026-10-09)

The initial buffered-HLS probe used output `-ss`, which decodes/discards from
the beginning and did not establish a demuxer input seek. The probe now places
`-ss` before the input ([FFmpeg seeking options](https://ffmpeg.org/ffmpeg.html#Main-options)).
After eight seconds of initial decode and synthetic origin expiry, a 3-second
input seek decoded two seconds with zero origin segment requests. Its 59 video
frame and 95 audio-block fingerprints exactly matched a local input-seek
reference; the video differs from playback at the beginning. Clearing the
buffer caused the same request to fail with one rejected origin request.
Evidence: `.deps/hls-buffer-decode.json`, `.log` and `.result` (0).

The native UI previously updated the playback clock base before the seek was
accepted or completed, then announced seek begin after publishing the request.
A fast demux completion could therefore be followed by a late UI begin,
re-arming the hold without a remaining request. The generated overlay now
leaves clock commits and begin/end ownership on the demux thread. Both the UI
and direct request path reject nonfinite and excessive targets before side
effects; the existing duration clamp remains authoritative.

The extracted shipping UI method and demux request passed ASan/UBSan tests for
refusal, pending requests, fast completion before UI return, duration clamping,
inactive playback and invalid targets. Evidence: `.deps/seek-handoff.log` /
`.result` (0). Existing provider/playback-recovery and exact generated pthread
handoff regressions also passed (`.deps/seek-provider-regression.log` /
`.result` and `.deps/seek-recovery-regression.log` / `.result`, both 0).
A new full PS5 build remains pending at this source milestone. The prior frozen PR50 package remains
unchanged and does not include this native fix. No console traffic occurred.

### Seek candidate packaging and bounded installation cases (2026-10-09)

Source `00372796` completed the full PS5 build, packaging and immutable freeze:
build `prosperotube-00372796_1010-0258`, 126 files, eboot SHA-256 prefix
`ce25ebbfc8158d28`. Build and freeze evidence: `.deps/seek-ps5-build.log`
and `.deps/seek-freeze.log`, with both result files containing 0.

Both initial console cases verified all installed bytes twice, including the
130-second settling period, and established exact title registration. Each
final running-title check found a title running, so neither sent a launch,
input or close command. Evidence: `.deps/seek-console.log` and
`.deps/seek-console-primary.log` (terminal result 1).

After the user reserved the fallback console, a separate idle-first case
established idle state but found that the installed bytes no longer matched
the frozen candidate. It stopped without uploading or launching. The case
does not identify which file differed or why; its adapter also omitted the
FTP SELF command used by the installation verifier, so no corruption cause
is established. A separate fresh installation case then stopped on its very
first running-title check because a title was running. It made no uploads,
service probes, launch, input or close requests. Both owned locks were released.
Evidence: `.deps/seek-console-reserved.log` and
`.deps/seek-console-reserved-install.log` (terminal result 1 each).

This frozen build has never been launched. PR50 helper execution, storage,
persistence, native seek behavior and actual YouTube playback on PS5 remain
unverified. A future idle-first installation must restore and verify the
exact candidate before launch.

### Relative seek positions and absolute HLS timestamps (2026-10-09)

The generated finite HLS fixture begins at 1.445333 seconds. A host FFmpeg
`av_seek_frame`/packet-read reproduction of the previous native conversion
admitted the first video packet at relative position 1.854667 for a requested
3.25-second seek. Adding the common container origin to the seek and video
discard target admitted a packet at 3.354667 instead; packet decode order
permits this small forward difference. Audio passed its corresponding bound.
This establishes packet gating, not decoded frame timing or audible A/V sync.

The shipping helper now converts relative positions into absolute stream
timestamps and video discard targets. Audio discard comparisons subtract the
same common origin; separate stream start times must not become separate
clocks. Invalid conversion leaves the prior clock base intact and resumes a
previously playing scrub. The generated overlay applies this to the actual
demux and audio paths without modifying the upstream checkout.

Real HLS packet reads plus boundary tests passed ASan/UBSan, including
fractional targets, distinct stream starts, negative/missing origins,
invalid inputs/time bases and addition overflow. Evidence:
`.deps/seek-timeline.log` / `.result` (0). Existing extracted shipping UI and
request handoff also passed (`.deps/seek-timeline-handoff.log` / `.result`, 0).
Native provider and playback-recovery ASan/UBSan regressions passed
(`.deps/seek-timeline-native.log` / `.result`, 0).
The host uses libavformat 62.3.100; PS5 uses the existing FFmpeg 7.1.1 recipe.
Full PS5 packaging and hardware acceptance of this change remain pending.
No console or YouTube traffic occurred for this offline change.

Source `dd34eb58` subsequently passed the full PS5 build and immutable freeze:
`prosperotube-dd34eb58_1010-0325`, 126 files, eboot SHA-256 prefix
`ffde9983f34493af`. Evidence: `.deps/seek-timeline-ps5-build.log` and
`.deps/seek-timeline-freeze.log` / result files (both 0). No console deployment
or launch occurred. This package corrects in-place seeks, not initial resume.

### Initial resume origin and clock anchoring (2026-10-09)

Initial resume used another relative-to-stream conversion that omitted the
container origin. Its audio re-anchor also treated absolute first-packet PTS
as a relative UI position. The overlay now uses the common shipping timestamp
helper for initial seeking and subtracts the same origin when re-anchoring.
The existing intentional keyframe run-up is preserved; resume position follows
the actual landing rather than claiming an exact decoded start at the saved
position. Invalid conversions take the existing failed-resume path.

The extracted shipping initial-resume block and audio re-anchor passed a real
generated HLS seek under ASan/UBSan. Checks cover the requested absolute stream
timestamp, relative first-audio landing, one-shot re-anchor, refused seeking,
nonfinite positions and overflow rejection without a seek call. Existing
shipping UI/request handoff passed in the same case. Evidence:
`.deps/resume-timeline.log` / `.result` (0). Header linkage was also compiled
as C++ against host FFmpeg. Full PS5 packaging of this follow-up remains
pending at the source milestone; console resume/audio acceptance is unverified.

Source `3c3eb818` subsequently passed all 211 PS5 object builds, link,
containers, folder/PFS/ZIP packaging and immutable freeze:
`prosperotube-3c3eb818_1010-0333`, 126 files, eboot SHA-256 prefix
`7c3e112076128376`. Evidence: `.deps/resume-timeline-ps5-build.log` and
`.deps/resume-timeline-freeze.log` / result files (both 0). No deployment or
launch occurred. This package includes both seek and initial-resume origin
fixes. Actual decoded resume position, video-only resume anchoring, PS5 sound
and synchronization remain acceptance requirements.

### Stored SELF verification and current console installation (2026-10-09)

The first `3c3eb818` installation case established idle state but stopped at
`sce_module/libc.prx` upload comparison. Its adapter mistakenly toggled FTP
SELF mode twice on the upload connection. SELF selects signed stored bytes
versus their ELF view; it is not a filesystem elevation command. The adapter
therefore requested the wrong comparison view. This does not establish file
corruption. No launch, input or close command followed; the lock was released.
Evidence: `.deps/resume-storage-console.log` / `.result` (1).

The corrected adapter selects stored bytes exactly once per data connection,
after its separate minimal running-title checks. Extracted FTP-function and
command-sequence checks passed without console traffic:
`.deps/check-ftp-self-mode.log` / `.result` (0). A new bounded case then
verified all 126 uploaded files and compared them all again after the
130-second settling period. Registration passed, but the final running-title
check found a title had started. No launch, input or close was sent, and the
lock was released. Evidence: `.deps/resume-storage-stored-self-console.log`
/ `.result` (1). These installed bytes are verified; app execution, helper
diagnostics, persistence, clean teardown and YouTube playback remain pending.

### Video-only resume clock ownership (2026-10-09)

Initial resume now arms its anchor for video-only sources as well as A/V.
The video decoder's existing valid/non-discard frame branch re-anchors the
relative UI position when no audio stream is selected. A/V retains audio
ownership. Missing timestamps and discarded frames leave the pending anchor
intact; unreasonable or nonfinite positions cannot change the clock base.

Extracted shipping resume, video-anchor and video-clock code passed ASan/UBSan,
including real host AVC decoding of the generated video-only MP4. Checks cover
the actual decoded frame position, one-shot anchoring, A/V ownership, missing
timestamps, seek discard and unrelated origins. Evidence:
`.deps/video-resume-timeline.log` / `.result` (0). PS5 build and hardware
acceptance of this follow-up remain pending at the source milestone.

### Video-only PS5 package and fresh public YouTube check (2026-10-10)

Source `e52315dd` completed all 211 PS5 object builds, link, container conversion,
folder/PFS/ZIP packaging and immutable freeze: build
`prosperotube-e52315dd_1010-0354`, 126 files, eboot SHA-256 prefix
`60967ac1c01c1852`. Evidence: `.deps/video-resume-ps5-build.log` and
`.deps/video-resume-freeze.log` / result files (both 0). This package has
never been deployed or launched. The previous `3c3eb818` installation remains
verified but unlaunched by its bounded case.

A fresh Guest check resolved public video `jNQXAC9IVRw` through the shipping
companion and DASH relay, then host FFmpeg decoded AVC/AAC for 8.0 seconds
at input offset zero and 8.0 seconds at input offset four. Pairing, resolution,
audio/video probing, both duration checks and temporary-server cleanup passed.
Evidence: `.deps/first-video-current-public.json`, `.log` and `.result` (0).
No account profile, mutation, simulated expiry or console traffic occurred.
The probe now records attempted input offsets explicitly and labels VOD cases
without a positive offset as `vod_decode_without_seek`, avoiding a false seek
claim when metadata provides no usable duration. The updated probe was executed
successfully; observed offsets were exactly `[0, 4]`.

This is host service compatibility and build evidence. PR50 helper execution,
PS5 storage/persistence, real YouTube playback, decoded position, physical
controller behavior, audible A/V synchronization and teardown remain pending.

### Settled-install startup control and primary availability (2026-10-10)

A separate `3c3eb818` startup control reused the already-settled installation
without uploading. Fresh idle checks and declared services passed, followed by
an exact readback of every installed file and exact title registration. The
final prelaunch check found an unrelated title had started; no launch, input,
close or installation upload was sent. Evidence:
`.deps/resume-storage-reuse-control-console.log` / `.result` (1). The saved
ShadowMount delta confirms the unrelated start. The case used this committed
control intentionally; it does not test the newer video-only resume change.

A separate primary-console case for frozen source `e52315dd` stopped on its
first minimal FTP running-title check because a title was running. No other
service probe, directory preparation, upload, launch, input, close or retry
occurred. Evidence: `.deps/video-resume-storage-primary-console.log` /
`.result` (1). Both owned locks were released. These are no-run results;
neither establishes PR50 helper execution or native startup.


### Per-profile hidden Cross and controls timeout (offline, 2026-10-10)

The pinned SmartTube OK-button and UI-auto-hide settings now reach the actual
PS5 PlayerScreen, provider chooser and playback options. Four hidden-Cross modes
are supported; visible Cross retains play/pause. Never/1–15 second hiding is
profile-scoped. Valid legacy profiles retain pause/3.5s; new profiles default
to show/3s. Pause-only and speed-only suppress the regular HUD, preserving
buffering, scrub, end, music and stats overlays. Last-speed selection still uses
the existing console-wide toggle; this batch does not complete all profile settings.

All thirteen real-provider sanitizer regressions pass, including new defaults,
persistence, profile isolation, legacy migration, corrupt/duplicate rows and the
64-profile storage bound. Extracted shipping input/visibility, speed-reopen,
SponsorBlock and chat renderer/picker checks pass. Evidence: ignored
`.deps/player-controls-{provider,input,speed,sponsor,chat}.log`. The initial
combined runner stopped at outdated phone-queue mocks; the corrected SponsorBlock
test also verifies phone/local EOF precedence. The first chooser fixture tried
to activate a row outside the published page; the fixture now navigates the
actual eight-row page before activation. Those diagnostic logs remain retained.
Console acceptance remains pending; no console connection was made.

The corrected actual RmlUi chooser/chat test and affected PS5 provider,
PlayerScreen, playback picker and RML provider objects pass:
`.deps/player-controls-ui-sdk.result` (0), UI-final/SDK logs. The existing
199 qualified-math warnings remain; this is not a warning-free build claim.


Source `f6b78748` completed the full PS5 build, container/folder/PFS/ZIP
packaging and immutable freeze: 126 files, eboot SHA-256 prefix
`8d193cb081d74d0d`, build `prosperotube-f6b78748_1011-0348`.
Candidate: `/home/denis/.cache/prosperotube/candidates/f6b78748-offline-player-controls/PPSA99142`.
Evidence: `.deps/offline-player-controls-{ps5-build,freeze}.log` and result
files (both 0). Nothing was installed or launched. All offline work and the full
SmartTube objective remain incomplete; remaining sections stay active.


### Seek confirmation and actual playback ownership (offline, 2026-10-11)

Per-profile Regular / confirm-paused / confirm-playing choices now reach the
provider, playback picker and shipping controller. Regular commits on D-pad
release (600ms idle fallback); confirmation retains the preview until Cross or
Circle. Both previous pause states survive commit, cancel and refused admission.
Explicit phone/voice/timer pause or resume retires the preview directly without
an intermediate resume. Active opening/stopped/seeking/finished provider FSM
states enter preview directly, preserving the real engine pause state.

A separate actual-playback position now feeds chat replay, phone snapshots and
provider/local history while the HUD displays the preview target. Extracted
shipping getters, callbacks and complete resume-writing method prove that a
pending target is not reported or stored, including a changed resume base.
Logical paused reporting includes paused previews. EOF callbacks cannot consume
phone/local queues during preview, pending seek or decoder discard. Cross/Circle
hints and music hints describe confirmation/cancellation in both renderers.

Shipping methods execute with the actual EVO StateMachine under ASan/UBSan:
all modes, both pause states, release/confirm/cancel, refused requests, external
pause/resume, stale FSM states, held-key acceleration, bounds and input priority.
`bash scripts/test_seek_controls.sh` passed after the final fixture expansion.
The thirteen provider tests, phone adapter and timer tests passed in
`.deps/seek-controls-final.result` (0). Connected input/speed/SponsorBlock,
mailbox and actual demux/media seek regressions, chat HUD/options, actual RmlUi
chooser/chat and affected PS5 controller/Application/provider/render objects
passed in `.deps/seek-controls-connected-final.result` (0). SDK warnings remain.

The first focused test incorrectly expected a strict less-than-duration clamp;
it was corrected to the foundation's inclusive duration boundary. This was a
fixture failure, not a shipping decoder failure. Physical PS5 controller feel,
playing-preview sound/motion, cancellation, voice/phone/timer interaction and
full seeking-thumbnail behavior remain unaccepted. No console connection occurred.


Source `e72ea7a9` completed all 216 PS5 object builds, link, container conversion,
folder/PFS/ZIP packaging and immutable freeze: build
`prosperotube-e72ea7a9_1011-0409`, 126 files, eboot SHA-256 prefix
`f781c88de0150ef7`. Candidate:
`/home/denis/.cache/prosperotube/candidates/e72ea7a9-offline-seek-controls/PPSA99142`.
Evidence: `.deps/offline-seek-controls-{ps5-build,freeze}.log` and result files
(both 0). No deployment, console check, launch or input was performed. The full
SmartTube objective and remaining offline work are still active.


### Offline profile settings (2026-10-11)

Quality/codec, caption/audio language and caption enable/automatic preferences, speed policy/all-video
speed/last-speed toggle, SponsorBlock policy and exclusions, DeArrow/paid-content
notifications, autoplay/stream prompt/history sync, chat and sleep timer are now
persisted independently for up to 128 profiles. Existing known profiles migrate
their prior preferences; new accounts get defaults. Guest/selected profiles are
reserved when loading bounded saved rows, duplicate profiles use the first valid
identity, invalid fields use defaults and full-table selection fails explicitly.
Device companion pairing, phone identity and disconnect preferences remain shared.
Original active-video ownership governs speed memories and last-speed toggle when
the library account changes; this cannot modify the newly selected account.
Initialization without a store resets preferences/profile/source/local records.

All fourteen shipping-provider ASan/UBSan regressions and PS5 provider compilation
passed: `.deps/profile-settings-final.result` is 0. New tests exercise shipping
settings actions, restart, legacy migration, default separation, source reset,
corrupt/duplicate rows, capacity and active-account speed ownership. Older native
and Music fixtures explicitly configure account preferences or expect fresh
account defaults instead of relying on inherited console-wide settings. SDK
compilation emitted 208 qualified-math warnings, without errors. Earlier failed
fixtures/results are retained as diagnostic evidence; the final result supersedes
them. No console connection, deployment or hardware acceptance occurred.


Source `8d660737` completed all 216 PS5 objects, link/container conversion and
folder/PFS/ZIP packaging, then immutable freeze: build
`prosperotube-8d660737_1011-0425`, 126 files, eboot SHA-256 prefix
`dc47bbd694039d72`. Candidate:
`/home/denis/.cache/prosperotube/candidates/8d660737-offline-profile-settings/PPSA99142`.
Both `.deps/offline-profile-settings-{ps5-build,freeze}.result` files are 0.
Full SmartTube parity and remaining offline work are still active.


### Offline local unavailable-video advance (2026-10-11)

The native local queue now advances after a classified `video_unavailable`
resolver failure, following SmartTube VideoLoaderController's five-second delay.
It requires the matching current request, profile and queue origin with autoplay
on. Skipped records retain queue membership, resume position and watched state.
A bounded per-run ID set prevents retries within the same run, including after a
later playable item reaches EOF. Terminal exhaustion displays the original error.
Authentication/challenge/rate-limit/network/unknown failures, failed pairing and
arbitrary error text cannot start this path. Stale replies are rejected.

The actual generated ProviderHostScreen schedules the next item, clears old
choices and cancels on any new button, browser exit, profile/source/search change,
autoplay disable or new phone launch. Successful player navigation retains the
run's failed-ID set; fresh manual playback resets it. Phone playlist failure
advancement remains separate unfinished work; this batch adds only a scoped
pending-launch query for phone priority over the local countdown.

Focused shipping-provider sanitizer checks and extracted shipping host blocks
passed (`.deps/queue-failure-focused.result` 0 and host log). Expanded final
provider/phone/PS5 checks are recorded separately below. A test compiled against
an earlier object caught missing immediate autoplay-off retirement; the shipping
branch now cancels its pending advance explicitly, and stale/pairing/off-toggle
fixtures verify the final source. No console connection occurred.


All fifteen provider cases passed in `.deps/queue-failure-provider-verified.log`.
After the queued-membership guard was tightened, the affected queue, Music,
recovery and SponsorBlock cases, shipping host blocks, actual phone adapter and
PS5 provider/phone/ProviderHostScreen compilation passed again:
`.deps/queue-failure-connected.result` is 0. The queued candidate is revalidated
before launch; removing it during the delay cannot launch it. Compilation caught
an incorrectly placed C++ chrono include inside the host's C linkage block; it
was moved beside the C++ headers, then all three affected PS5 objects compiled.
SDK compilation retained 208 qualified-math warnings without errors. Earlier
failed iteration logs/results remain diagnostic evidence, not acceptance.


Source `73c7b869` completed all 216 PS5 objects, link/container conversion and
folder/PFS/ZIP packaging, then immutable freeze: build
`prosperotube-73c7b869_1011-0444`, 126 files, eboot SHA-256 prefix
`2e0a46ca8df87873`. Both
`.deps/offline-local-queue-failures-{ps5-build,freeze}.result` files are 0.
This records offline packaging; hardware acceptance is separate. The user
subsequently renewed exclusive console testing authorization.


### PS5 local queue acceptance (2026-10-11)

Frozen `73c7b869` passed full installed readback and settling. The first observer
missed a countdown notice absent from the UI snapshot; it captured automatic
next playback, then verified clean exact-title release and original-state/sidecar
restoration. That case is inconclusive for cancellation, not product acceptance.

The corrected observer records served typed failed jobs and passed an offline
HTTP check before the separate case. On PS5, a button cancelled the first pending
advance: no next-video resolve occurred during the following six seconds. A
fresh unavailable attempt then resolved the next queued item 5.959 seconds after
the failing extraction. Native playback time advanced from 1.41 to 3.41 seconds,
without a decoder fatal. The failed item remained queued, unwatched and at its
original nine-second resume position. Exact owned GPU/process/video/mount
release, healthy services and byte-for-byte original provider/sidecar restoration
passed. `.deps/local-queue-hardware-observer2.result` is 0; evidence is in
`.deps/console/local-queue-73c7b869-hardware-observer2/`.

This generated-media case establishes local queue behavior and native playback
progress, not public-service availability, physical motion/audio, phone playlist
failure advance or hardware acceptance of the other recent preferences.


### Offline phone unavailable-video advance (2026-10-11)

Typed current-scope `video_unavailable` results now advance phone playlist
occurrences after five monotonic seconds, retaining sparse indices and duplicate
occurrences. Every retry gets a fresh ticket and requires its own native selection
binding. Unknown/auth/network errors cannot enter this path. Any button, host
exit, scope retirement, stop, replacement playlist or disconnect can retire the
pending run. Intermediate commands retain their order. Exhaustion consumes the
unplayable command without declaring playback success or failing the receiver;
media feedback and successful startup binding remain separate from its transport
cursor. Saved local queue/history are untouched. A copied prior selection cannot
satisfy a new launch's binding requirement.

`.deps/phone-unavailable-connected.result` is 0: fifteen provider sanitizer cases,
real receiver/phone adapter, extracted shipping phone and local host blocks, and
PS5 remote/phone/ProviderHostScreen objects passed. The expanded real adapter
adds EOF-owned retries, empty queues, disconnect close and late stop arrival
through actual driver JSON/poll replies; `.deps/phone-unavailable-expanded.result`
is 0. SDK output retained 36 qualified-math warnings and one existing dangling-else
warning. No console traffic occurred for this batch. Packaging and PS5 acceptance
are recorded separately; entirely unavailable long windows still require refill
continuation beyond the bounded supplied playlist window.


Source `c9721611` completed all 216 PS5 objects, link/container conversion and
folder/PFS/ZIP packaging, then immutable freeze: build
`prosperotube-c9721611_1011-0511`, 126 files, eboot SHA-256 prefix
`9fe69649f7be3445`. Both
`.deps/offline-phone-queue-failures-{ps5-build,freeze}.result` files are 0.
The user requires all remaining offline features and the final candidate before
another console connection; this is an intermediate offline milestone.
