# YouTube phone receiver — backend integration

The disconnect preference now cycles keep playing → stop phone playback → close
the app. Existing saved stop preferences retain their meaning. The close choice
matches SmartTube's whole-app behavior, including while browsing; retired
receiver commands cannot close a new scope. Closure releases media, cancels
capture/captions/provider operations and retires the receiver, presents the
safe-close screen and parks rendering. Only confirmed GPU fence retirement
permits one `sceSystemServiceLoadExec("exit", NULL)` request. Refusal, missing
fences or a drain timeout leave the process parked. Host execution/fault tests
cover this path. Candidate `9340ab5a` also passed a single PS5 fixture case:
playback/pause, saved close preference and disconnect-triggered native self-exit,
exact video/process/mount release, healthy services and original state restoration.
Real-phone disconnect and browsing-only hardware close remain open.

Successful phone launches now bind the exact provider selection before startup
acknowledgment, closing the interval in which an immediate manual reopen of the
same video could inherit remote queue ownership.

The companion implements YouTube's Lounge / MDX TV-code receiver protocol.
Registration, code generation, command reception and player-state feedback passed
a live temporary-receiver check. Native pairing settings and identity storage are
integrated. Native player command execution passed a PS5 fixture cycle, including
playback, seek, captions, queue transitions and disconnect policy. Pairing and
control from a real YouTube phone application remain to be accepted.

The native `tube_remote` driver is now included in generated PS5 build inputs.
It has passed ASan/UBSan command/scope/feedback tests and a PS5 SDK object compile.
It provides opt-in asynchronous registration/pair/poll/report/stop, typed commands,
owner-tagged acknowledgments and stable device/screen identity getters.
The provider calls the driver and Application connects it to the native player.

## Native pairing settings

YouTube preferences > Phone pairing provides Link a phone, connection On/Off,
and Reset phone pairing. The screen refreshes when the receiver returns its code
and formats it into groups of three digits. Opening preferences does not enable
the receiver. Linking explicitly enables it and requests a code; an enabled saved
receiver starts again with the application. Turning it off closes the receiver
while retaining its identity; reset closes it and forgets both IDs.

The native device UUID is created before registration using the PS5 C library's
random source. Enable/device/screen fields live in the atomic `youtube.json` under
`/data/prosperotube`, through the existing Lapy-backed data path. The UUID must be
saved before advertising; a returned screen ID is saved before pairing continues.
Failed writes stop advertisement and display a local-data warning. Neither the
TV code nor opaque receiver session is persisted. Corrupt/future storage remains
protected. Identities survive restart; late registration after reset is closed
without restoring the forgotten IDs. Browser cancellation keeps an explicitly
enabled receiver; provider shutdown closes it.

The provider workflow passed mock-transport ASan/UBSan tests, including blocked
atomic save, reset, restart and late cleanup. The provider, driver and RML host
objects compiled with the PS5 SDK. A full package, console pairing, random-source
runtime behavior and real phone acceptance still need verification.

This is original Python implementation based on the pinned SmartTube service
[Lounge interface](https://github.com/yuliskov/MediaServiceCore/blob/4d9767733aa92269b9d24f24844a7fa1bb5196c1/youtubeapi/src/main/java/com/liskovsoft/youtubeapi/lounge/LoungeService.java)
and its command models. Application framing follows
[Closure ChannelRequest](https://github.com/google/closure-library/blob/master/closure/goog/net/channelrequest.js):
decimal length, newline, JSON, with lengths measured in UTF-16 code units.
No Android or MediaServiceCore source is incorporated.

## Authenticated API

POST `/v1/remote` uses the companion Bearer token and bounded JSON requests.
There is no receiver, pairing request or upstream connection at companion startup.
Only an explicit `start` enables the receiver. The Lounge transport runs separately
from the library's serialized Node worker.

| Action | Input | Result / behavior |
| --- | --- | --- |
| `start` | Optional stable `device_id` UUID, saved `screen_id`, display `name`, strict Boolean `voice_enabled` (default false) | Opaque `session`, `device_id`, `screen_id`, name and listening status |
| `pair` | `session` | Nine- or twelve-digit `pairing_code` for the YouTube phone application's Link with TV code screen |
| `poll` | `session`, `ack` (initially zero) | At most 32 ordered typed commands; unacknowledged commands remain available on retries |
| `update` | `session`, increasing positive `update_id`, `player` | Sends now-playing, playback clock/state, volume/mute, previous/next and captions to the phone |
| `stop` | `session` | Cancels the owned stream, clears commands/secrets and attempts bounded channel termination |

`player` contains `video_id`, numeric `state`, `position_sec`, `duration_sec`,
`playlist`, `index`, opaque `ctt` returned by setPlaylist, `volume` (0–100),
`muted`, `has_previous`, `has_next`, caption `language` and `vss_id`.
State values follow the reference: playing 1, paused 2, idle 3, ready 4;
undetected 0 and undefined -1 are accepted. Repeating an accepted update ID with
identical data returns its previous success without sending another packet.
Older IDs and different data under the same ID are rejected.

Incoming commands include set/update playlist, play/pause, next/previous, seek,
volume, stop, now-playing query, phone connection events, device list, D-pad,
voice initiation/cancellation and caption selection. Unknown command names are
ignored. Known commands expose only validated fields, including a queue of at
most 128 video IDs per native window. The companion can retain up to 10,000 IDs
from a phone-supplied queue and refill windows after confirmed player feedback.
Global indices and repeated videos retain their positions; refills update the
queue without reopening playback. When the library worker is enabled, confirmed
playback can expand partial phone queues through profile-scoped WatchNext reads.
Finite playlists retain absolute indices; infinite Radio/Mix responses that reset
their indices are rebased at the current occurrence, keeping up to 32 preceding
items. Only contiguous, validated windows are accepted. Queue replacement, stop,
receiver failure and changed player identity retire delayed expansion results.
Failures retain the phone-supplied queue with bounded retry. Live Guest initial
and index-20 Mix reads passed; private/TV playlists and PS5 window expansion
acceptance remain open. Phone state alone does not prove native playback occurred.

The native integration must apply each command on the main thread before
acknowledging its sequence. Polling does not consume commands. Acknowledgments
cannot exceed the highest delivered sequence or move backwards. Wire IDs are
deduplicated; missing IDs, malformed frames and queue overflow fail the channel
without silently dropping a playback command.

## Lifetime and native integration gate

There are at most four active receiver contexts, each with a 120-second renewable
native lease and a 128-command queue. Normal long-poll completion opens another
back-channel request. Transport errors require explicit restart; uncertain state
reports are not automatically replayed. Failure clears pending commands and
stops the stream, so delayed callbacks cannot revive the receiver. Stop and
companion shutdown cancel owned tasks and clear private protocol state. Channel
termination is best effort with a two-second deadline.

Lounge tokens, SID and gsessionid remain private to the companion; they are never
returned through the API, formatted in errors or included in receiver repr.
Upstream URLs are fixed to YouTube; no request can supply an arbitrary host.

The native player adapter now executes playlist launch through EVO's existing
resolver/fallback/worker path. It acknowledges only after successful startup,
keeps fractional offsets, rejects retired owners and reports the actual selected
profile/source player. Play/pause, pause-preserving seek admission, stop,
next/previous, queue replacement, PCM volume, D-pad and caption selection
are connected. Caption language changes fetch a validated track into an inactive
SRT file and acknowledge only after the actual subtitle engine parses and selects
it. Failed, timed-out or retired requests preserve the current track. The operation
is bound to the logical playback selection, including when the same video is
manually reopened. The local subtitle picker uses the same staged operation.
A separate ephemeral phone queue preserves the
saved local queue and advances after proven presentation EOF. Playlist windows
retain the sender's global index and click-tracking parameters.

Volume scales the final S16 output with an atomic main/audio-thread gain; mute
does not change the media clock or retirement accounting. Unknown caption
languages are not silently acknowledged. The local picker now stages a second
YouTube language through Square into an independent EVO cue buffer; primary
changes and failed replacements preserve the other line. Phone caption commands
continue to select the primary language. PS5 secondary-caption visual/input
acceptance remains open. Supplied queues and partial-queue expansion use the bounded windows
described above. The saved disconnect choice defaults to keeping
playback running; opt-in stop applies only to the current phone-owned video and
clears its ephemeral queue. It returns to the browser. The separate close choice
requests whole-app exit through the checked GPU park path.
Native fixture playback, paused caption switching, queue/EOF and disconnect
checks passed on PS5. Real phone acceptance remains open.
The receiver advertises playback and D-pad capabilities. Microphone capability
requires both native `voice_enabled` and a configured local ASR backend; the API
clamps the request off when ASR is unavailable. A capability change requires
retiring/rebinding the receiver with the same saved identity. Old clients omit
the flag and retain their previous capabilities.
The optional [local speech-recognition service](VOICE_SEARCH.md) now provides
authenticated bounded PCM upload, asynchronous query results and cancellation.
Real whisper.cpp inference passed the host API. Native capture, local/phone
controls, scoped upload/job lifecycle and actual search navigation are connected;
the first package built/froze. Availability negotiation passes host/native tests
and PS5 object compilation. Its full package built/froze, and the console case
established startup/UI/capability before a malformed fixture command stopped the
case. Physical microphone/phone acceptance remains open.

The driver keeps receiving/renewing the lease while commands wait for application.
It validates an entire response before changing its command queue. If local
execution advances acknowledgment during an in-flight poll, the returned applied
prefix is discarded and the outstanding prefix must remain identical. A command
owner changes with the provider/profile/identity scope, so a delayed launch result
cannot acknowledge a same-number command from a new receiver. Late successful
registration after scope change is closed through its original source/token;
new registration waits for owned cleanup callbacks to complete. Pairing identity
and code revisions can be used by the provider's persistence/UI integration.

Sparse WatchNext windows optionally include `video_indices`, one strictly
increasing original global occurrence index per `video_ids` entry. The first
index equals `window_start`, and the current video must match its exact index.
Known unavailable rows are omitted without renumbering the surviving entries.
Absent maps retain the contiguous-window contract. The companion validates and
forwards the map; native next/previous and EOF use compact playable entries while
feedback preserves original indices, including duplicate video IDs. Unknown or
ambiguous rows still reject expansion and preserve the supplied phone queue.

## Remote recommendation fallback discovery and adapter

`playback_next` accepts the strict boolean `remote_origin` only for this operation.
When true and the autoplay endpoint is absent, the worker examines the primary
parsed WatchNext video group. Existing, self-referencing and invalid autoplay
endpoints retain their existing handling. Arbitrary shelves, ads and next-button
endpoints are not used as substitutes.

An available group returns `fallback_context: {group, index: 0}` and
`fallback_count`, while `next_video_id` remains null. Index zero is a synthetic
anchor for the current video. The consumer must request a `group_window` starting
at target one, verify channel IDs/names when blocks are active, and apply normal
blocked-occurrence traversal. The capability belongs to the selected profile;
video occurrences remain distinct. Empty groups return no capability.

The native phone adapter now exposes optional owner/selection-scoped asynchronous
lookup and cancellation callbacks for no-playlist EOF and confirmed terminal
playlist refill. It retains ended feedback, honors incoming controls and queue
extensions, rejects stale/invalid results, and bounds lookup to 30 seconds.
Repeated terminal replies cannot extend that deadline. The adapter/driver sanitizer
suite covers these paths; the adapter does not access the saved local queue.

The provider and application now supply the callbacks. A separate scoped job reads
remote next metadata, verifies the fallback group's anchor and finite bounds, and
scans verified identities/names across windows. Existing autoplay endpoints remain
authoritative; a blocked endpoint does not cause arbitrary suggestion substitution.
Receiver, playback, profile, source and block-policy changes retire stale results.
Host integration covers a receiver command through EOF and a new launch ticket,
with unchanged saved local records. The callback currently returns only a video ID;
returned playlist context and distinct same-ID occurrences still need propagation.
Real-service and console acceptance are deferred until the final candidate.
